/* beeptest.c - exercise the PC speaker via SYS_BEEP. CI can't hear audio, so
 * this just confirms the tone syscalls return 0 and the kernel stays alive
 * through them (the speaker is gated off again after each tone). */
#include "ulib.h"

void _start(void) {
    int notes[4] = {523, 659, 784, 1047}; /* C5 E5 G5 C6 arpeggio */
    int ok = 1;
    for (int i = 0; i < 4; i++)
        if (sys_beep(notes[i], 90) != 0)
            ok = 0;
    if (sys_beep(0, 0) != 0) /* explicit silence */
        ok = 0;
    uprintf(ok ? "beep: ok\n" : "beep: FAIL\n");
    sys_exit(ok ? 0 : 1);
}
