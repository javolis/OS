/* syscall.h — int 0x80 system call interface.
 * Convention: eax = syscall number (and return value), ebx = first arg. */
#pragma once

#include "idt.h"

#define SYSCALL_VECTOR 0x80

#define SYS_EXIT 0
#define SYS_WRITE 1    /* ebx = NUL-terminated string (user address) */
#define SYS_SLEEP 2    /* ebx = milliseconds; blocks the task */
#define SYS_GETPID 3   /* returns the caller's pid in eax */
#define SYS_READLINE 4 /* ebx = buffer, ecx = size; foreground task only;
                        * blocks for a line of keyboard input, returns its
                        * length (or -1) */

void syscall_handle(struct registers *regs);
