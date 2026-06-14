/* ugfx.h - tiny userland 2D graphics over /dev/fb (header-only).
 *
 * Draw into an in-memory backbuffer with putpixel/fillrect/clear, then
 * ugfx_flush() blits the whole buffer to the screen in one pass (so the
 * display never shows a half-drawn frame). Colors are 0x00RRGGBB; the
 * backbuffer is stored in the framebuffer's own byte order (B, G, R[, x])
 * so a flush is a straight copy. Works on 24bpp and 32bpp framebuffers.
 *
 * Header-only static inlines: just #include "ugfx.h" - no extra objects to
 * link. The backbuffer comes from umalloc, so the program needs the heap
 * (it does by default). */
#pragma once
#include "ulib.h"

#include "font8x8.h" /* shared 8x8 bitmap font (compiled with -Iinclude) */

typedef struct {
    int ok;                 /* 1 once a framebuffer is available */
    unsigned width, height; /* pixels */
    unsigned pitch;         /* backbuffer bytes per row (= device pitch) */
    unsigned bpp, bypp;     /* bits / bytes per pixel (24->3, 32->4) */
    unsigned char *back;    /* backbuffer, pitch*height bytes (umalloc) */
} ugfx_t;

/* Pack a color. Stored as 0x00RRGGBB; ugfx writes the device byte order. */
static inline unsigned ugfx_rgb(unsigned r, unsigned g, unsigned b) {
    return ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF);
}

/* Query geometry and allocate the backbuffer. Returns 0 on success, -1 if
 * there is no framebuffer (VGA-text boot) or the heap is exhausted. */
static inline int ugfx_init(ugfx_t *g) {
    struct fbinfo fi;
    g->ok = 0;
    g->back = 0;
    if (sys_fbinfo(&fi) != 0)
        return -1;
    g->width = fi.width;
    g->height = fi.height;
    g->pitch = fi.pitch;
    g->bpp = fi.bpp;
    g->bypp = fi.bpp / 8;
    g->back = (unsigned char *)umalloc(g->pitch * g->height);
    if (!g->back)
        return -1;
    g->ok = 1;
    return 0;
}

static inline void ugfx_putpixel(ugfx_t *g, int x, int y, unsigned rgb) {
    if (!g->ok || x < 0 || y < 0 || (unsigned)x >= g->width ||
        (unsigned)y >= g->height)
        return;
    unsigned char *p = g->back + (unsigned)y * g->pitch + (unsigned)x * g->bypp;
    p[0] = (unsigned char)rgb;         /* B */
    p[1] = (unsigned char)(rgb >> 8);  /* G */
    p[2] = (unsigned char)(rgb >> 16); /* R */
    if (g->bypp == 4)
        p[3] = 0;
}

/* Read a backbuffer pixel as 0x00RRGGBB (bytes are B, G, R). */
static inline unsigned ugfx_getpixel(ugfx_t *g, int x, int y) {
    if (!g->ok || x < 0 || y < 0 || (unsigned)x >= g->width ||
        (unsigned)y >= g->height)
        return 0;
    unsigned char *p = g->back + (unsigned)y * g->pitch + (unsigned)x * g->bypp;
    return ((unsigned)p[2] << 16) | ((unsigned)p[1] << 8) | (unsigned)p[0];
}

/* Alpha-blend rgb over the existing pixel with coverage cov (0..255). This
 * is what makes anti-aliased text and soft UI edges possible. */
static inline void ugfx_blend_pixel(ugfx_t *g, int x, int y, unsigned rgb,
                                    unsigned cov) {
    if (cov == 0)
        return;
    if (cov >= 255) {
        ugfx_putpixel(g, x, y, rgb);
        return;
    }
    unsigned bg = ugfx_getpixel(g, x, y);
    int br = (int)((bg >> 16) & 0xFF), bgc = (int)((bg >> 8) & 0xFF),
        bb = (int)(bg & 0xFF);
    int fr = (int)((rgb >> 16) & 0xFF), fgc = (int)((rgb >> 8) & 0xFF),
        fb = (int)(rgb & 0xFF);
    int a = (int)cov;
    unsigned r = (unsigned)(br + (fr - br) * a / 255);
    unsigned gg = (unsigned)(bgc + (fgc - bgc) * a / 255);
    unsigned b = (unsigned)(bb + (fb - bb) * a / 255);
    ugfx_putpixel(g, x, y, (r << 16) | (gg << 8) | b);
}

