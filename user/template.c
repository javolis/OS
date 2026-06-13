/* template.c - starter for your own app.
 *
 * Copy this to user/<name>.c, add user/<name>.elf to USER_ELFS in the
 * Makefile, `make iso`, then `run <name>.elf [args...]` from a shell.
 *
 * Available to you:
 *   - syscalls in usys.h  (sys_read/sys_writefd/sys_open/sys_spawn/...)
 *   - ulib.h: uprintf, umalloc/ufree, the ustr and umem helpers,
 *             uputs/ugetline, and the ufopen/ufgets file reader.
 *
 * Your program gets argc/argv, a 16 KiB stack, an on-demand heap, and
 * fds 0/1 on the console (or pipes/files under shell redirection). */
#include "ulib.h"

void _start(int argc, char **argv) {
    const char *who = (argc >= 2) ? argv[1] : "world";

    /* heap + formatted output, just to show the pieces working */
    char *msg = umalloc(64);
    ustrcpy(msg, "hello, ");
    ustrcat(msg, who);
    uprintf("template: %s\n", msg);
    ufree(msg);

    sys_exit(0);
}
