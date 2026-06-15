/* ata.h — ATA (IDE) PIO disk on the primary bus, master drive. */
#pragma once
#include <stdint.h>

void ata_init(void);     /* probe the primary master; logs what it finds */
int ata_present(void);   /* 1 if a disk was found */
uint32_t ata_sectors(void); /* total 512-byte sectors (LBA28) */

/* Read/write `count` 512-byte sectors at LBA `lba` (PIO, polled).
 * Returns 0 on success, -1 on error or if no disk. count is 1..255. */
int ata_read(uint32_t lba, uint8_t count, void *buf);
int ata_write(uint32_t lba, uint8_t count, const void *buf);
