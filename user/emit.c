/* emit.c - userland: write a fixed sentinel to stdout (fd 1, so it honors
 * redirection). Used to prove '>' / '<' actually move data through a file
 * rather than to the console. */
#include "usys.h"

void _start(void) {
    const char *s = "redirect-sentinel-ok\n";
    int len = 0;
    while (s[len])
        len++;
    sys_writefd(1, s, len);
    sys_exit(0);
}
