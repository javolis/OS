/* ugfxtest.c - userland: exercise the ugfx library.
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

    unsigned red = ugfx_rgb(255, 0, 0);
    unsigned green = ugfx_rgb(0, 255, 0);
    unsigned blue = ugfx_rgb(0, 0, 255);

    ugfx_clear(&g, red);                  /* whole surface red */
    ugfx_fillrect(&g, 10, 10, 20, 20, green); /* a green square */
    ugfx_putpixel(&g, 40, 40, blue);      /* one blue pixel */

    /* Verify the backbuffer directly. Byte order is B, G, R. */
    int ok = 1;
    /* origin is untouched red: 00 00 FF */
    if (g.back[0] != 0x00 || g.back[1] != 0x00 || g.back[2] != 0xFF)
        ok = 0;
    /* (15,15) is inside the green square: 00 FF 00 */
    {
        unsigned char *p = g.back + 15u * g.pitch + 15u * g.bypp;
        if (p[0] != 0x00 || p[1] != 0xFF || p[2] != 0x00)
            ok = 0;
    }
    /* (40,40) is the blue pixel: FF 00 00 */
    {
        unsigned char *p = g.back + 40u * g.pitch + 40u * g.bypp;
        if (p[0] != 0xFF || p[1] != 0x00 || p[2] != 0x00)
            ok = 0;
    }
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
    if (r != (int)g.bypp || fp[0] != 0x00 || fp[1] != 0x00 || fp[2] != 0xFF) {
        uprintf("ugfxtest: FAIL flush readback r=%d\n", r);
        sys_exit(1);
    }

    uprintf("ugfxtest: ok %ux%u %ubpp\n", g.width, g.height, g.bpp);
    ugfx_free(&g);
    sys_exit(0);
}
