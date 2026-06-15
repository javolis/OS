/* lxhello.c - a freestanding program that speaks the LINUX i386 syscall ABI
 * (int 0x80; write=4, exit=1; args in ebx/ecx/edx). Run it with the shell's
 * "linux <file>" command to prove Avolis routes Linux syscalls to its
 * translation layer. It deliberately does NOT use ulib/usys, whose int-0x80
 * stubs carry Avolis's *native* syscall numbers. */

static long lx_write(int fd, const char *buf, unsigned len) {
    long r;
    __asm__ volatile("int $0x80"
                     : "=a"(r)
                     : "a"(4), "b"(fd), "c"(buf), "d"(len)
                     : "memory");
    return r;
}
static void lx_exit(int code) __attribute__((noreturn));
static void lx_exit(int code) {
    __asm__ volatile("int $0x80" : : "a"(1), "b"(code));
    for (;;) {
    }
}

void _start(void) {
    const char *m = "lxhello: hello from the linux abi\n";
    unsigned n = 0;
    while (m[n])
        n++;
    lx_write(1, m, n);
    lx_exit(0);
}
