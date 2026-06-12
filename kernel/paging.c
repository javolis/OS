/* paging.c — final page tables for the higher-half kernel.
 *
 * boot.s enters the higher half on a temporary 4 MiB-page directory that
 * maps only the first 4 MiB of RAM (at 0 and at KERNEL_VIRT_BASE).
 * paging_init builds the real directory with 4 KiB pages — physical frame P
 * appears at KERNEL_VIRT_BASE + P — and drops the identity mapping, so from
 * then on NULL dereferences page-fault and everything below the kernel base
 * is free for future user address spaces. */
#include <stdint.h>

#include "kprintf.h"
#include "memlayout.h"
#include "paging.h"
#include "pmm.h"

#define PAGE_PRESENT 0x1u
#define PAGE_WRITE 0x2u
#define PAGE_USER 0x4u
#define ENTRIES 1024
#define FRAME_SIZE 4096u

/* The offset-mapped window ends where the heap region begins (0xE0000000),
 * capping mappable RAM at 512 MiB — far above what QEMU gives us. */
#define MAX_OFFSET_MAPPED_FRAMES ((0xE0000000u - KERNEL_VIRT_BASE) / FRAME_SIZE)

static uint32_t page_directory_phys;

/* Grab a frame for paging structures and zero it through the higher-half
 * window. Returns the frame's physical address (what PDEs want). */
static uint32_t alloc_table_phys(void) {
    uint32_t frame = pmm_alloc_frame();
    if (!frame) {
        kprintf("PANIC: out of physical frames while building page tables\n");
        for (;;)
            __asm__ volatile("cli; hlt");
    }
    uint32_t *table = phys_to_virt(frame);
    for (int i = 0; i < ENTRIES; i++)
        table[i] = 0;
    return frame;
}

void paging_init(void) {
    /* While this rebuild runs, only the first 4 MiB are mapped (boot.s),
     * so every frame touched here must lie below 4 MiB physical. pmm's
     * next-fit cursor starts just past its bitmap (~1 MiB), and the few
     * dozen tables needed stay well inside that window. */
    page_directory_phys = alloc_table_phys();
    uint32_t *dir = phys_to_virt(page_directory_phys);

    uint32_t frames = pmm_total_frames();
    if (frames > MAX_OFFSET_MAPPED_FRAMES)
        frames = MAX_OFFSET_MAPPED_FRAMES;

    for (uint32_t f = 0; f < frames; f++) {
        uint32_t phys = f * FRAME_SIZE;
        uint32_t virt = KERNEL_VIRT_BASE + phys;
        uint32_t pd_idx = virt >> 22;
        uint32_t pt_idx = (virt >> 12) & 0x3FF;

        if (!(dir[pd_idx] & PAGE_PRESENT))
            dir[pd_idx] = alloc_table_phys() | PAGE_PRESENT | PAGE_WRITE;
        uint32_t *table = phys_to_virt(dir[pd_idx] & ~0xFFFu);
        table[pt_idx] = phys | PAGE_PRESENT | PAGE_WRITE;
    }

    /* Switch to the new directory (full TLB flush): the boot-time identity
     * mapping disappears here. CR4.PSE stays set but is unused — the new
     * tables contain only 4 KiB pages. */
    __asm__ volatile("mov %0, %%cr3" : : "r"(page_directory_phys));
}

static void map_page(uint32_t virt, uint32_t phys, uint32_t flags) {
    uint32_t *dir = phys_to_virt(page_directory_phys);
    uint32_t pd_idx = virt >> 22;
    uint32_t pt_idx = (virt >> 12) & 0x3FF;

    if (!(dir[pd_idx] & PAGE_PRESENT))
        dir[pd_idx] = alloc_table_phys() | PAGE_PRESENT | PAGE_WRITE;
    /* User access requires the user bit at both levels. */
    if (flags & PAGE_USER)
        dir[pd_idx] |= PAGE_USER;
    uint32_t *table = phys_to_virt(dir[pd_idx] & ~0xFFFu);
    table[pt_idx] = (phys & ~0xFFFu) | flags;
    __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

void paging_map(uint32_t virt, uint32_t phys) {
    map_page(virt, phys, PAGE_PRESENT | PAGE_WRITE);
}

void paging_map_user(uint32_t virt, uint32_t phys) {
    map_page(virt, phys, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
}
