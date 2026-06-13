/* fb.c - linear 32bpp framebuffer console backend.
 *
 * GRUB (via the multiboot video request in boot.s) hands us a linear
 * framebuffer: a physical address plus geometry. We map it into a fixed
 * kernel window and expose pixel/rectangle primitives. Only direct-RGB
 * 32bpp is handled; anything else makes fb_init return 0 so the caller
 * keeps the legacy VGA text console. */
#include <stddef.h>
#include <stdint.h>

#include "fb.h"
#include "memlayout.h"
#include "multiboot.h"
#include "paging.h"

#define FB_VIRT_BASE 0xF0000000u  /* kernel window for the framebuffer */
#define FB_MAX_BYTES (16u << 20)  /* cap the mapping at 16 MiB */
#define FRAME_SIZE 4096u

static uint8_t *fb;
static uint32_t fb_w, fb_h, fb_p;

int fb_init(const struct multiboot_info *mbi) {
    if (!(mbi->flags & MULTIBOOT_INFO_FRAMEBUFFER))
        return 0;
    if (mbi->framebuffer_type != MULTIBOOT_FRAMEBUFFER_TYPE_RGB ||
        mbi->framebuffer_bpp != 32)
        return 0;

    uint32_t phys = (uint32_t)mbi->framebuffer_addr;
    fb_w = mbi->framebuffer_width;
    fb_h = mbi->framebuffer_height;
    fb_p = mbi->framebuffer_pitch;

    uint32_t bytes = fb_p * fb_h;
    if (bytes == 0 || bytes > FB_MAX_BYTES)
        return 0;

    /* Map the framebuffer (likely high MMIO, outside the offset-mapped
     * RAM window) into the kernel's fixed fb window, page by page. */
    uint32_t pages = (bytes + FRAME_SIZE - 1) / FRAME_SIZE;
    for (uint32_t i = 0; i < pages; i++)
        paging_map(FB_VIRT_BASE + i * FRAME_SIZE, phys + i * FRAME_SIZE);

    fb = (uint8_t *)FB_VIRT_BASE;
    return 1;
}

int fb_available(void) {
    return fb != NULL;
}
uint32_t fb_width(void) {
    return fb_w;
}
uint32_t fb_height(void) {
    return fb_h;
}
uint32_t fb_pitch(void) {
    return fb_p;
}
uint8_t *fb_base(void) {
    return fb;
}

void fb_putpixel(uint32_t x, uint32_t y, uint32_t rgb) {
    if (!fb || x >= fb_w || y >= fb_h)
        return;
    *(uint32_t *)(fb + y * fb_p + x * 4) = rgb;
}

void fb_fillrect(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                 uint32_t rgb) {
    if (!fb)
        return;
    for (uint32_t yy = y; yy < y + h && yy < fb_h; yy++) {
        uint32_t *row = (uint32_t *)(fb + yy * fb_p);
        for (uint32_t xx = x; xx < x + w && xx < fb_w; xx++)
            row[xx] = rgb;
    }
}

void fb_fill(uint32_t rgb) {
    fb_fillrect(0, 0, fb_w, fb_h, rgb);
}

uint32_t fb_checksum(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    uint32_t sum = 0;
    if (!fb)
        return 0;
    for (uint32_t yy = y; yy < y + h && yy < fb_h; yy++) {
        const uint32_t *row = (const uint32_t *)(fb + yy * fb_p);
        for (uint32_t xx = x; xx < x + w && xx < fb_w; xx++)
            sum = sum * 31u + row[xx];
    }
    return sum;
}
