/* usys.h — userland syscall stubs (int 0x80).
 * Convention: eax = syscall number (and return value), ebx = first arg. */
#pragma once
#include <stdint.h>

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

/* Open a file (ramfs first, then read-only initrd). Returns an fd or -1. */
static inline int sys_open(const char *name) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(8), "b"(name));
    return ret;
}

/* Create or truncate a writable ramfs file. Returns a write fd or -1. */
static inline int sys_create(const char *name) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(15), "b"(name));
    return ret;
}

/* Open (or create) a ramfs file for appending. Returns a write fd or -1. */
static inline int sys_append(const char *name) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(16), "b"(name));
    return ret;
}

/* Remove a ramfs file. Returns 0 or -1. */
static inline int sys_unlink(const char *name) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(17), "b"(name));
    return ret;
}

/* Terminate a task by pid. Returns 0 or -1. */
static inline int sys_kill(int pid) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(18), "b"(pid));
    return ret;
}

/* Keep in sync with the kernel's struct dirent in include/syscall.h. */
struct dirent {
    char name[32];
    unsigned int size;
    unsigned int kind; /* 0 = initrd (ro), 1 = ramfs file, 2 = ramfs dir */
};

/* Create a ramfs directory marker. Returns 0, or -1 if it exists/is full. */
static inline int sys_mkdir(const char *path) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(31), "b"(path));
    return ret;
}

/* Set a global environment variable (empty value clears it). 0 or -1. */
static inline int sys_setenv(const char *name, const char *val) {
    int ret;
    __asm__ volatile("int $0x80"
                     : "=a"(ret)
                     : "a"(32), "b"(name), "c"(val)
                     : "memory");
    return ret;
}

/* Copy an environment variable's value into buf. Returns its length, or -1
 * if unset. */
static inline int sys_getenv(const char *name, char *buf, int max) {
    int ret;
    __asm__ volatile("int $0x80"
                     : "=a"(ret)
                     : "a"(33), "b"(name), "c"(buf), "d"(max)
                     : "memory");
    return ret;
}

/* Enumerate files by index (initrd then ramfs). 0 on success, -1 at end. */
static inline int sys_readdir(int index, struct dirent *out) {
    int ret;
    __asm__ volatile("int $0x80"
                     : "=a"(ret)
                     : "a"(19), "b"(index), "c"(out)
                     : "memory");
    return ret;
}

/* Grow the heap by `incr` bytes; returns the old break, or (void*)-1. */
static inline void *sys_sbrk(int incr) {
    void *ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(20), "b"(incr));
    return ret;
}

/* Keep in sync with the kernel's struct fbinfo in include/syscall.h. */
struct fbinfo {
    unsigned int width;  /* pixels */
    unsigned int height; /* pixels */
    unsigned int pitch;  /* bytes per scanline */
    unsigned int bpp;    /* bits per pixel: 24 or 32 */
};

/* Query the framebuffer geometry. Returns 0 if a framebuffer is present
 * (then open "/dev/fb" and writefd raw pixel bytes to it), -1 if the boot
 * console is VGA text only. */
static inline int sys_fbinfo(struct fbinfo *out) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(21), "b"(out) : "memory");
    return ret;
}

/* Read one raw key (no echo or line editing); foreground process only.
 * Returns 0..255, or -1 if not the foreground task. Blocks until a key
 * arrives. Arrow keys arrive as 0x80 (up) / 0x81 (down). */
static inline int sys_getkey(void) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(22));
    return ret;
}

/* Non-blocking key read for UI loops: next raw key 0..255, or -1 if none
 * is waiting. */
static inline int sys_trygetkey(void) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(34));
    return ret;
}

#define MOUSE_LEFT 0x01
#define MOUSE_RIGHT 0x02
#define MOUSE_MIDDLE 0x04

/* Keep in sync with the kernel's struct mousestate in include/syscall.h. */
struct mousestate {
    int x;
    int y;
    unsigned int buttons;
};

/* Read the cursor position + button bitmask. 0 on success, -1 if no mouse. */
static inline int sys_mouse(struct mousestate *out) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(35), "b"(out) : "memory");
    return ret;
}

/* Play a square-wave tone of `freq` Hz on the PC speaker for `ms` ms, then
 * silence. freq 0 silences immediately. Blocks the caller for the duration
 * (the scheduler runs others meanwhile). Returns 0. */
