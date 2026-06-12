/* sysinfo.c — userland: system stats via SYS_SYSINFO, formatted with the
 * tiny user libc. */
#include "ulib.h"

void _start(void) {
    struct sysinfo si;

    if (sys_sysinfo(&si) == 0)
        uprintf("sysinfo: ticks=%u frames=%u/%u tasks=%u\n", si.ticks,
                si.free_frames, si.total_frames, si.tasks_alive);
    else
        uprintf("sysinfo: syscall failed\n");
    sys_exit();
}
