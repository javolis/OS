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

/* Keep in sync with the userland copy in user/usys.h. */
struct sysinfo {
    uint32_t ticks;        /* PIT ticks since boot (100 Hz) */
    uint32_t free_frames;  /* physical memory */
    uint32_t total_frames;
    uint32_t tasks_alive;  /* live user tasks, caller included */
};

void syscall_handle(struct registers *regs);
