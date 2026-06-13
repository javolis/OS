/* fb.h - linear framebuffer (32bpp RGB) provided by the bootloader. */
#pragma once
#include <stdint.h>

struct multiboot_info;

/* Parse and map the bootloader framebuffer. Returns 1 if a usable 32bpp
 * RGB framebuffer is available, 0 otherwise (caller falls back to VGA). */
int fb_init(const struct multiboot_info *mbi);

int fb_available(void);
uint32_t fb_width(void);
uint32_t fb_height(void);
uint32_t fb_pitch(void); /* bytes per scanline */
uint8_t *fb_base(void);  /* mapped kernel VA, or NULL */

void fb_putpixel(uint32_t x, uint32_t y, uint32_t rgb);
void fb_fill(uint32_t rgb);
void fb_fillrect(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                 uint32_t rgb);

/* Cheap additive checksum of a pixel region - lets CI confirm (over
 * serial) that drawing actually changed the framebuffer. */
uint32_t fb_checksum(uint32_t x, uint32_t y, uint32_t w, uint32_t h);
