/* pmm.c — physical memory manager: a bitmap allocator over 4 KiB frames,
 * seeded from the Multiboot memory map.
 *
 * The bitmap (one bit per frame, set = used) is placed on the first frame
 * boundary after the kernel image. Everything starts reserved; frames inside
 * bootloader-reported available regions are freed, then the regions already
 * in use — low memory, the kernel image, and the bitmap itself — are
 * re-reserved. The Multiboot info structures live in low memory, so the
 * low-memory reservation covers them too. */
#include <stddef.h>
#include <stdint.h>

#include "memlayout.h"
#include "multiboot.h"
#include "pmm.h"

#define FRAME_SIZE 4096u

/* Image bounds provided by linker.ld (higher-half virtual addresses). */
extern char kernel_start[], kernel_end[];

static uint8_t *bitmap; /* higher-half pointer to the bitmap */
static uint32_t bitmap_phys;
static uint32_t bitmap_bytes;
static uint32_t total_frames;
static uint32_t free_count;
static uint32_t next_hint; /* next-fit scan start, avoids rescanning */

static int frame_used(uint32_t frame) {
    return bitmap[frame / 8] & (1 << (frame % 8));
}

static void mark_used(uint32_t frame) {
    if (!frame_used(frame)) {
        bitmap[frame / 8] |= (uint8_t)(1 << (frame % 8));
        free_count--;
    }
}

static void mark_free(uint32_t frame) {
    if (frame_used(frame)) {
        bitmap[frame / 8] &= (uint8_t)~(1 << (frame % 8));
        free_count++;
    }
}

/* Walk the Multiboot memory map; entries are variable-sized, with `size`
 * not counting the size field itself. GRUB hands over physical addresses,
 * accessed here through the higher-half window. */
#define MMAP_FIRST(mbi) \
    ((const struct multiboot_mmap_entry *)phys_to_virt((mbi)->mmap_addr))
#define MMAP_NEXT(e) \
    ((const struct multiboot_mmap_entry *)((const uint8_t *)(e) + (e)->size + 4))
#define MMAP_END(mbi) \
    ((const struct multiboot_mmap_entry *)((const uint8_t *)MMAP_FIRST(mbi) + \
                                           (mbi)->mmap_length))

void pmm_init(const struct multiboot_info *mbi) {
    /* Highest usable physical address, clamped to the 32-bit space. */
    uint64_t max_addr = 0;
    for (const struct multiboot_mmap_entry *e = MMAP_FIRST(mbi);
         e < MMAP_END(mbi); e = MMAP_NEXT(e)) {
        if (e->type != MULTIBOOT_MEMORY_AVAILABLE)
            continue;
        uint64_t end = e->addr + e->len;
        if (end > 0x100000000ull)
            end = 0x100000000ull;
        if (end > max_addr)
            max_addr = end;
    }
    total_frames = (uint32_t)(max_addr / FRAME_SIZE);

    /* Bitmap goes on the first frame boundary after the kernel image. */
    bitmap_phys = (virt_to_phys(kernel_end) + FRAME_SIZE - 1) &
                  ~(FRAME_SIZE - 1);
    bitmap = phys_to_virt(bitmap_phys);
    bitmap_bytes = (total_frames + 7) / 8;

    /* Start with everything reserved... */
    for (uint32_t i = 0; i < bitmap_bytes; i++)
        bitmap[i] = 0xFF;
    free_count = 0;

    /* ...free every frame fully contained in an available region... */
    for (const struct multiboot_mmap_entry *e = MMAP_FIRST(mbi);
         e < MMAP_END(mbi); e = MMAP_NEXT(e)) {
        if (e->type != MULTIBOOT_MEMORY_AVAILABLE)
            continue;
        if (e->addr >= 0x100000000ull)
            continue;
        uint64_t end = e->addr + e->len;
        if (end > 0x100000000ull)
            end = 0x100000000ull;
        uint32_t first = (uint32_t)((e->addr + FRAME_SIZE - 1) / FRAME_SIZE);
        uint32_t last = (uint32_t)(end / FRAME_SIZE);
        for (uint32_t f = first; f < last; f++)
            mark_free(f);
    }

    /* ...then re-reserve what's already occupied. Low memory also covers
     * the Multiboot structures; the image reservation starts at 1 MiB to
     * include the low-linked .boot section ahead of kernel_start. */
    for (uint32_t a = 0; a < 0x100000; a += FRAME_SIZE) /* low memory */
        mark_used(a / FRAME_SIZE);
    for (uint32_t a = 0x100000; a < virt_to_phys(kernel_end);
         a += FRAME_SIZE) /* .boot + kernel image */
        mark_used(a / FRAME_SIZE);
    for (uint32_t a = bitmap_phys; a < bitmap_phys + bitmap_bytes;
         a += FRAME_SIZE) /* the bitmap itself */
        mark_used(a / FRAME_SIZE);

    /* Keep the scan cursor in low physical memory: paging_init rebuilds the
     * page tables while only the first 4 MiB are mapped, so its frames must
     * come from down here. */
    next_hint = (bitmap_phys + bitmap_bytes) / FRAME_SIZE + 1;
}

uint32_t pmm_alloc_frame(void) {
    if (free_count == 0)
        return 0;
    for (uint32_t i = 0; i < total_frames; i++) {
        uint32_t frame = (next_hint + i) % total_frames;
        if (!frame_used(frame)) {
            mark_used(frame);
            next_hint = frame + 1;
            return frame * FRAME_SIZE;
        }
    }
    return 0; /* unreachable while free_count is accurate */
}

void pmm_free_frame(uint32_t addr) {
    mark_free(addr / FRAME_SIZE);
}

uint32_t pmm_total_frames(void) {
    return total_frames;
}

uint32_t pmm_free_frames(void) {
    return free_count;
}
