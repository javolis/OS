/* fat.c — minimal read-only FAT16 driver over the ATA disk.
 *
 * Parses the boot-sector BPB to locate the FAT, the fixed root directory and
 * the data region, then reads files by walking their 16-bit cluster chain.
 * Enough to list the root directory and load files (e.g. apps the user copied
 * onto the disk from their host). Long file names are ignored; 8.3 only. */
#include <stddef.h>
#include <stdint.h>

#include "ata.h"
#include "fat.h"
#include "kprintf.h"

static struct {
    uint32_t fat_start;  /* first FAT sector */
    uint32_t root_start; /* first root-dir sector */
    uint32_t data_start; /* first data sector (cluster 2) */
    uint32_t spc;        /* sectors per cluster */
    uint32_t spf;        /* sectors per FAT */
    uint32_t num_fats;   /* number of FAT copies */
    uint32_t root_ents;  /* root directory entries */
    int mounted;
} fs;

static uint8_t secbuf[512];

static uint16_t rd16(const uint8_t *p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

int fat_mount(void) {
    fs.mounted = 0;
    if (!ata_present() || ata_read(0, 1, secbuf) != 0)
        return -1;
    if (secbuf[510] != 0x55 || secbuf[511] != 0xAA)
        return -1;
    uint32_t bps = rd16(secbuf + 11);
    uint32_t spc = secbuf[13];
    uint32_t reserved = rd16(secbuf + 14);
    uint32_t num_fats = secbuf[16];
    uint32_t root_ents = rd16(secbuf + 17);
    uint32_t spf = rd16(secbuf + 22);
    if (bps != 512 || spc == 0 || spf == 0 || root_ents == 0)
        return -1; /* not FAT12/16 (FAT32 zeroes spf16 + root_ents) */

    fs.spc = spc;
    fs.spf = spf;
    fs.num_fats = num_fats;
    fs.root_ents = root_ents;
    fs.fat_start = reserved;
    fs.root_start = reserved + num_fats * spf;
    fs.data_start = fs.root_start + (root_ents * 32 + 511) / 512;
    fs.mounted = 1;
    kprintf("fat: FAT16 mounted (spc %lu, root %lu entries)\n",
            (unsigned long)spc, (unsigned long)root_ents);
    return 0;
}

int fat_mounted(void) {
    return fs.mounted;
}

/* Convert an on-disk 11-byte 8.3 field to a dotted, NUL-terminated name. */
static void name_to_dotted(const uint8_t *raw, char *out) {
    int o = 0;
    for (int i = 0; i < 8 && raw[i] != ' '; i++)
        out[o++] = (char)raw[i];
    if (raw[8] != ' ') {
        out[o++] = '.';
        for (int i = 8; i < 11 && raw[i] != ' '; i++)
            out[o++] = (char)raw[i];
    }
    out[o] = '\0';
}

/* Convert a dotted user name to the padded 11-byte 8.3 form (uppercased). */
static void name_to_83(const char *in, uint8_t *out) {
    for (int i = 0; i < 11; i++)
        out[i] = ' ';
    int i = 0, o = 0;
    while (in[i] && in[i] != '.' && o < 8) {
        char c = in[i++];
        out[o++] = (c >= 'a' && c <= 'z') ? (uint8_t)(c - 32) : (uint8_t)c;
    }
    while (in[i] && in[i] != '.')
        i++;
    if (in[i] == '.') {
        i++;
        o = 8;
        while (in[i] && o < 11) {
            char c = in[i++];
            out[o++] = (c >= 'a' && c <= 'z') ? (uint8_t)(c - 32) : (uint8_t)c;
        }
    }
}

/* Find a root-dir entry: by index (want==NULL) or by 8.3 name. Fills
 * *first_cluster, *size, and dotted name. Returns 0, or -1 if not found. */
static int find_entry(int index, const uint8_t *want, char *dotted,
                      uint32_t *first, uint32_t *size) {
    uint32_t entries_per_sec = 512 / 32;
    uint32_t root_secs = (fs.root_ents * 32 + 511) / 512;
    int seen = 0;
    for (uint32_t s = 0; s < root_secs; s++) {
        if (ata_read(fs.root_start + s, 1, secbuf) != 0)
            return -1;
        for (uint32_t e = 0; e < entries_per_sec; e++) {
            const uint8_t *d = secbuf + e * 32;
            if (d[0] == 0x00)
                return -1; /* end of directory */
            if (d[0] == 0xE5)
                continue; /* deleted */
            if ((d[11] & 0x0F) == 0x0F)
                continue; /* long-file-name fragment */
            if (d[11] & 0x08)
                continue; /* volume label */
            if (d[11] & 0x10)
                continue; /* subdirectory */
            if (want) {
                int match = 1;
                for (int i = 0; i < 11; i++)
                    if (d[i] != want[i]) {
                        match = 0;
                        break;
                    }
                if (!match)
                    continue;
            } else if (seen++ != index) {
                continue;
            }
            if (dotted)
                name_to_dotted(d, dotted);
            *first = rd16(d + 26);
            *size = (uint32_t)d[28] | ((uint32_t)d[29] << 8) |
                    ((uint32_t)d[30] << 16) | ((uint32_t)d[31] << 24);
            return 0;
        }
    }
    return -1;
}

int fat_readdir(int index, char *name, uint32_t *size) {
    if (!fs.mounted)
        return -1;
    uint32_t first;
    return find_entry(index, NULL, name, &first, size);
}

static uint16_t fat_entry(uint32_t cluster) {
    uint32_t off = cluster * 2;
    if (ata_read(fs.fat_start + off / 512, 1, secbuf) != 0)
        return 0xFFFF;
    return rd16(secbuf + (off % 512));
}

int fat_read_file(const char *name, void *buf, uint32_t maxlen) {
    if (!fs.mounted)
        return -1;
    uint8_t want[11];
    name_to_83(name, want);
    uint32_t first, size;
    if (find_entry(-1, want, NULL, &first, &size) != 0)
        return -1;
    if (size > maxlen)
        size = maxlen;

    uint8_t *out = (uint8_t *)buf;
    uint32_t got = 0, cluster = first;
    while (got < size && cluster >= 2 && cluster < 0xFFF8) {
        uint32_t lba = fs.data_start + (cluster - 2) * fs.spc;
        for (uint32_t s = 0; s < fs.spc && got < size; s++) {
            uint8_t tmp[512];
            if (ata_read(lba + s, 1, tmp) != 0)
                return (int)got;
            uint32_t n = size - got;
            if (n > 512)
                n = 512;
            for (uint32_t i = 0; i < n; i++)
                out[got + i] = tmp[i];
            got += n;
        }
        cluster = fat_entry(cluster);
    }
    return (int)got;
}

int fat_size(const char *name) {
    if (!fs.mounted)
        return -1;
    uint8_t want[11];
    name_to_83(name, want);
    uint32_t first, size;
    if (find_entry(-1, want, NULL, &first, &size) != 0)
        return -1;
    return (int)size;
}

/* --- write support --- */
#define MAX_WR_CLUSTERS 256 /* up to 512 KiB at 2 KiB clusters */

/* Write a 16-bit FAT entry to every FAT copy. */
static void set_fat_entry(uint32_t cluster, uint16_t val) {
    uint32_t off = cluster * 2;
    for (uint32_t f = 0; f < fs.num_fats; f++) {
        uint32_t sec = fs.fat_start + f * fs.spf + off / 512;
        uint8_t b[512];
        if (ata_read(sec, 1, b) != 0)
            return;
        b[off % 512] = (uint8_t)(val & 0xFF);
        b[off % 512 + 1] = (uint8_t)((val >> 8) & 0xFF);
        ata_write(sec, 1, b);
    }
}

int fat_write_file(const char *name, const void *buf, uint32_t len) {
    if (!fs.mounted)
        return -1;
    uint32_t clustersz = fs.spc * 512;
    uint32_t ncl = (len + clustersz - 1) / clustersz; /* 0 if len==0 */
    if (ncl > MAX_WR_CLUSTERS)
        return -1;

    uint8_t want[11];
    name_to_83(name, want);
    const uint8_t *data = (const uint8_t *)buf;

    /* Locate the dir entry to (re)use: an existing same-named file (whose old
     * chain we free), else the first deleted/empty slot. */
    uint32_t entries_per_sec = 512 / 32;
    uint32_t root_secs = (fs.root_ents * 32 + 511) / 512;
    uint32_t dir_sec = 0, dir_off = 0;
    int have_slot = 0;
    uint8_t b[512];
    for (uint32_t s = 0; s < root_secs && !have_slot; s++) {
        if (ata_read(fs.root_start + s, 1, b) != 0)
            return -1;
        for (uint32_t e = 0; e < entries_per_sec; e++) {
            uint8_t *d = b + e * 32;
            if (d[0] == 0x00) {
                dir_sec = fs.root_start + s;
                dir_off = e * 32;
                have_slot = 1;
                break;
            }
            if (d[0] == 0xE5) {
                if (!dir_sec) {
                    dir_sec = fs.root_start + s;
                    dir_off = e * 32;
                }
                continue;
            }
            if ((d[11] & 0x0F) == 0x0F || (d[11] & 0x08))
                continue;
            int match = 1;
            for (int i = 0; i < 11; i++)
                if (d[i] != want[i]) {
                    match = 0;
                    break;
                }
            if (match) {
                uint32_t c = rd16(d + 26); /* free the old cluster chain */
                while (c >= 2 && c < 0xFFF8) {
                    uint16_t nx = fat_entry(c);
                    set_fat_entry(c, 0);
                    c = nx;
                }
                dir_sec = fs.root_start + s;
                dir_off = e * 32;
                have_slot = 1;
                break;
            }
        }
    }
    if (!have_slot && dir_sec) /* used a recorded deleted slot */
        have_slot = 1;
    if (!have_slot)
        return -1; /* root directory full */

    /* Allocate `ncl` free clusters. */
    static uint32_t clusters[MAX_WR_CLUSTERS]; /* keep off the kernel stack */
    uint32_t max_cluster = 2 + (ata_sectors() - fs.data_start) / fs.spc;
    uint32_t got = 0;
    for (uint32_t c = 2; c < max_cluster && got < ncl; c++)
        if (fat_entry(c) == 0x0000)
            clusters[got++] = c;
    if (got < ncl)
        return -1; /* not enough free space */

    /* Write the data into the clusters (zero-padding the final sector). */
    for (uint32_t i = 0; i < ncl; i++) {
        uint32_t lba = fs.data_start + (clusters[i] - 2) * fs.spc;
        for (uint32_t s = 0; s < fs.spc; s++) {
            uint8_t sec[512];
            uint32_t base = (i * fs.spc + s) * 512;
            for (uint32_t k = 0; k < 512; k++)
                sec[k] = (base + k < len) ? data[base + k] : 0;
            ata_write(lba + s, 1, sec);
        }
    }

    /* Chain the clusters in the FAT. */
    for (uint32_t i = 0; i < ncl; i++)
        set_fat_entry(clusters[i],
                      (i + 1 < ncl) ? (uint16_t)clusters[i + 1] : 0xFFFF);

    /* Write the directory entry. */
    if (ata_read(dir_sec, 1, b) != 0)
        return -1;
    uint8_t *d = b + dir_off;
    for (int i = 0; i < 11; i++)
        d[i] = want[i];
    d[11] = 0x20; /* archive */
    for (int i = 12; i < 26; i++)
        d[i] = 0;
    uint16_t fc = ncl ? (uint16_t)clusters[0] : 0;
    d[26] = (uint8_t)(fc & 0xFF);
    d[27] = (uint8_t)((fc >> 8) & 0xFF);
    d[28] = (uint8_t)(len & 0xFF);
    d[29] = (uint8_t)((len >> 8) & 0xFF);
    d[30] = (uint8_t)((len >> 16) & 0xFF);
    d[31] = (uint8_t)((len >> 24) & 0xFF);
    ata_write(dir_sec, 1, b);
    return (int)len;
}
