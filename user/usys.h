/* usys.h — userland syscall stubs (int 0x80).
 * Convention: eax = syscall number (and return value), ebx = first arg. */
#pragma once

static inline int sys_write(const char *s) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(1), "b"(s) : "memory");
    return ret;
}

static inline void sys_exit(int code) {
    __asm__ volatile("int $0x80" : : "a"(0), "b"(code));
    __builtin_unreachable();
}

/* Blocks the process (the scheduler runs others meanwhile). */
static inline int sys_sleep(int ms) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(2), "b"(ms));
    return ret;
}

static inline int sys_getpid(void) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(3));
    return ret;
}

/* Read one edited line of keyboard input (foreground process only).
 * Returns the length, or -1. */
static inline int sys_readline(char *buf, int size) {
    int ret;
    __asm__ volatile("int $0x80"
                     : "=a"(ret)
                     : "a"(4), "b"(buf), "c"(size)
                     : "memory");
    return ret;
}

/* Launch an initrd program ("file.elf arg1 arg2..."). Returns its pid. */
static inline int sys_spawn(const char *cmdline) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(5), "b"(cmdline), "c"(0));
    return ret;
}

/* Same, but the child inherits the keyboard (caller must be foreground). */
static inline int sys_spawn_fg(const char *cmdline) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(5), "b"(cmdline), "c"(1));
    return ret;
}

/* Block until the given pid exits. */
static inline int sys_wait(int pid) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(6), "b"(pid));
    return ret;
}

/* Keep in sync with the kernel's struct sysinfo in include/syscall.h. */
struct sysinfo {
    unsigned int ticks; /* PIT ticks since boot (100 Hz) */
    unsigned int free_frames;
    unsigned int total_frames;
    unsigned int tasks_alive;
};

static inline int sys_sysinfo(struct sysinfo *out) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(7), "b"(out) : "memory");
    return ret;
}

/* Open an initrd file read-only. Returns a file descriptor or -1. */
static inline int sys_open(const char *name) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(8), "b"(name));
    return ret;
}

/* Returns bytes read; 0 at EOF. fd 0 reads one edited keyboard line. */
static inline int sys_read(int fd, char *buf, int n) {
    int ret;
    __asm__ volatile("int $0x80"
                     : "=a"(ret)
                     : "a"(9), "b"(fd), "c"(buf), "d"(n)
                     : "memory");
    return ret;
}

static inline int sys_writefd(int fd, const char *buf, int n) {
    int ret;
    __asm__ volatile("int $0x80"
                     : "=a"(ret)
                     : "a"(10), "b"(fd), "c"(buf), "d"(n)
                     : "memory");
    return ret;
}

static inline int sys_close(int fd) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(11), "b"(fd));
    return ret;
}

/* Create a pipe: fds[0] = read end, fds[1] = write end. Returns 0 or -1. */
static inline int sys_pipe(int fds[2]) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(12), "b"(fds) : "memory");
    return ret;
}

/* Spawn a background program with its stdin/stdout wired to the given fds
 * of the calling process (-1 = console). Returns the child pid or -1. */
static inline int sys_spawn_io(const char *cmdline, int in_fd, int out_fd) {
    int ret;
    __asm__ volatile("int $0x80"
                     : "=a"(ret)
                     : "a"(13), "b"(cmdline), "c"(in_fd), "d"(out_fd));
    return ret;
}
