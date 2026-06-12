/* syscall.c — int 0x80 system call dispatch. */
#include <stdint.h>

#include "kprintf.h"
#include "memlayout.h"
#include "paging.h"
#include "sched.h"
#include "syscall.h"

#define SYS_WRITE_MAX 1024

/* Validate a user string: every byte up to the NUL must lie below the
 * kernel base on a mapped page of the calling task's address space
 * (syscalls run with the caller's CR3 loaded), within a length cap.
 * Returns 1 if the string is safe to read. */
static int user_string_ok(uint32_t addr) {
    uint32_t dir = paging_active_directory();
    const char *s = (const char *)addr;

    for (uint32_t i = 0; i < SYS_WRITE_MAX; i++) {
        uint32_t a = addr + i;
        if (a < addr || a >= KERNEL_VIRT_BASE)
            return 0; /* wrapped or reached kernel space */
        if ((i == 0 || (a & 0xFFF) == 0) && !paging_get_phys(dir, a))
            return 0; /* unmapped page */
        if (s[i] == '\0')
            return 1;
    }
    return 0; /* no NUL within the cap */
}

void syscall_handle(struct registers *regs) {
    switch (regs->eax) {
    case SYS_EXIT:
        task_exit();

    case SYS_WRITE: {
        if (!user_string_ok(regs->ebx)) {
            regs->eax = (uint32_t)-1;
            return;
        }
        kprintf("%s", (const char *)regs->ebx);
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
