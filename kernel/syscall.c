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

    case SYS_SLEEP: {
        /* 100 Hz -> 10 ms ticks; round up so short sleeps aren't zero. */
        uint32_t nticks = (regs->ebx + 9) / 10;
        sched_sleep_current(nticks ? nticks : 1);
        regs->eax = 0;
        return;
    }

    case SYS_GETPID:
        regs->eax = sched_current_pid();
        return;

    default:
        regs->eax = (uint32_t)-1;
        return;
    }
}
