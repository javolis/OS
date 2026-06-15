/* fat.h — read-only FAT16 filesystem on the ATA primary master. */
#pragma once
#include <stdint.h>

int fat_mount(void);   /* parse the boot sector's BPB; 0 if a FAT16 vol, -1 */
int fat_mounted(void); /* 1 once mounted */

/* Enumerate root-directory files (not dirs/volume labels). Fills `name`
 * (dotted 8.3, NUL-terminated, needs 13 bytes) and *size. Returns 0, or -1
 * past the last entry. */
int fat_readdir(int index, char *name, uint32_t *size);

/* Read up to maxlen bytes of a root file (name is dotted 8.3, case-
 * insensitive) into buf. Returns the byte count, or -1 if not found. */
int fat_read_file(const char *name, void *buf, uint32_t maxlen);
