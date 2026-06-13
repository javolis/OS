/* file.c - refcounted kernel file objects behind user file descriptors.
 * Refcounts are touched from syscall context (IF off) and from the kernel
 * shell's preemptible context, so they are guarded. */
#include <stddef.h>
#include <stdint.h>

#include "file.h"
#include "io.h"
#include "kheap.h"

static struct file console = {
    .kind = FILE_CONSOLE,
    .refs = 1, /* base reference, never dropped */
};

struct file *file_console(void) {
    return &console;
}

struct file *file_alloc(int kind) {
    struct file *f = kmalloc(sizeof(*f));
    if (!f)
        return NULL;
    f->kind = kind;
    f->refs = 1;
    f->data = NULL;
    f->size = 0;
    f->offset = 0;
    return f;
}

void file_ref(struct file *f) {
    uint32_t fl = irq_save();
    f->refs++;
    irq_restore(fl);
}

void file_unref(struct file *f) {
    uint32_t fl = irq_save();
    int gone = (--f->refs == 0);
    irq_restore(fl);
    if (gone)
        kfree(f);
}

void file_close_all(struct file **fds, int n) {
    for (int i = 0; i < n; i++) {
        if (fds[i]) {
            file_unref(fds[i]);
            fds[i] = NULL;
        }
    }
}
