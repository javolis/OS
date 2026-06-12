/* kheap.h — kernel heap allocator. */
#pragma once
#include <stdint.h>

void kheap_init(void);
void *kmalloc(uint32_t size); /* NULL on exhaustion or size 0 */
void kfree(void *ptr);        /* NULL is a no-op */

void kheap_stats(uint32_t *used_out, uint32_t *total_out);