static inline int sys_beep(int freq, int ms) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(36), "b"(freq), "c"(ms));
    return ret;
}

/* Play `count` 16-bit PCM samples (interleaved L,R, 48 kHz stereo) through
 * the AC'97 codec. Blocks until the DMA drains. Returns the number of
 * samples played, or -1 if there is no audio device. */
static inline int sys_audio(const int16_t *samples, int count) {
    int ret;
    __asm__ volatile("int $0x80"
                     : "=a"(ret)
                     : "a"(37), "b"(samples), "c"(count)
                     : "memory");
    return ret;
}

/* Capture `count` 16-bit PCM samples from the AC'97 input into `buf`. Blocks
 * until the DMA fills it. Returns the number captured, or -1 if no device. */
static inline int sys_audio_record(int16_t *buf, int count) {
    int ret;
    __asm__ volatile("int $0x80"
                     : "=a"(ret)
                     : "a"(38), "b"(buf), "c"(count)
                     : "memory");
    return ret;
}

/* Keep in sync with the kernel's struct procinfo in include/syscall.h. */
struct procinfo {
    uint32_t pid;
    uint32_t state; /* 1 ready 2 blocked 3 waitkbd 4 waitpid 5 waitchan
                     * 6 zombie 7 running */
    char name[16];
};

/* Fill *out with the index-th task in the table. Returns 0, or -1 past the
 * end (enumerate by calling with index 0, 1, 2, ... until it returns -1). */
static inline int sys_ps(int index, struct procinfo *out) {
    int ret;
    __asm__ volatile("int $0x80"
                     : "=a"(ret)
                     : "a"(39), "b"(index), "c"(out)
                     : "memory");
    return ret;
}

/* Set the AC'97 master volume (0-100). Returns the clamped level, or -1 if
 * there is no audio device. */
static inline int sys_audio_volume(int level) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(40), "b"(level));
    return ret;
}

/* Power the machine off / restart it. Neither returns. */
static inline void sys_poweroff(void) {
    __asm__ volatile("int $0x80" : : "a"(41));
}
static inline void sys_reboot(void) {
    __asm__ volatile("int $0x80" : : "a"(42));
}

/* Send an ICMP echo to an IPv4 address (host byte order: a<<24|b<<16|c<<8|d)
 * and wait for the reply. Returns the round-trip time in ms, or -1. */
static inline int sys_ping(unsigned int ip) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(23), "b"(ip));
    return ret;
}

/* DNS-resolve a hostname; writes the IPv4 address (host order) to *out.
 * Returns 0 on success, -1 on failure. */
static inline int sys_resolve(const char *name, unsigned int *out) {
    int ret;
    __asm__ volatile("int $0x80"
                     : "=a"(ret)
                     : "a"(24), "b"(name), "c"(out)
                     : "memory");
    return ret;
}

/* Run DHCP and apply the lease. Returns the leased IPv4 address (host
 * order), or 0 on failure. */
static inline unsigned int sys_dhcp(void) {
    unsigned int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(25));
    return ret;
}

/* TCP client (one connection at a time). */
static inline int sys_tcp_connect(unsigned int ip, int port) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(26), "b"(ip), "c"(port));
    return ret;
}
static inline int sys_tcp_send(const void *buf, int len) {
    int ret;
    __asm__ volatile("int $0x80"
                     : "=a"(ret)
                     : "a"(27), "b"(buf), "c"(len)
                     : "memory");
    return ret;
}
static inline int sys_tcp_recv(void *buf, int max) {
    int ret;
    __asm__ volatile("int $0x80"
                     : "=a"(ret)
                     : "a"(28), "b"(buf), "c"(max)
                     : "memory");
    return ret;
}
static inline int sys_tcp_close(void) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(29));
    return ret;
}

/* Keep in sync with the kernel's struct netinfo in include/syscall.h. */
struct netinfo {
    unsigned int ip;
    unsigned int gateway;
    unsigned int netmask;
    unsigned int dns;
};

/* Read the current IPv4 configuration (host byte order). 0 if networking is
 * up, -1 if there is no NIC. */
static inline int sys_netinfo(struct netinfo *out) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(30), "b"(out) : "memory");
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

/* Keep in sync with the kernel's struct systime in include/syscall.h. */
struct systime {
    unsigned short year;
    unsigned char month, day, hour, minute, second;
};

static inline int sys_time(struct systime *out) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(14), "b"(out) : "memory");
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
