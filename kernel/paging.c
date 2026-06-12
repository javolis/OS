/* paging.c — enable x86 paging with all physical RAM identity-mapped.
 *
 * Identity mapping keeps every existing pointer (kernel image, VGA buffer,
 * PMM bitmap, frames handed out by pmm_alloc_frame) valid after CR0.PG is
 * set, while making all memory access go through real page tables — the
 * stepping stone to non-identity kernel mappings and user address spaces. */
#include <stdint.h>

#include "kprintf.h"
#include "paging.h"
#include "pmm.h"

#define PAGE_PRESENT 0x1u
#define PAGE_WRITE 0x2u
#define ENTRIES 1024
#define FRAME_SIZE 4096u

static uint32_t *page_directory;

/* Grab a frame for paging structures and zero it (entries default to
 * not-present). Frames are identity-addressable while paging is off. */
static uint32_t *alloc_table(void) {
    uint32_t frame = pmm_alloc_frame();
    if (!frame) {
        kprintf("PANIC: out of physical frames while building page tables\n");
        for (;;)
            __asm__ volatile("cli; hlt");
    }
    uint32_t *table = (uint32_t *)frame;
    for (int i = 0; i < ENTRIES; i++)
        table[i] = 0;
    return table;
}

void paging_init(void) {
    page_directory = alloc_table();

    /* Identity-map every physical frame, building page tables on demand
     * (one table per 4 MiB of address space). */
    uint32_t frames = pmm_total_frames();
    for (uint32_t f = 0; f < frames; f++) {
        uint32_t addr = f * FRAME_SIZE;
        uint32_t pd_idx = addr >> 22;
        uint32_t pt_idx = (addr >> 12) & 0x3FF;

        if (!(page_directory[pd_idx] & PAGE_PRESENT)) {
            uint32_t *table = alloc_table();
            page_directory[pd_idx] =
                (uint32_t)table | PAGE_PRESENT | PAGE_WRITE;
        }
        uint32_t *table = (uint32_t *)(page_directory[pd_idx] & ~0xFFFu);
        table[pt_idx] = addr | PAGE_PRESENT | PAGE_WRITE;
    }

    /* Load the directory and turn paging on. */
    __asm__ volatile("mov %0, %%cr3" : : "r"(page_directory));
    uint32_t cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000u; /* CR0.PG */
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr0));
}

void paging_map(uint32_t virt, uint32_t phys) {
    uint32_t pd_idx = virt >> 22;
    uint32_t pt_idx = (virt >> 12) & 0x3FF;

    if (!(page_directory[pd_idx] & PAGE_PRESENT)) {
        uint32_t *table = alloc_table();
        page_directory[pd_idx] = (uint32_t)table | PAGE_PRESENT | PAGE_WRITE;
    }
    /* Page tables sit in identity-mapped RAM, so their physical address is
     * also their virtual address. */
    uint32_t *table = (uint32_t *)(page_directory[pd_idx] & ~0xFFFu);
    table[pt_idx] = (phys & ~0xFFFu) | PAGE_PRESENT | PAGE_WRITE;
    __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
}
