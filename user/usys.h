/* usys.h — userland syscall stubs (int 0x80).
 * Convention: eax = syscall number (and return value), ebx = first arg. */
#pragma once

static inline int sys_write(const char *s) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(1), "b"(s) : "memory");
    return ret;
}

static inline void sys_exit(void) {
    __asm__ volatile("int $0x80" : : "a"(0));
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
