/* pipe.h - in-kernel byte pipe behind a pair of file descriptors. */
#pragma once
#include <stdint.h>

struct file;

/* Create a pipe and its two end file objects (each with refs = 1).
 * Returns 0 and sets the read/write outputs, or -1 on allocation failure. */
int pipe_create(struct file **rd, struct file **wr);

/* Blocking byte transfer. read returns bytes read, 0 at EOF (all writers
 * closed); write returns bytes written, -1 if all readers closed first. */
int pipe_read(struct file *f, uint8_t *dst, uint32_t n);
int pipe_write(struct file *f, const uint8_t *src, uint32_t n);

/* Called by file_unref when a pipe-end file object reaches refcount 0. */
void pipe_close_end(struct file *f);
