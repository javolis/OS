/* paging.h — x86 paging (32-bit, 4 KiB pages). */
#pragma once
#include <stdint.h>

void paging_init(void);

/* Map the 4 KiB page at virtual address `virt` to physical frame `phys`
 * (kernel, read/write), allocating a page table if needed. */
void paging_map(uint32_t virt, uint32_t phys);
