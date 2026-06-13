/* devtest.c - userland: verify /dev/null (discards writes, reads EOF) and
 * /dev/zero (reads endless zero bytes). */
#include "ulib.h"

void _start(void) {
    /* /dev/null: write succeeds (discarded), read returns EOF. */
    int fd = sys_open("/dev/null");
    if (fd < 0) {
        uprintf("devtest: FAIL open null\n");
        sys_exit(1);
    }
    int w = sys_writefd(fd, "ignored", 7);
    char buf[8];
    int r = sys_read(fd, buf, sizeof(buf));
    sys_close(fd);
    if (w != 7 || r != 0) {
        uprintf("devtest: FAIL null w=%d r=%d\n", w, r);
        sys_exit(1);
    }

    /* /dev/zero: read returns zero bytes (count = request). */
    fd = sys_open("/dev/zero");
    char z[16];
    for (int i = 0; i < 16; i++)
        z[i] = 0x55; /* poison so we can see it overwritten */
    r = sys_read(fd, z, 16);
    sys_close(fd);
    int all_zero = (r == 16);
    for (int i = 0; i < 16; i++)
        if (z[i] != 0)
            all_zero = 0;
    if (!all_zero) {
        uprintf("devtest: FAIL zero r=%d\n", r);
        sys_exit(1);
    }

    uprintf("devtest: dev files ok\n");
    sys_exit(0);
}
