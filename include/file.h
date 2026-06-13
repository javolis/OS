/* file.h - refcounted kernel file objects behind user file descriptors. */
#pragma once
#include <stdint.h>

#define MAX_FDS 8

enum file_kind {
    FILE_CONSOLE = 1, /* keyboard in / screen+serial out */
    FILE_INITRD,      /* read-only view of an initrd file */
    FILE_PIPE_READ,   /* read end of a pipe */
    FILE_PIPE_WRITE,  /* write end of a pipe */
};

struct pipe;

struct file {
    int kind;
    int refs;
    const uint8_t *data; /* FILE_INITRD */
    uint32_t size;
    uint32_t offset;
    struct pipe *pipe; /* FILE_PIPE_* */
};

/* The shared console object (fds 0/1 of every process). Never freed: its
 * base reference is retained forever. */
struct file *file_console(void);

struct file *file_alloc(int kind); /* refs = 1; NULL if out of memory */
void file_ref(struct file *f);
void file_unref(struct file *f); /* frees at refcount 0 */

/* Unref every non-NULL entry and clear the array (task teardown). */
void file_close_all(struct file **fds, int n);
