/* envtest.c - userland: set an environment variable, then spawn a child
 * that reads it, demonstrating a spawned program inherits the environment. */
#include "ulib.h"

void _start(void) {
    if (sys_setenv("GREETING", "hello-env") != 0) {
        uprintf("envtest: FAIL setenv\n");
        sys_exit(1);
    }
    int pid = sys_spawn("envchild.elf");
    if (pid < 0) {
        uprintf("envtest: FAIL spawn\n");
        sys_exit(1);
    }
    sys_wait(pid);
    uprintf("envtest: done\n");
    sys_exit(0);
}