/* Filled rectangle, clipped to the surface (negative origins are fine). */
static inline void ugfx_fillrect(ugfx_t *g, int x, int y, int w, int h,
                                 unsigned rgb) {
    for (int yy = y; yy < y + h; yy++)
        for (int xx = x; xx < x + w; xx++)
            ugfx_putpixel(g, xx, yy, rgb);
}

static inline void ugfx_clear(ugfx_t *g, unsigned rgb) {
    ugfx_fillrect(g, 0, 0, (int)g->width, (int)g->height, rgb);
}

/* Blit the whole backbuffer to /dev/fb. Returns 0 on success, -1 otherwise.
 * Re-opens the device each call so the write starts at offset 0. */
static inline int ugfx_flush(ugfx_t *g) {
    if (!g->ok)
        return -1;
    int fd = sys_open("/dev/fb");
    if (fd < 0)
        return -1;
    unsigned total = g->pitch * g->height;
    unsigned off = 0;
    while (off < total) {
        unsigned chunk = total - off;
        if (chunk > 1024) /* SYS_WRITE_MAX */
            chunk = 1024;
        int w = sys_writefd(fd, (const char *)(g->back + off), (int)chunk);
        if (w <= 0)
            break;
        off += (unsigned)w;
    }
    sys_close(fd);
    return (off == total) ? 0 : -1;
}

/* Draw one 8x8 glyph at pixel (x,y) in color rgb. "Off" pixels are left
 * untouched (transparent), so text overlays whatever is already drawn. */
static inline void ugfx_char(ugfx_t *g, int x, int y, char c, unsigned rgb) {
    unsigned char uc = (unsigned char)c;
    if (uc < FONT_FIRST || uc > FONT_LAST)
        uc = ' ';
    const uint8_t *gl = font8x8[uc - FONT_FIRST];
    for (int row = 0; row < FONT_H; row++) {
        uint8_t bits = gl[row];
        for (int col = 0; col < FONT_W; col++)
            if (bits & (0x80 >> col))
                ugfx_putpixel(g, x + col, y + row, rgb);
    }
}

/* Draw a NUL-terminated string starting at (x,y); 8px per glyph, '\n' wraps
 * to a new line at the starting x. */
static inline void ugfx_text(ugfx_t *g, int x, int y, const char *s,
                             unsigned rgb) {
    int cx = x, cy = y;
    for (; *s; s++) {
        if (*s == '\n') {
            cx = x;
            cy += FONT_H;
            continue;
        }
        ugfx_char(g, cx, cy, *s, rgb);
        cx += FONT_W;
    }
}

/* Like ugfx_text, but each font pixel becomes a scale x scale block, for
 * big titles. scale < 1 is treated as 1. */
static inline void ugfx_text_scaled(ugfx_t *g, int x, int y, const char *s,
                                    unsigned rgb, int scale) {
    if (scale < 1)
        scale = 1;
    int cx = x, cy = y;
    for (; *s; s++) {
        if (*s == '\n') {
            cx = x;
            cy += FONT_H * scale;
            continue;
        }
        unsigned char uc = (unsigned char)*s;
        if (uc < FONT_FIRST || uc > FONT_LAST)
            uc = ' ';
        const uint8_t *gl = font8x8[uc - FONT_FIRST];
        for (int row = 0; row < FONT_H; row++)
            for (int col = 0; col < FONT_W; col++)
                if (gl[row] & (0x80 >> col))
                    ugfx_fillrect(g, cx + col * scale, cy + row * scale, scale,
                                  scale, rgb);
        cx += FONT_W * scale;
    }
}

static inline void ugfx_free(ugfx_t *g) {
    if (g->back)
        ufree(g->back);
    g->back = 0;
    g->ok = 0;
}
