/* paging.h — x86 paging (32-bit, 4 KiB pages) and address spaces. */
#pragma once
#include <stdint.h>

void paging_init(void);

/* Map the 4 KiB page at virtual address `virt` to physical frame `phys`
 * (kernel, read/write) in the kernel address space, allocating a page
 * table if needed. */
void paging_map(uint32_t virt, uint32_t phys);

/* Map a ring-3-accessible page into a specific address space. */
void paging_map_user_in(uint32_t dir_phys, uint32_t virt, uint32_t phys);

/* Create a new address space sharing the kernel half (PDEs 768-1023) with
 * the kernel directory; the user half starts empty. Returns the directory's
 * physical address. */
uint32_t paging_new_address_space(void);

/* Free a (non-active) address space: every user-half mapped frame, its
 * page tables, and the directory itself. */
void paging_destroy_address_space(uint32_t dir_phys);

void paging_switch(uint32_t dir_phys);
uint32_t paging_kernel_directory(void);
