/* syscall.h — int 0x80 system call interface.
 * Convention: eax = syscall number (and return value), ebx = first arg. */
#pragma once

#include "idt.h"

#define SYSCALL_VECTOR 0x80

#define SYS_EXIT 0     /* ebx = exit code */
#define SYS_WRITE 1    /* ebx = NUL-terminated string (user address) */
#define SYS_SLEEP 2    /* ebx = milliseconds; blocks the task */
#define SYS_GETPID 3   /* returns the caller's pid in eax */
#define SYS_READLINE 4 /* ebx = buffer, ecx = size; foreground task only;
                        * blocks for a line of keyboard input, returns its
                        * length (or -1) */
#define SYS_SPAWN 5    /* ebx = cmdline string (file name + args); ecx = 1
                        * to pass the foreground on (caller must own it);
                        * returns child pid or -1 */
#define SYS_WAIT 6     /* ebx = pid; blocks until it exits, returns its
                        * exit status (and reclaims it) */
#define SYS_SYSINFO 7  /* ebx = struct sysinfo* (user, writable) */
#define SYS_OPEN 8     /* ebx = initrd file name; returns fd or -1 */
#define SYS_READ 9     /* ebx = fd, ecx = buf, edx = n; returns bytes read,
                        * 0 at EOF (console: one edited line, fg only) */
#define SYS_WRITEFD 10  /* ebx = fd, ecx = buf, edx = n; returns n or -1 */
#define SYS_CLOSE 11    /* ebx = fd */
#define SYS_PIPE 12     /* ebx = int[2]; fills {read fd, write fd} */
#define SYS_SPAWN_IO 13 /* ebx = cmdline, ecx = stdin fd, edx = stdout fd
                         * (-1 = console); background; returns pid or -1 */
#define SYS_TIME 14     /* ebx = struct systime* (user, writable) */
#define SYS_CREATE 15   /* ebx = name; create/truncate a ramfs file for
                         * writing; returns fd or -1 */
#define SYS_APPEND 16   /* ebx = name; open (or create) a ramfs file with
                         * the write offset at end-of-file; returns fd/-1 */
#define SYS_UNLINK 17   /* ebx = name; remove a ramfs file; 0 or -1 */
#define SYS_KILL 18     /* ebx = pid; terminate a task; 0 or -1. No
                         * permission model yet: any task may kill any. */
#define SYS_READDIR 19  /* ebx = index, ecx = struct dirent* (writable);
                         * enumerates initrd then ramfs; 0 or -1 at end */
#define SYS_SBRK 20     /* ebx = signed increment; grows the per-process
                         * heap and returns the old break, or -1 */
#define SYS_FBINFO 21   /* ebx = struct fbinfo* (user, writable); fills the
                         * framebuffer geometry. 0 if a framebuffer exists,
                         * -1 on a VGA-text-only boot. Pair with open("/dev/fb")
                         * and writefd to blit raw pixels. */

/* Keep in sync with the userland copy in user/usys.h. */
struct dirent {
    char name[32];
    uint32_t size;
    uint32_t kind; /* 0 = initrd (read-only), 1 = ramfs (read/write) */
};

/* Keep in sync with the userland copy in user/usys.h. */
struct systime {
    uint16_t year;
    uint8_t month, day, hour, minute, second;
};

/* Keep in sync with the userland copy in user/usys.h. */
struct fbinfo {
    uint32_t width;  /* pixels */
    uint32_t height; /* pixels */
    uint32_t pitch;  /* bytes per scanline */
    uint32_t bpp;    /* bits per pixel: 24 or 32 */
};

/* Keep in sync with the userland copy in user/usys.h. */
struct sysinfo {
    uint32_t ticks;        /* PIT ticks since boot (100 Hz) */
    uint32_t free_frames;  /* physical memory */
    uint32_t total_frames;
    uint32_t tasks_alive;  /* live user tasks, caller included */
};

void syscall_handle(struct registers *regs);
