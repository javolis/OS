/* rectest.c - capture a short clip from the AC'97 input. CI has no real mic
 * (QEMU's "none" audiodev feeds silence), so this confirms the capture DMA
 * path runs without faulting and the syscall reports the samples it filled. */
#include "ulib.h"

#define FRAMES 4800 /* 0.1 s at 48 kHz */

static int16_t buf[FRAMES * 2]; /* interleaved L,R stereo */

void _start(void) {
    int n = sys_audio_record(buf, FRAMES * 2);
    if (n < 0) {
        uprintf("rec: no device\n");
        sys_exit(1);
    }
    uprintf("rec: captured %d samples\n", n);
    uprintf("rec: ok\n");
    sys_exit(0);
}
