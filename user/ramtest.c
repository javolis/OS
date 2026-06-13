/* ramtest.c - userland: prove the ramfs round-trips. Create a file, write
 * to it, close, reopen, read it back, and print what came out. */
#include "ulib.h"

void _start(void) {
    const char *msg = "ramfs round-trip works\n";

    int fd = sys_create("scratch.txt");
    if (fd < 0) {
        uprintf("ramtest: create failed\n");
        sys_exit(1);
    }
    sys_writefd(fd, msg, (int)ustrlen(msg));
    sys_close(fd);

    fd = sys_open("scratch.txt");
    if (fd < 0) {
        uprintf("ramtest: reopen failed\n");
        sys_exit(1);
    }
    char buf[64];
    int n = sys_read(fd, buf, sizeof(buf) - 1);
    sys_close(fd);
    if (n <= 0) {
        uprintf("ramtest: read failed\n");
        sys_exit(1);
    }
    buf[n] = '\0';
    uprintf("ramtest: %s", buf);
    sys_exit(0);
}
