/* killtest.c - userland: spawn a long-running child, kill it by pid, and
 * confirm wait reports it was killed (status -1) rather than exited. */
#include "ulib.h"

void _start(void) {
    int pid = sys_spawn("clock.elf"); /* 10 x 300ms sleeps; plenty of time */
    if (pid < 0) {
        uprintf("killtest: spawn failed\n");
        sys_exit(1);
    }
    if (sys_kill(pid) != 0) {
        uprintf("killtest: kill failed\n");
        sys_exit(1);
    }
    int status = sys_wait(pid); /* killed tasks report -1 */
    if (status == -1)
        uprintf("killtest: killed child cleanly\n");
    else
        uprintf("killtest: FAIL status=%d\n", status);
    sys_exit(0);
}
