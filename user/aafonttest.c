/* aafonttest.c - userland: render anti-aliased text and prove it is actually
 * antialiased. We draw the heading in pure-red-channel orange (R=255) on a
 * black field; fully-lit glyph pixels read R=255, background reads R=0, and
 * any pixel with 0 < R < 255 can only come from coverage blending - i.e. an
 * antialiased edge. Counting those is a self-check CI can verify. */
#include "uafont.h"

void _start(void) {
    ugfx_t g;
    if (ugfx_init(&g) != 0) {
        uprintf("aafont: no framebuffer\n");
        sys_exit(0);
    }
    ugfx_clear(&g, ugfx_rgb(0, 0, 0));
    unsigned orange = ugfx_rgb(255, 122, 26); /* R = 255 */
    ua_text(&g, UAFONT_HEAD, 40, 80, "Avolis", orange);
    ua_text(&g, UAFONT_BODY, 40, 120, "anti-aliased text", ugfx_rgb(240, 240, 240));

    int edges = 0;
    for (int y = 48; y < 92 && y < (int)g.height; y++)
        for (int x = 38; x < 220 && x < (int)g.width; x++) {
            unsigned r = (ugfx_getpixel(&g, x, y) >> 16) & 0xFF;
            if (r > 0 && r < 255)
                edges++;
        }
    ugfx_flush(&g);

    if (edges > 20)
        uprintf("aafont: antialiased ok (%d edge px)\n", edges);
    else
        uprintf("aafont: FAIL not antialiased (%d)\n", edges);
    sys_exit(edges > 20 ? 0 : 1);
}
