/* memlayout.h — kernel virtual memory layout.
 *
 *   0xC0000000 +-------------------------------+  KERNEL_VIRT_BASE
 *              | all physical RAM, offset-     |  phys P at virt BASE + P
 *              | mapped (kernel image included:|  (kernel linked at
 *              | linked at BASE + 1 MiB)       |   0xC0100000)
 *   0xE0000000 +-------------------------------+
 *              | kernel heap (grows on demand) |
 *              +-------------------------------+
 *
 * Everything below KERNEL_VIRT_BASE is unmapped in the kernel's tables —
 * reserved for future user address spaces, and NULL dereferences fault. */
#pragma once
#include <stdint.h>

#define KERNEL_VIRT_BASE 0xC0000000u

/* Kernel heap region (one 4 MiB page-directory slot — its page table is
 * preallocated at paging_init so process directories can share the kernel
 * half by copying PDEs once at creation; see paging_new_address_space). */
#define KHEAP_VIRT_BASE 0xE0000000u
#define KHEAP_VIRT_LIMIT 0xE0400000u

static inline void *phys_to_virt(uint32_t phys) {
    return (void *)(phys + KERNEL_VIRT_BASE);
}

static inline uint32_t virt_to_phys(const void *virt) {
    return (uint32_t)virt - KERNEL_VIRT_BASE;
}
