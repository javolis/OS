/* ramfs.h - a tiny writable in-RAM filesystem (files live in the kheap).
 * Distinct from the read-only initrd; survives for the life of the boot. */
#pragma once
#include <stdint.h>

#define RAMFS_NAME_MAX 32

struct ramfs_file {
    char name[RAMFS_NAME_MAX];
    uint8_t *data; /* kheap buffer, NULL when empty */
    uint32_t size;
    uint32_t cap;
    int used;
};

/* Find an existing file, or NULL. */
struct ramfs_file *ramfs_find(const char *name);

/* Create (or truncate to empty if it exists) a file; NULL if the table is
 * full or the name is too long. */
struct ramfs_file *ramfs_create(const char *name);

/* Overwrite-and-extend: write n bytes at offset, growing storage as
 * needed. Returns n, or -1 on allocation failure. */
int ramfs_write(struct ramfs_file *f, uint32_t offset, const uint8_t *src,
                uint32_t n);

/* Remove a file by name, freeing its storage. 0 on success, -1 if absent. */
int ramfs_unlink(const char *name);

/* Print "<size>  <name>" for each file via kprintf. */
void ramfs_list(void);
