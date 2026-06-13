/* tee.c - userland: copy stdin to stdout and also to a named file
 * (argv[1], a ramfs file). A pipeline tap. */
#include "ulib.h"

void _start(int argc, char **argv) {
    int fd = -1;
    if (argc >= 2) {
        fd = sys_create(argv[1]);
        if (fd < 0) {
            uprintf("tee: cannot create %s\n", argv[1]);
            sys_exit(1);
        }
    }
    char buf[128];
    int n;
    while ((n = sys_read(0, buf, sizeof(buf))) > 0) {
        sys_writefd(1, buf, n);
        if (fd >= 0)
            sys_writefd(fd, buf, n);
    }
    if (fd >= 0)
        sys_close(fd);
    sys_exit(0);
}
