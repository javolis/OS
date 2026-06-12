/* syscall.c — int 0x80 system call dispatch. */
#include <stdint.h>

#include "initrd.h"
#include "keyboard.h"
#include "kprintf.h"
#include "memlayout.h"
#include "paging.h"
#include "process.h"
#include "sched.h"
#include "serial.h"
#include "syscall.h"
#include "term.h"

#define SYS_WRITE_MAX 1024
#define SPAWN_CMDLINE_MAX 128

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

/* Every page of [addr, addr+len) must be a present, writable, user page
 * in the caller's address space. */
static int user_range_writable(uint32_t addr, uint32_t len) {
    if (len == 0 || addr + len < addr || addr + len > KERNEL_VIRT_BASE)
        return 0;
    uint32_t dir = paging_active_directory();
    for (uint32_t a = addr & ~0xFFFu; a < addr + len; a += 4096) {
        uint32_t pte = paging_get_pte(dir, a);
        if (!(pte & 0x1) || !(pte & 0x2) || !(pte & 0x4))
            return 0; /* present | writable | user */
    }
    return 1;
}

/* Next keyboard character for a syscall context: blocks the TASK (not the
 * CPU) when the buffer is empty. IF is off here, so the check-then-block
 * sequence cannot lose a wakeup. */
static char task_getchar(void) {
    for (;;) {
        int c = keyboard_trygetchar();
        if (c >= 0)
            return (char)c;
        sched_block_on_keyboard();
    }
}

/* Line-disciplined read into a user buffer: echo, backspace editing,
 * returns on Enter. The caller's address space is active, so the buffer
 * is written through its own user mapping. */
static uint32_t do_readline(char *dst, uint32_t max) {
    uint32_t len = 0;
    for (;;) {
        char c = task_getchar();
        if (c == '\n') {
            kprintf("\n");
            break;
        }
        if (c == '\b') {
            if (len > 0) {
                len--;
                term_putchar('\b');
                serial_write("\b \b");
            }
            continue;
        }
        if ((unsigned char)c >= 0x80)
            continue; /* arrows etc.: no history in user readline yet */
        if (len + 1 < max) {
            dst[len++] = c;
            kprintf("%c", c);
        }
    }
    dst[len] = '\0';
    return len;
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

    case SYS_READLINE: {
        /* Only the foreground task may read the keyboard. */
        if (sched_current_pid() != sched_foreground_pid() ||
            !user_range_writable(regs->ebx, regs->ecx)) {
            regs->eax = (uint32_t)-1;
            return;
        }
        regs->eax = do_readline((char *)regs->ebx, regs->ecx);
        return;
    }

    case SYS_SPAWN: {
        if (!user_string_ok(regs->ebx)) {
            regs->eax = (uint32_t)-1;
            return;
        }
        /* Copy to a kernel buffer; the string lives in the caller's
         * address space and process_spawn re-reads it repeatedly. */
        char cmdline[SPAWN_CMDLINE_MAX];
        const char *src = (const char *)regs->ebx;
        uint32_t n = 0;
        while (src[n] && n + 1 < sizeof(cmdline)) {
            cmdline[n] = src[n];
            n++;
        }
        cmdline[n] = '\0';

        char fname[32];
        n = 0;
        while (cmdline[n] && cmdline[n] != ' ' && n + 1 < sizeof(fname)) {
            fname[n] = cmdline[n];
            n++;
        }
        fname[n] = '\0';

        uint32_t size;
        const char *img = initrd_find(fname, &size);
        if (!img) {
            regs->eax = (uint32_t)-1;
            return;
        }
        int pid = process_spawn(img, img + size, cmdline);
        if (pid > 0 && regs->ecx == 1 &&
            sched_current_pid() == sched_foreground_pid())
            sched_set_foreground((uint32_t)pid);
        regs->eax = (uint32_t)pid;
        return;
    }

    case SYS_WAIT:
        sched_wait_pid(regs->ebx);
        regs->eax = 0;
        return;

    default:
        regs->eax = (uint32_t)-1;
        return;
    }
}
