/* cat.c - userland: stream a file (or stdin) to stdout through file
 * descriptors. */
#include "ulib.h"

void _start(int argc, char **argv) {
    int fd = 0; /* default: stdin */

    if (argc >= 2) {
        fd = sys_open(argv[1]);
        if (fd < 0) {
            uprintf("cat: %s: not found\n", argv[1]);
            sys_exit(1);
        }
    }

    char buf[128];
    int n;
    while ((n = sys_read(fd, buf, sizeof(buf))) > 0)
        sys_writefd(1, buf, n);

    if (fd > 0)
        sys_close(fd);
    sys_exit(0);
}
