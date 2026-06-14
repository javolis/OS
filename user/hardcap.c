/* hardcap.c - hardening capstone: roll up this batch's robustness features
 * (ramfs directories, the environment, and hostile-pointer rejection) into
 * one self-checking program, like the graphics and networking capstones.
 * Prints "hardcap: ALL PASS (N tests)" only if every check succeeds. */
#include "ulib.h"

static int pass, total;

static void check(int cond, const char *name) {
    total++;
    if (cond)
        pass++;
    else
        uprintf("hardcap: FAIL %s\n", name);
}

void _start(void) {
    /* ramfs directories: make one, write a file inside it, read it back. */
    check(sys_mkdir("var") == 0, "mkdir");
    int fd = sys_create("var/x");
    int wrote = (fd >= 0) ? sys_writefd(fd, "hi", 2) : -1;
    if (fd >= 0)
        sys_close(fd);
    check(wrote == 2, "write in dir");
    char b[8];
    fd = sys_open("var/x");
    int n = (fd >= 0) ? sys_read(fd, b, sizeof(b)) : -1;
    if (fd >= 0)
        sys_close(fd);
    check(n == 2 && b[0] == 'h' && b[1] == 'i', "read in dir");

    /* Environment: set, read back, then clear. */
    char v[16];
    check(sys_setenv("HK", "HV") == 0, "setenv");
    check(sys_getenv("HK", v, sizeof(v)) == 2 && ustreq(v, "HV"), "getenv");
    check(sys_setenv("HK", "") == 0 && sys_getenv("HK", v, sizeof(v)) == -1,
          "env clear");

    /* Hostile pointers are rejected and the kernel survives (we keep
     * running, which is itself the proof it did not fault). */
    check(sys_sysinfo((struct sysinfo *)0xC0100000) == -1, "reject kernel ptr");
    check(sys_open((const char *)0) == -1, "reject null string");

    if (pass == total)
        uprintf("hardcap: ALL PASS (%d tests)\n", total);
    else
        uprintf("hardcap: %d/%d passed\n", pass, total);
    sys_exit(pass == total ? 0 : 1);
}
