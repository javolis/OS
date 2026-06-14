/* ugfxtest.c - userland: exercise the ugfx library (shapes and text).
 *
 * Draws into the backbuffer, verifies the exact pixel bytes in memory (no
 * device needed), then flushes to /dev/fb and reads the first pixel back to
 * confirm the blit landed. Self-checking so CI verifies it over serial. */
#include "ugfx.h"

void _start(void) {
    ugfx_t g;
    if (ugfx_init(&g) != 0) {
        uprintf("ugfxtest: no framebuffer\n");
        sys_exit(0);
    }

    unsigned white = ugfx_rgb(255, 255, 255);
    unsigned green = ugfx_rgb(0, 255, 0);
    unsigned blue = ugfx_rgb(0, 0, 255);

    ugfx_clear(&g, ugfx_rgb(0, 0, 0));        /* black field */
    ugfx_fillrect(&g, 10, 10, 20, 20, green); /* a green square */
    ugfx_putpixel(&g, 40, 40, blue);          /* one blue pixel */
    /* A hyphen glyph: its bitmap is 0x7E on row 3 only (cols 1..6 set),
     * every other row empty - precise pixels to verify text rendering. */
    ugfx_char(&g, 100, 100, '-', white);
    ugfx_text(&g, 100, 120, "ugfx", white); /* exercise the string path */

    /* Verify the backbuffer directly. Byte order is B, G, R. */
    int ok = 1;
#define CHK(X, Y, B, G_, R)                                                    \
    do {                                                                       \
        unsigned char *p = g.back + (unsigned)(Y) * g.pitch +                  \
                           (unsigned)(X) * g.bypp;                             \
        if (p[0] != (B) || p[1] != (G_) || p[2] != (R))                        \
            ok = 0;                                                            \
    } while (0)

    CHK(0, 0, 0x00, 0x00, 0x00);    /* untouched background: black */
    CHK(15, 15, 0x00, 0xFF, 0x00);  /* inside green square */
    CHK(40, 40, 0xFF, 0x00, 0x00);  /* the blue pixel */
    CHK(101, 103, 0xFF, 0xFF, 0xFF); /* glyph '-' row 3 col 1: lit white */
    CHK(100, 100, 0x00, 0x00, 0x00); /* glyph cell row 0: transparent (bg) */
    CHK(100, 103, 0x00, 0x00, 0x00); /* glyph '-' row 3 col 0: off (bg) */
#undef CHK
    if (!ok) {
        uprintf("ugfxtest: FAIL backbuffer\n");
        sys_exit(1);
    }

    /* Flush to the screen, then read the first pixel back from /dev/fb. */
    if (ugfx_flush(&g) != 0) {
        uprintf("ugfxtest: FAIL flush\n");
        sys_exit(1);
    }
    int fd = sys_open("/dev/fb");
    unsigned char fp[4] = {0, 0, 0, 0};
    int r = sys_read(fd, (char *)fp, (int)g.bypp);
    sys_close(fd);
    if (r != (int)g.bypp || fp[0] != 0x00 || fp[1] != 0x00 || fp[2] != 0x00) {
        uprintf("ugfxtest: FAIL flush readback r=%d\n", r);
        sys_exit(1);
    }

    uprintf("ugfxtest: ok %ux%u %ubpp text+gfx\n", g.width, g.height, g.bpp);
    ugfx_free(&g);
    sys_exit(0);
}
