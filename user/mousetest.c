/* mousetest.c - userland: read the mouse, then poll until CI's injected
 * movement and click show up, proving the PS/2 driver + SYS_MOUSE work. */
#include "ulib.h"

void _start(void) {
    struct mousestate m0, m;
    if (sys_mouse(&m0) != 0) {
        uprintf("mouse: no mouse\n");
        sys_exit(0);
    }
    uprintf("mouse: start %d,%d\n", m0.x, m0.y);

    int moved = 0, clicked = 0;
    for (int i = 0; i < 300; i++) { /* ~15s at 50ms; CI injects repeatedly */
        if (sys_mouse(&m) != 0)
            break;
        if (!moved && (m.x != m0.x || m.y != m0.y)) {
            moved = 1;
            uprintf("mouse: moved to %d,%d\n", m.x, m.y);
        }
        if (!clicked && (m.buttons & MOUSE_LEFT)) {
            clicked = 1;
            uprintf("mouse: button left\n");
        }
        if (moved && clicked)
            break;
        sys_sleep(50);
    }
    uprintf(moved ? "mouse: ok\n" : "mouse: no movement\n");
    sys_exit(moved ? 0 : 1);
}
