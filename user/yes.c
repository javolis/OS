/* yes.c - userland: repeatedly print a line (argv[1] or 'y') until the
 * write fails (e.g. a downstream pipe reader closed). The canonical
 * broken-pipe producer. */
#include "ulib.h"

void _start(int argc, char **argv) {
    const char *s = (argc >= 2) ? argv[1] : "y";
    char line[64];
    int i = 0;
    while (s[i] && i < (int)sizeof(line) - 1) {
        line[i] = s[i];
        i++;
    }
    line[i++] = '\n';

    while (sys_writefd(1, line, i) >= 0)
        ; /* loop until a broken pipe (or forever, to the console) */
    sys_exit(0);
}
