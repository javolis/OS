/* gfxdemo.c - userland: a composed graphics scene using ugfx.
 *
 * Draws a title bar, scaled text, a color-swatch row, a nested-rectangle
 * figure and a border, then blits it all to the screen in one flush. This
 * is the first purely visual program; it self-checks the blit landed (the
 * top-left border pixel) so CI still verifies it over serial. */
#include "ugfx.h"

void _start(void) {
    ugfx_t g;
    if (ugfx_init(&g) != 0) {
        uprintf("gfxdemo: no framebuffer (text-mode boot)\n");
        sys_exit(0);
    }
    int W = (int)g.width, H = (int)g.height;

    /* Vertical gradient background: deep blue at top fading to black. */
    for (int y = 0; y < H; y++) {
        unsigned b = (unsigned)(120 - 120 * y / H); /* 120 -> 0 */
        ugfx_fillrect(&g, 0, y, W, 1, ugfx_rgb(0, b / 3, b));
    }

    /* Title bar with a big title and a subtitle. */
    ugfx_fillrect(&g, 0, 0, W, 92, ugfx_rgb(20, 20, 40));
    ugfx_text_scaled(&g, 20, 16, "javolis / OS", ugfx_rgb(120, 220, 255), 5);
    ugfx_text_scaled(&g, 22, 66, "framebuffer graphics", ugfx_rgb(160, 160, 190),
                     2);

    /* A row of color swatches: 8 of the 16.7M available colors. */
    unsigned sw[8] = {
        ugfx_rgb(220, 60, 60),  ugfx_rgb(220, 140, 60), ugfx_rgb(220, 220, 60),
        ugfx_rgb(60, 200, 80),  ugfx_rgb(60, 180, 220), ugfx_rgb(80, 80, 230),
        ugfx_rgb(170, 80, 220), ugfx_rgb(240, 240, 240)};
    int sx = 40, sy = 150, sww = 80, swh = 80, gap = 14;
    for (int i = 0; i < 8; i++)
        ugfx_fillrect(&g, sx + i * (sww + gap), sy, sww, swh, sw[i]);
    ugfx_text(&g, sx, sy + swh + 14, "8 of 16.7M colors (24bpp direct RGB)",
              ugfx_rgb(210, 210, 210));

    /* Nested rectangles as a simple figure, bottom-left. */
    for (int i = 0; i < 6; i++) {
        unsigned c = (i & 1) ? ugfx_rgb(255, 255, 255) : ugfx_rgb(40, 120, 200);
        int m = i * 14;
        ugfx_fillrect(&g, 60 + m, 320 + m, 220 - 2 * m, 220 - 2 * m, c);
    }
    ugfx_text_scaled(&g, 360, 400, "drawn from ring 3\nvia /dev/fb + ugfx",
                     ugfx_rgb(200, 220, 200), 2);

    /* White border frame, drawn last so (0,0) is a known sentinel. */
    unsigned wc = ugfx_rgb(255, 255, 255);
    ugfx_fillrect(&g, 0, 0, W, 3, wc);
    ugfx_fillrect(&g, 0, H - 3, W, 3, wc);
    ugfx_fillrect(&g, 0, 0, 3, H, wc);
    ugfx_fillrect(&g, W - 3, 0, 3, H, wc);

    if (ugfx_flush(&g) != 0) {
        uprintf("gfxdemo: FAIL flush\n");
        sys_exit(1);
    }

    /* Verify the blit landed: top-left pixel is the white border. */
    int fd = sys_open("/dev/fb");
    unsigned char fp[4] = {0, 0, 0, 0};
    int r = sys_read(fd, (char *)fp, (int)g.bypp);
    sys_close(fd);
    if (r != (int)g.bypp || fp[0] != 0xFF || fp[1] != 0xFF || fp[2] != 0xFF) {
        uprintf("gfxdemo: FAIL readback r=%d\n", r);
        sys_exit(1);
    }

    uprintf("gfxdemo: rendered %ux%u %ubpp ok\n", g.width, g.height, g.bpp);
    ugfx_free(&g);
    sys_exit(0);
}
