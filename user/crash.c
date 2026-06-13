/* crash.c - userland: deliberately faults so the kernel can prove it
 * kills the offending task and keeps everything else running. */
#include "usys.h"

void _start(void) {
    sys_write("crash: writing to NULL now\n");
    *(volatile int *)0 = 42; /* page fault in ring 3 - the kernel kills us */
    sys_write("crash: still alive?!\n"); /* must never print */
    sys_exit(0);
}
