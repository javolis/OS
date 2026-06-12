/* syscall.h — int 0x80 system call interface.
 * Convention: eax = syscall number (and return value), ebx = first arg. */
#pragma once

#include "idt.h"

#define SYSCALL_VECTOR 0x80

#define SYS_EXIT 0
#define SYS_WRITE 1 /* ebx = NUL-terminated string (user address) */

void syscall_handle(struct registers *regs);
