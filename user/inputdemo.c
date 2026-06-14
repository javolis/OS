/* inputdemo.c - userland: an interactive graphics loop.
 *
 * Draws a box that moves in response to w/a/s/d keys read one at a time
 * through SYS_GETKEY (no echo), redrawing the whole frame each move. 'q'
 * quits. The loop is bounded so it always terminates (CI drives it with a
 * fixed key sequence and cannot hang). Movements are logged to serial so
 * CI can confirm interaction without watching the screen. */
#include "ugfx.h"

static void scene(ugfx_t *g, int bx, int by) {
    ugfx_clear(g, ugfx_rgb(10, 12, 30));
    ugfx_text_scaled(g, 20, 20, "ugfx input demo", ugfx_rgb(120, 220, 255), 3);
    ugfx_text(g, 20, 70, "move: w a s d    quit: q", ugfx_rgb(200, 200, 200));
    ugfx_fillrect(g, bx, by, 60, 60, ugfx_rgb(240, 200, 60));     /* box */
    ugfx_fillrect(g, bx + 8, by + 8, 44, 44, ugfx_rgb(200, 60, 60)); /* inset */
    ugfx_flush(g);
}

void _start(void) {
    ugfx_t g;
    if (ugfx_init(&g) != 0) {
        uprintf("inputdemo: no framebuffer\n");
        sys_exit(0);
    }
    int W = (int)g.width, H = (int)g.height;
    int bx = W / 2 - 30, by = H / 2 - 30;
    const int step = 40;
    int moves = 0;

    scene(&g, bx, by);
    uprintf("inputdemo: ready at %d,%d\n", bx, by);

    for (int i = 0; i < 16; i++) { /* bounded: always terminates */
        int k = sys_getkey();
        if (k < 0)
            break; /* not foreground */
        if (k == 'q' || k == 'Q')
            break;
        int moved = 1;
        if (k == 'w' || k == 'W')
            by -= step;
        else if (k == 's' || k == 'S')
            by += step;
        else if (k == 'a' || k == 'A')
            bx -= step;
        else if (k == 'd' || k == 'D')
            bx += step;
        else
            moved = 0;
        if (bx < 0)
            bx = 0;
        if (bx > W - 60)
            bx = W - 60;
        if (by < 0)
            by = 0;
        if (by > H - 60)
            by = H - 60;
        if (moved) {
            moves++;
            scene(&g, bx, by);
            uprintf("inputdemo: key %c -> %d,%d\n", (char)k, bx, by);
        }
    }

    uprintf("inputdemo: bye after %d moves\n", moves);
    ugfx_free(&g);
    sys_exit(0);
}
