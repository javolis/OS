/* ramfs.c - a tiny writable in-RAM filesystem.
 *
 * A fixed table of named files whose contents live in kheap buffers that
 * grow on demand. All access is from syscall context (non-preemptible),
 * so the table needs no lock. Storage is the read/write counterpart to the
 * read-only initrd; the two namespaces are searched separately (ramfs
 * first) by SYS_OPEN. */
#include <stddef.h>
#include <stdint.h>

#include "kheap.h"
#include "kprintf.h"
#include "ramfs.h"

#define RAMFS_MAX_FILES 16

static struct ramfs_file files[RAMFS_MAX_FILES];

static int names_equal(const char *a, const char *b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

static void copy_name(char *dst, const char *src) {
    uint32_t i = 0;
    while (src[i] && i < RAMFS_NAME_MAX - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

struct ramfs_file *ramfs_find(const char *name) {
    for (int i = 0; i < RAMFS_MAX_FILES; i++)
        if (files[i].used && names_equal(files[i].name, name))
            return &files[i];
    return NULL;
}

struct ramfs_file *ramfs_create(const char *name) {
    uint32_t len = 0;
    while (name[len])
        len++;
    if (len == 0 || len >= RAMFS_NAME_MAX)
        return NULL;

    struct ramfs_file *f = ramfs_find(name);
    if (f) {
        f->size = 0; /* truncate; keep the existing buffer */
        return f;
    }
    for (int i = 0; i < RAMFS_MAX_FILES; i++) {
        if (!files[i].used) {
            files[i].used = 1;
            copy_name(files[i].name, name);
            files[i].data = NULL;
            files[i].size = 0;
            files[i].cap = 0;
            return &files[i];
        }
    }
    return NULL; /* table full */
}

/* Ensure the buffer holds at least `want` bytes, growing geometrically. */
static int ensure_cap(struct ramfs_file *f, uint32_t want) {
    if (want <= f->cap)
        return 0;
    uint32_t cap = f->cap ? f->cap : 64;
    while (cap < want)
        cap *= 2;
    uint8_t *buf = kmalloc(cap);
    if (!buf)
        return -1;
    for (uint32_t i = 0; i < f->size; i++)
        buf[i] = f->data[i];
    if (f->data)
        kfree(f->data);
    f->data = buf;
    f->cap = cap;
    return 0;
}

int ramfs_write(struct ramfs_file *f, uint32_t offset, const uint8_t *src,
                uint32_t n) {
    if (ensure_cap(f, offset + n) != 0)
        return -1;
    for (uint32_t i = 0; i < n; i++)
        f->data[offset + i] = src[i];
    if (offset + n > f->size)
        f->size = offset + n;
    return (int)n;
}

int ramfs_unlink(const char *name) {
    struct ramfs_file *f = ramfs_find(name);
    if (!f)
        return -1;
    if (f->data)
        kfree(f->data);
    f->data = NULL;
    f->size = 0;
    f->cap = 0;
    f->used = 0;
    return 0;
}

void ramfs_list(void) {
    for (int i = 0; i < RAMFS_MAX_FILES; i++)
        if (files[i].used)
            kprintf("%8lu  %s\n", files[i].size, files[i].name);
}

/* Fill *name_out/*size_out for the idx-th used file (0-based). Returns 1
 * if present, 0 if idx is past the last used entry. */
int ramfs_entry(uint32_t idx, const char **name_out, uint32_t *size_out) {
    uint32_t i = 0;
    for (int s = 0; s < RAMFS_MAX_FILES; s++) {
        if (!files[s].used)
            continue;
        if (i == idx) {
            *name_out = files[s].name;
            *size_out = files[s].size;
            return 1;
        }
        i++;
    }
    return 0;
}
