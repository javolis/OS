/* lxtest.c - exercise a broader slice of the Linux i386 syscall ABI
 * (writev, uname, getpid) from a freestanding binary, to verify Avolis's
 * translation layer handles the calls real static programs make at startup.
 * Run with the shell's "linux lxtest.elf". No ulib/usys (native syscalls). */

static long lx(int n, int a, int b, int c) {
    long r;
    __asm__ volatile("int $0x80"
                     : "=a"(r)
                     : "a"(n), "b"(a), "c"(b), "d"(c)
                     : "memory");
    return r;
}
static unsigned slen(const char *s) {
    unsigned n = 0;
    while (s[n])
        n++;
    return n;
}
static void w(const char *s) { lx(4, 1, (int)s, (int)slen(s)); } /* write */

struct iov {
    const char *base;
    unsigned len;
};
static char uts[390]; /* struct utsname */

void _start(void) {
    const char *a = "lxtest: ", *b = "writev works\n";
    struct iov iov[2] = {{a, slen(a)}, {b, slen(b)}};
    lx(146, 1, (int)iov, 2); /* writev */

    if (lx(122, (int)uts, 0, 0) == 0) { /* uname */
        w("lxtest: uname ");
        w(uts);
        w("\n");
    }
    if (lx(20, 0, 0, 0) > 0) /* getpid */
        w("lxtest: getpid ok\n");

    lx(252, 0, 0, 0); /* exit_group */
    for (;;) {
    }
}
