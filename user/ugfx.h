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

static inline void ugfx_free(ugfx_t *g) {
    if (g->back)
        ufree(g->back);
    g->back = 0;
    g->ok = 0;
}
