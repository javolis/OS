/* avwalltest.c - userland: render the constellation wallpaper and confirm it
 * drew (orange node/line pixels present, which the dark warm vignette can't
 * produce). Self-checking so CI verifies it over serial. */
#include "avwall.h"

void _start(void) {
    ugfx_t g;
    if (ugfx_init(&g) != 0) {
        uprintf("avwall: no framebuffer\n");
        sys_exit(0);
    }
    avwall_render(&g, 0x12345678, 0x281404);
    int orange = 0;
    for (int y = 0; y < (int)g.height && !orange; y += 2)
        for (int x = 0; x < (int)g.width; x += 2) {
            unsigned p = ugfx_getpixel(&g, x, y);
            int r = (p >> 16) & 0xFF, gg = (p >> 8) & 0xFF, b = p & 0xFF;
            if (r > 150 && gg > 60 && gg < 180 && b < 100) {
                orange = 1;
                break;
            }
        }
    ugfx_flush(&g);
    uprintf(orange ? "avwall: ok\n" : "avwall: FAIL\n");
    sys_exit(orange ? 0 : 1);
}
