/* badptr.c - userland: throw hostile pointers (kernel-space, NULL, unmapped)
 * at every pointer-taking syscall and confirm each is rejected with -1. The
 * real proof is implicit: if the kernel ever dereferenced one of these, it
 * would panic on a ring-0 fault and the whole smoke run would die. Reaching
 * the end with everything rejected means user pointers are validated before
 * use. */
#include "ulib.h"

static int gaps;

static void want_fail(int r, const char *what) {
    if (r != -1) {
        uprintf("badptr: GAP %s returned %d\n", what, r);
        gaps++;
    }
}

void _start(void) {
    char *K = (char *)0xC0100000;  /* kernel space */
    char *Z = (char *)0;           /* NULL */
    char *U = (char *)0x07000000;  /* mapped-range but unmapped page */
    unsigned int scratch;

    /* String arguments. */
    want_fail(sys_write(K), "write kernel");
    want_fail(sys_write(Z), "write null");
    want_fail(sys_write(U), "write unmapped");
    want_fail(sys_open(K), "open kernel");
    want_fail(sys_setenv(K, "x"), "setenv kernel name");
    want_fail(sys_mkdir(K), "mkdir kernel");
    want_fail(sys_resolve(K, &scratch), "resolve kernel name");

    /* Writable-buffer arguments. */
    want_fail(sys_sysinfo((struct sysinfo *)K), "sysinfo kernel");
    want_fail(sys_readdir(0, (struct dirent *)K), "readdir kernel");
    want_fail(sys_time((struct systime *)K), "time kernel");
    want_fail(sys_fbinfo((struct fbinfo *)K), "fbinfo kernel");
    want_fail(sys_netinfo((struct netinfo *)K), "netinfo kernel");
    want_fail(sys_getenv("PATH", K, 8), "getenv kernel buf");
    want_fail(sys_readline(K, 8), "readline kernel buf");

    /* Read/write through a real fd but into/from a bad buffer. */
    want_fail(sys_writefd(1, K, 8), "writefd kernel buf");
    int fd = sys_open("notes.txt");
    if (fd >= 0) {
        want_fail(sys_read(fd, K, 8), "read kernel buf");
        sys_close(fd);
    }

    if (gaps == 0)
        uprintf("badptr: all hostile pointers rejected\n");
    else
        uprintf("badptr: %d gaps\n", gaps);
    sys_exit(gaps == 0 ? 0 : 1);
}
