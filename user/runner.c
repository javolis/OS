/* runner.c — userland: spawns a child program and waits for it, proving
 * user processes can manage other processes. */
#include "usys.h"

void _start(void) {
    sys_write("runner: spawning child\n");
    int pid = sys_spawn("echo.elf from runner child");
    if (pid < 0) {
        sys_write("runner: spawn failed\n");
        sys_exit();
    }
    sys_wait(pid);
    sys_write("runner: child finished\n");
    sys_exit();
}
