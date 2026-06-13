/* appendtest.c - userland: prove sys_append concatenates and sys_unlink
 * removes. Self-contained so the smoke test needs only one command. */
#include "ulib.h"

void _start(void) {
    int fd = sys_create("log.txt");
    sys_writefd(fd, "A\n", 2);
    sys_close(fd);

    fd = sys_append("log.txt");
    sys_writefd(fd, "B\n", 2);
    sys_close(fd);

    fd = sys_open("log.txt");
    char buf[16];
    int n = sys_read(fd, buf, sizeof(buf) - 1);
    sys_close(fd);
    buf[n > 0 ? n : 0] = '\0';

    if (!ustreq(buf, "A\nB\n")) {
        uprintf("appendtest: FAIL content=%s\n", buf);
        sys_exit(1);
    }

    /* Remove it; a reopen must then fail. */
    if (sys_unlink("log.txt") != 0 || sys_open("log.txt") >= 0) {
        uprintf("appendtest: FAIL unlink\n");
        sys_exit(1);
    }

    uprintf("appendtest: append+unlink ok\n");
    sys_exit(0);
}
