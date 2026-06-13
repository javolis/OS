/* initrd.c — read-only initial ramdisk: a USTAR tar archive that GRUB
 * loads as the first Multiboot module. The PMM reserves its frames, so
 * the archive can simply be read in place through the higher-half window. */
#include <stddef.h>
#include <stdint.h>

#include "initrd.h"
#include "kprintf.h"
#include "memlayout.h"
#include "multiboot.h"

/* USTAR header: one 512-byte block; file data follows, padded to 512. */
struct tar_header {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12]; /* octal, NUL/space terminated */
    char mtime[12];
    char chksum[8];
    char typeflag; /* '0' or NUL = regular file */
    char linkname[100];
    char magic[6]; /* "ustar" */
    char version[2];
    char pad[247];
} __attribute__((packed));

#define TAR_BLOCK 512u

static const uint8_t *initrd_base;
static uint32_t initrd_size;

static uint32_t parse_octal(const char *s, uint32_t n) {
    uint32_t v = 0;
    for (uint32_t i = 0; i < n && s[i] >= '0' && s[i] <= '7'; i++)
        v = v * 8 + (uint32_t)(s[i] - '0');
    return v;
}

static int names_equal(const char *a, const char *b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

static int is_regular_file(const struct tar_header *h) {
    return h->typeflag == '0' || h->typeflag == '\0';
}

void initrd_init(const struct multiboot_info *mbi) {
    if (!(mbi->flags & MULTIBOOT_INFO_MODS) || mbi->mods_count == 0) {
        kprintf("initrd: no boot module found\n");
        return;
    }
    const struct multiboot_mod *mod = phys_to_virt(mbi->mods_addr);
    initrd_base = phys_to_virt(mod->mod_start);
    initrd_size = mod->mod_end - mod->mod_start;

    uint32_t files = 0;
    uint32_t off = 0;
    while (off + TAR_BLOCK <= initrd_size) {
        const struct tar_header *h =
            (const struct tar_header *)(initrd_base + off);
        if (h->name[0] == '\0')
            break;
        if (is_regular_file(h))
            files++;
        uint32_t fsize = parse_octal(h->size, sizeof(h->size));
        off += TAR_BLOCK + ((fsize + TAR_BLOCK - 1) & ~(TAR_BLOCK - 1));
    }
    kprintf("initrd: %lu files, %lu bytes.\n", files, initrd_size);
}

int initrd_present(void) {
    return initrd_base != NULL;
}

const void *initrd_find(const char *name, uint32_t *size_out) {
    if (!initrd_base)
        return NULL;
    uint32_t off = 0;
    while (off + TAR_BLOCK <= initrd_size) {
        const struct tar_header *h =
            (const struct tar_header *)(initrd_base + off);
        if (h->name[0] == '\0')
            break;
        uint32_t fsize = parse_octal(h->size, sizeof(h->size));
        if (is_regular_file(h) && names_equal(h->name, name)) {
            *size_out = fsize;
            return initrd_base + off + TAR_BLOCK;
        }
        off += TAR_BLOCK + ((fsize + TAR_BLOCK - 1) & ~(TAR_BLOCK - 1));
    }
    return NULL;
}

void initrd_list(void) {
    if (!initrd_base) {
        kprintf("no initrd\n");
        return;
    }
    uint32_t off = 0;
    while (off + TAR_BLOCK <= initrd_size) {
        const struct tar_header *h =
            (const struct tar_header *)(initrd_base + off);
        if (h->name[0] == '\0')
            break;
        uint32_t fsize = parse_octal(h->size, sizeof(h->size));
        if (is_regular_file(h))
            kprintf("%8lu  %s\n", fsize, h->name);
        off += TAR_BLOCK + ((fsize + TAR_BLOCK - 1) & ~(TAR_BLOCK - 1));
    }
}

/* Fill *name_out/*size_out for the idx-th regular file (0-based). Returns
 * 1 if present, 0 if idx is past the end. */
int initrd_entry(uint32_t idx, const char **name_out, uint32_t *size_out) {
    if (!initrd_base)
        return 0;
    uint32_t off = 0, i = 0;
    while (off + TAR_BLOCK <= initrd_size) {
        const struct tar_header *h =
            (const struct tar_header *)(initrd_base + off);
        if (h->name[0] == '\0')
            break;
        uint32_t fsize = parse_octal(h->size, sizeof(h->size));
        if (is_regular_file(h)) {
            if (i == idx) {
                *name_out = h->name;
                *size_out = fsize;
                return 1;
            }
            i++;
        }
        off += TAR_BLOCK + ((fsize + TAR_BLOCK - 1) & ~(TAR_BLOCK - 1));
    }
    return 0;
}
