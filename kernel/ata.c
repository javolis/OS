/* ata.c — ATA (IDE) PIO driver for the primary bus master drive.
 *
 * Polled 28-bit LBA reads/writes over the legacy I/O ports (0x1F0..0x1F7 +
 * control 0x3F6). No DMA, no IRQ — simple and enough for a persistent store.
 * QEMU exposes a `-drive ... if=ide,index=0` image here. */
#include <stdint.h>

#include "ata.h"
#include "io.h"
#include "kprintf.h"

#define IO 0x1F0
#define CTRL 0x3F6

#define REG_DATA 0
#define REG_FEATURES 1
#define REG_SECCOUNT 2
#define REG_LBA0 3
#define REG_LBA1 4
#define REG_LBA2 5
#define REG_DRIVE 6
#define REG_STATUS 7
#define REG_CMD 7

#define SR_BSY 0x80
#define SR_DRDY 0x40
#define SR_DRQ 0x08
#define SR_ERR 0x01

#define CMD_READ 0x20
#define CMD_WRITE 0x30
#define CMD_FLUSH 0xE7
#define CMD_IDENTIFY 0xEC

static int present;
static uint32_t total_sectors;

static void delay400(void) {
    for (int i = 0; i < 4; i++)
        (void)inb(CTRL); /* each status read ~100ns */
}

/* Wait for BSY to clear and DRQ to set; -1 on error/timeout. */
static int wait_drq(void) {
    delay400();
    for (int guard = 0; guard < 2000000; guard++) {
        uint8_t s = inb(IO + REG_STATUS);
        if (s & SR_ERR)
            return -1;
        if (!(s & SR_BSY) && (s & SR_DRQ))
            return 0;
    }
    return -1;
}

static int wait_ready(void) {
    delay400();
    for (int guard = 0; guard < 2000000; guard++) {
        uint8_t s = inb(IO + REG_STATUS);
        if (s & SR_ERR)
            return -1;
        if (!(s & SR_BSY) && (s & SR_DRDY))
            return 0;
    }
    return -1;
}

static void setup_lba(uint32_t lba, uint8_t count) {
    outb(IO + REG_DRIVE, 0xE0 | ((lba >> 24) & 0x0F)); /* master, LBA mode */
    outb(IO + REG_FEATURES, 0);
    outb(IO + REG_SECCOUNT, count);
    outb(IO + REG_LBA0, (uint8_t)(lba & 0xFF));
    outb(IO + REG_LBA1, (uint8_t)((lba >> 8) & 0xFF));
    outb(IO + REG_LBA2, (uint8_t)((lba >> 16) & 0xFF));
}

int ata_read(uint32_t lba, uint8_t count, void *buf) {
    if (!present || count == 0)
        return -1;
    setup_lba(lba, count);
    outb(IO + REG_CMD, CMD_READ);
    uint16_t *b = (uint16_t *)buf;
    for (int s = 0; s < count; s++) {
        if (wait_drq() != 0)
            return -1;
        for (int i = 0; i < 256; i++)
            b[s * 256 + i] = inw(IO + REG_DATA);
    }
    return 0;
}

int ata_write(uint32_t lba, uint8_t count, const void *buf) {
    if (!present || count == 0)
        return -1;
    setup_lba(lba, count);
    outb(IO + REG_CMD, CMD_WRITE);
    const uint16_t *b = (const uint16_t *)buf;
    for (int s = 0; s < count; s++) {
        if (wait_drq() != 0)
            return -1;
        for (int i = 0; i < 256; i++)
            outw(IO + REG_DATA, b[s * 256 + i]);
    }
    outb(IO + REG_CMD, CMD_FLUSH);
    wait_ready();
    return 0;
}

void ata_init(void) {
    /* Select the master and issue IDENTIFY. */
    outb(IO + REG_DRIVE, 0xA0);
    outb(IO + REG_SECCOUNT, 0);
    outb(IO + REG_LBA0, 0);
    outb(IO + REG_LBA1, 0);
    outb(IO + REG_LBA2, 0);
    outb(IO + REG_CMD, CMD_IDENTIFY);

    uint8_t st = inb(IO + REG_STATUS);
    if (st == 0 || st == 0xFF) {
        kprintf("ata: no disk on primary master\n");
        return;
    }
    int guard = 2000000;
    while ((inb(IO + REG_STATUS) & SR_BSY) && guard-- > 0) {
    }
    /* Non-zero LBA mid/hi after IDENTIFY means it's not a plain ATA disk. */
    if (inb(IO + REG_LBA1) != 0 || inb(IO + REG_LBA2) != 0) {
        kprintf("ata: primary master is not an ATA disk\n");
        return;
    }
    guard = 2000000;
    uint8_t s;
    do {
        s = inb(IO + REG_STATUS);
        if (s & SR_ERR) {
            kprintf("ata: IDENTIFY error\n");
            return;
        }
    } while (!(s & SR_DRQ) && guard-- > 0);
    if (!(s & SR_DRQ)) {
        kprintf("ata: IDENTIFY timed out\n");
        return;
    }

    uint16_t id[256];
    for (int i = 0; i < 256; i++)
        id[i] = inw(IO + REG_DATA);
    total_sectors = (uint32_t)id[60] | ((uint32_t)id[61] << 16);
    present = 1;
    kprintf("ata: disk present, %lu sectors (%lu MiB)\n",
            (unsigned long)total_sectors, (unsigned long)(total_sectors / 2048));

    /* Sanity-check a read: the boot sector should carry the 0x55AA signature. */
    static uint8_t sec[512];
    if (ata_read(0, 1, sec) == 0 && sec[510] == 0x55 && sec[511] == 0xAA)
        kprintf("ata: boot sector ok (0x55AA)\n");
    else
        kprintf("ata: boot sector unreadable\n");
}

int ata_present(void) {
    return present;
}

uint32_t ata_sectors(void) {
    return total_sectors;
}
