/* clock.c - userland: prints a tick five times at 300 ms intervals using
 * the blocking sleep syscall. While it sleeps, the scheduler runs other
 * tasks - including the shell, which stays responsive between ticks. */
#include "usys.h"

void _start(void) {
    for (int i = 0; i < 10; i++) {
        sys_write("clock: tick\n");
        sys_sleep(300);
    }
    sys_write("clock: done\n");
    sys_exit(0);
}
