/* runner.c - userland: spawns children and waits for them, proving user
 * processes can manage other processes and collect exit codes. */
#include "ulib.h"

void _start(void) {
    sys_write("runner: spawning child\n");
    int pid = sys_spawn("echo.elf from runner child");
    if (pid < 0) {
        sys_write("runner: spawn failed\n");
        sys_exit(1);
    }
    sys_wait(pid);
    sys_write("runner: child finished\n");

    int pid2 = sys_spawn("exitcode.elf");
    if (pid2 >= 0)
        uprintf("runner: child status=%d\n", sys_wait(pid2));
    sys_exit(0);
}
