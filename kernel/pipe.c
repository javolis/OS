/* pipe.c - in-kernel byte pipe with blocking read/write.
 *
 * A pipe is a fixed ring buffer with two end file objects (read, write).
 * The buffer is shared kernel memory; pipe ops only run in syscall context,
 * which is non-preemptible (IF off), so the buffer needs no lock - blocking
 * happens only at explicit yield points (sched_block_on_chan), and the
 * waker re-runs the check after being scheduled back. readers/writers count
 * the open ends so the other side sees EOF (writers==0) or a broken pipe
 * (readers==0). */
#include <stddef.h>
#include <stdint.h>

#include "file.h"
#include "kheap.h"
#include "pipe.h"
#include "sched.h"

#define PIPE_SIZE 512u

struct pipe {
    uint8_t buf[PIPE_SIZE];
    uint32_t head; /* next write index */
    uint32_t tail; /* next read index */
    uint32_t count;
    int readers;
    int writers;
};

int pipe_create(struct file **rd_out, struct file **wr_out) {
    struct pipe *p = kmalloc(sizeof(*p));
    struct file *rd = file_alloc(FILE_PIPE_READ);
    struct file *wr = file_alloc(FILE_PIPE_WRITE);
    if (!p || !rd || !wr) {
        if (p)
            kfree(p);
        if (rd)
            file_unref(rd);
        if (wr)
            file_unref(wr);
        return -1;
    }
    p->head = p->tail = p->count = 0;
    p->readers = 1;
    p->writers = 1;
    rd->pipe = p;
    wr->pipe = p;
    *rd_out = rd;
    *wr_out = wr;
    return 0;
}

int pipe_read(struct file *f, uint8_t *dst, uint32_t n) {
    struct pipe *p = f->pipe;

    while (p->count == 0) {
        if (p->writers == 0)
            return 0; /* EOF: no data and no one left to write */
        sched_block_on_chan(p);
    }

    uint32_t take = n < p->count ? n : p->count;
    for (uint32_t i = 0; i < take; i++) {
        dst[i] = p->buf[p->tail];
        p->tail = (p->tail + 1) % PIPE_SIZE;
    }
    p->count -= take;
    sched_wake_chan(p); /* a writer may have been waiting for space */
    return (int)take;
}

int pipe_write(struct file *f, const uint8_t *src, uint32_t n) {
    struct pipe *p = f->pipe;
    uint32_t written = 0;

    while (written < n) {
        if (p->readers == 0)
            return written ? (int)written : -1; /* broken pipe */
        if (p->count == PIPE_SIZE) {
            sched_block_on_chan(p);
            continue;
        }
        while (written < n && p->count < PIPE_SIZE) {
            p->buf[p->head] = src[written++];
            p->head = (p->head + 1) % PIPE_SIZE;
            p->count++;
        }
        sched_wake_chan(p); /* a reader may have been waiting for data */
    }
    return (int)written;
}

void pipe_close_end(struct file *f) {
    struct pipe *p = f->pipe;
    if (!p)
        return;

    if (f->kind == FILE_PIPE_READ)
        p->readers--;
    else
        p->writers--;

    /* Wake the other side so it observes EOF / broken pipe instead of
     * blocking forever. */
    sched_wake_chan(p);

    if (p->readers == 0 && p->writers == 0)
        kfree(p);
}
