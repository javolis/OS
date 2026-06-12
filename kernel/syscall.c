/* syscall.c — int 0x80 system call dispatch. */
#include <stdint.h>

#include "kprintf.h"
#include "memlayout.h"
#include "sched.h"
#include "syscall.h"

void syscall_handle(struct registers *regs) {
    switch (regs->eax) {
    case SYS_EXIT:
        task_exit();

    case SYS_WRITE: {
        const char *s = (const char *)regs->ebx;
        /* Reject kernel-space pointers. (Length/page-boundary validation
         * is still missing — fine while the only user program is ours.) */
        if (regs->ebx >= KERNEL_VIRT_BASE) {
            regs->eax = (uint32_t)-1;
            return;
        }
        kprintf("%s", s);
        regs->eax = 0;
        return;
    }

    default:
        regs->eax = (uint32_t)-1;
        return;
    }
}
