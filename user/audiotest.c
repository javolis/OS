/* audiotest.c - synthesize a short tone and play it through AC'97. CI can't
 * hear audio, so this confirms the codec was found, the DMA playback path
 * runs without faulting, and the syscall reports the samples it played. */
#include "ulib.h"

#define FRAMES 4800 /* 0.1 s at 48 kHz */

static int16_t buf[FRAMES * 2]; /* interleaved L,R stereo */

void _start(void) {
    int period = 48000 / 440; /* ~109 samples per 440 Hz cycle */
    for (int i = 0; i < FRAMES; i++) {
        int16_t s = ((i % period) < period / 2) ? 8000 : -8000; /* square wave */
        buf[i * 2] = s;
        buf[i * 2 + 1] = s;
    }

    int n = sys_audio(buf, FRAMES * 2);
    if (n < 0) {
        uprintf("audio: no device\n");
        sys_exit(1);
    }
    uprintf("audio: played %d samples\n", n);
    uprintf("audio: ok\n");
    sys_exit(0);
}
