/* kheap.c — kernel heap: a first-fit free list with block split and
 * coalesce, living in a dedicated virtual region that grows page-by-page
 * via pmm_alloc_frame + paging_map.
 *
 * The region starts far above identity-mapped physical RAM, so backing
 * frames need not be physically contiguous — virtual contiguity comes from
 * the page tables. Blocks are kept in address order with no gaps, which
 * makes any pair of consecutive free blocks mergeable. */
#include <stddef.h>
#include <stdint.h>

#include "kheap.h"
#include "kprintf.h"
#include "paging.h"
#include "pmm.h"

/* Above the 512 MiB offset-mapped RAM window that starts at 0xC0000000. */
#define KHEAP_START 0xE0000000u
#define KHEAP_MAX 0xE0400000u /* 4 MiB cap, easy to raise later */
#define PAGE_SIZE 4096u

struct block {
    uint32_t size; /* payload bytes (excludes this header) */
    uint32_t free;
    struct block *next; /* next block by address */
};

#define HDR_SIZE ((uint32_t)sizeof(struct block))
#define MIN_SPLIT_REMAINDER (HDR_SIZE + 8)

static struct block *head;
static uint32_t heap_end; /* one past the highest mapped heap byte */

/* Map at least min_bytes of new heap space and append it to the free list,
 * coalescing with a free tail block. Returns 0 on exhaustion. (On a partial
 * failure mid-grow, already-mapped pages are simply not handed out — a
 * harmless leak on a path where the system is out of memory anyway.) */
static int grow(uint32_t min_bytes) {
    uint32_t bytes = (min_bytes + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    if (heap_end + bytes > KHEAP_MAX || heap_end + bytes < heap_end)
        return 0;

    for (uint32_t off = 0; off < bytes; off += PAGE_SIZE) {
        uint32_t frame = pmm_alloc_frame();
        if (!frame)
            return 0;
        paging_map(heap_end + off, frame);
    }

    struct block *nb = (struct block *)heap_end;
    nb->size = bytes - HDR_SIZE;
    nb->free = 1;
    nb->next = NULL;
    heap_end += bytes;

    if (!head) {
        head = nb;
        return 1;
    }
    struct block *tail = head;
    while (tail->next)
        tail = tail->next;
    tail->next = nb;
    if (tail->free) { /* adjacent by construction: merge */
        tail->size += HDR_SIZE + nb->size;
        tail->next = NULL;
    }
    return 1;
}

void kheap_init(void) {
    head = NULL;
    heap_end = KHEAP_START;
    if (!grow(PAGE_SIZE)) {
        kprintf("PANIC: cannot map the initial kernel heap page\n");
        for (;;)
            __asm__ volatile("cli; hlt");
    }
}

void *kmalloc(uint32_t size) {
    if (size == 0)
        return NULL;
    size = (size + 3u) & ~3u; /* keep payloads 4-byte aligned */

    for (;;) {
        for (struct block *b = head; b; b = b->next) {
            if (!b->free || b->size < size)
                continue;
            if (b->size >= size + MIN_SPLIT_REMAINDER) {
                struct block *nb =
                    (struct block *)((uint8_t *)b + HDR_SIZE + size);
                nb->size = b->size - size - HDR_SIZE;
                nb->free = 1;
                nb->next = b->next;
                b->size = size;
                b->next = nb;
            }
            b->free = 0;
            return (uint8_t *)b + HDR_SIZE;
        }
        if (!grow(size + HDR_SIZE))
            return NULL;
    }
}

void kfree(void *ptr) {
    if (!ptr)
        return;
    struct block *b = (struct block *)((uint8_t *)ptr - HDR_SIZE);
    b->free = 1;

    /* The list is address-ordered and gap-free, so consecutive free blocks
     * are always adjacent — merge every such run. */
    for (struct block *cur = head; cur; cur = cur->next) {
        while (cur->free && cur->next && cur->next->free) {
            cur->size += HDR_SIZE + cur->next->size;
            cur->next = cur->next->next;
        }
    }
}

void kheap_stats(uint32_t *used_out, uint32_t *total_out) {
    uint32_t used = 0;
    for (struct block *b = head; b; b = b->next)
        if (!b->free)
            used += b->size;
    *used_out = used;
    *total_out = heap_end - KHEAP_START;
}
