/* pmm.h — physical memory manager: 4 KiB frame allocator. */
#pragma once
#include <stdint.h>

struct multiboot_info;

void pmm_init(const struct multiboot_info *mbi);

/* Returns the physical address of a free frame, or 0 when out of memory
 * (frame 0 sits in reserved low memory, so 0 is never a valid result). */
uint32_t pmm_alloc_frame(void);
void pmm_free_frame(uint32_t addr);

uint32_t pmm_total_frames(void);
uint32_t pmm_free_frames(void);
