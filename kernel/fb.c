/* fb.c - linear framebuffer console backend (24bpp and 32bpp).
 *
 * GRUB (via the multiboot video request in boot.s) hands us a linear
 * framebuffer: a physical address plus geometry. We map it into a fixed
 * kernel window and expose pixel/rectangle primitives. Direct-RGB at 24
 * or 32 bits per pixel is handled (BIOS VBE often picks 24bpp, UEFI GOP
 * 32bpp); anything else makes fb_init return 0 so the caller keeps the
 * legacy VGA text console.
 *
 * Pixel values are passed around as 0x00RRGGBB. The byte layout in memory
 * is the standard direct-color order (little-endian: B, G, R[, x]), which
 * is what QEMU std-VGA VBE and UEFI GOP both report; we don't consult
 * color_info field positions, so an exotic channel order would render with
 * swapped colors but still boot. */
#include <stddef.h>
#include <stdint.h>

#include "fb.h"
#include "font8x8.h"
#include "memlayout.h"
#include "multiboot.h"
#include "paging.h"

#define FB_VIRT_BASE 0xF0000000u  /* kernel window for the framebuffer */
#define FB_MAX_BYTES (32u << 20)  /* cap the mapping at 32 MiB (up to ~1440p) */
#define FRAME_SIZE 4096u

static uint8_t *fb;
static uint32_t fb_w, fb_h, fb_p;
static uint32_t fb_bypp;          /* bytes per pixel: 3 (24bpp) or 4 (32bpp) */

int fb_init(const struct multiboot_info *mbi) {
    if (!(mbi->flags & MULTIBOOT_INFO_FRAMEBUFFER))
        return 0;
    if (mbi->framebuffer_type != MULTIBOOT_FRAMEBUFFER_TYPE_RGB ||
        (mbi->framebuffer_bpp != 32 && mbi->framebuffer_bpp != 24))
        return 0;

    uint32_t phys = (uint32_t)mbi->framebuffer_addr;
    fb_w = mbi->framebuffer_width;
    fb_h = mbi->framebuffer_height;
    fb_p = mbi->framebuffer_pitch;
    fb_bypp = mbi->framebuffer_bpp / 8;

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
uint32_t fb_bpp(void) {
    return fb_bypp * 8;
}
uint8_t *fb_base(void) {
    return fb;
}

void fb_putpixel(uint32_t x, uint32_t y, uint32_t rgb) {
    if (!fb || x >= fb_w || y >= fb_h)
        return;
    uint8_t *p = fb + y * fb_p + x * fb_bypp;
    if (fb_bypp == 4) {
        *(uint32_t *)p = rgb;
    } else { /* 24bpp: write B, G, R (the low three bytes of 0x00RRGGBB) */
        p[0] = (uint8_t)rgb;
        p[1] = (uint8_t)(rgb >> 8);
        p[2] = (uint8_t)(rgb >> 16);
    }
}

void fb_fillrect(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                 uint32_t rgb) {
    if (!fb)
        return;
    for (uint32_t yy = y; yy < y + h && yy < fb_h; yy++) {
        uint8_t *p = fb + yy * fb_p + x * fb_bypp;
        for (uint32_t xx = x; xx < x + w && xx < fb_w; xx++) {
            if (fb_bypp == 4) {
                *(uint32_t *)p = rgb;
            } else {
                p[0] = (uint8_t)rgb;
                p[1] = (uint8_t)(rgb >> 8);
                p[2] = (uint8_t)(rgb >> 16);
            }
            p += fb_bypp;
        }
    }
}

void fb_fill(uint32_t rgb) {
    fb_fillrect(0, 0, fb_w, fb_h, rgb);
}

void fb_draw_glyph(uint32_t px, uint32_t py, char c, uint32_t fg,
                   uint32_t bg) {
    if (!fb)
        return;
    unsigned char uc = (unsigned char)c;
    if (uc < FONT_FIRST || uc > FONT_LAST)
        uc = ' ';
    const uint8_t *g = font8x8[uc - FONT_FIRST];
    for (int row = 0; row < FONT_H; row++) {
        uint8_t bits = g[row];
        for (int col = 0; col < FONT_W; col++)
            fb_putpixel(px + col, py + row, (bits & (0x80 >> col)) ? fg : bg);
    }
}

/* Shift the whole framebuffer up by `lines` pixels; fill the exposed
 * bottom band with bg. */
void fb_scroll(uint32_t lines, uint32_t bg) {
    if (!fb || lines == 0 || lines >= fb_h)
        return;
    uint32_t keep = (fb_h - lines) * fb_p;
    uint8_t *dst = fb;
    const uint8_t *src = fb + lines * fb_p;
    for (uint32_t i = 0; i < keep; i++)
        dst[i] = src[i];
    fb_fillrect(0, fb_h - lines, fb_w, lines, bg);
}

uint32_t fb_checksum(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    uint32_t sum = 0;
    if (!fb)
        return 0;
    for (uint32_t yy = y; yy < y + h && yy < fb_h; yy++) {
        const uint8_t *p = fb + yy * fb_p + x * fb_bypp;
        for (uint32_t xx = x; xx < x + w && xx < fb_w; xx++) {
            uint32_t px = (fb_bypp == 4)
                              ? *(const uint32_t *)p
                              : (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                                    ((uint32_t)p[2] << 16);
            sum = sum * 31u + px;
            p += fb_bypp;
        }
    }
    return sum;
}
