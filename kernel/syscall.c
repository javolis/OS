/* syscall.c — int 0x80 system call dispatch. */
#include <stdint.h>

#include "kprintf.h"
#include "memlayout.h"
#include "syscall.h"

/* boot/usermode.s — resumes the kernel context saved by enter_user_mode. */
extern void kernel_resume(void) __attribute__((noreturn));

void syscall_handle(struct registers *regs) {
    switch (regs->eax) {
    case SYS_EXIT:
        kprintf("[user program exited]\n");
        kernel_resume();

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
