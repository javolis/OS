/* avuitest.c - userland: exercise the Avolis UI primitives and verify a few
 * pixels (gradient varies, panel surface is dark, focus ring + button fill
 * are orange). Self-checking so CI can confirm the toolkit renders. */
#include "avui.h"

static int orangeish(unsigned p) {
    int r = (p >> 16) & 0xFF, g = (p >> 8) & 0xFF, b = p & 0xFF;
    return r > 180 && g > 60 && g < 170 && b < 90;
}

void _start(void) {
    ugfx_t g;
    if (ugfx_init(&g) != 0) {
        uprintf("avui: no framebuffer\n");
        sys_exit(0);
    }
    int ok = 1;

    ugfx_vgradient(&g, 0, 0, (int)g.width, (int)g.height, ugfx_rgb(12, 14, 34),
                   ugfx_rgb(0, 0, 0));
    if (ugfx_getpixel(&g, 5, 2) == ugfx_getpixel(&g, 5, (int)g.height - 3))
        ok = 0; /* gradient should vary top to bottom */

    av_panel(&g, 100, 100, 200, 120, 1); /* focused -> orange ring */
    if (((ugfx_getpixel(&g, 200, 160) >> 16) & 0xFF) > 48)
        ok = 0; /* panel interior is the dark surface */
    int ring = 0;
    for (int yy = 96; yy < 104 && !ring; yy++)
        for (int xx = 110; xx < 290; xx++)
            if (orangeish(ugfx_getpixel(&g, xx, yy))) {
                ring = 1;
                break;
            }
    if (!ring)
        ok = 0;

    av_button(&g, 100, 260, 160, 44, "Open", 1); /* focused -> orange fill */
    if (!orangeish(ugfx_getpixel(&g, 180, 282)))
        ok = 0;

    ugfx_flush(&g);
    uprintf(ok ? "avui: ok\n" : "avui: FAIL\n");
    sys_exit(ok ? 0 : 1);
}
