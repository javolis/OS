/* syscall.c — int 0x80 system call dispatch. */
#include <stddef.h>
#include <stdint.h>

#include "file.h"
#include "initrd.h"
#include "io.h"
#include "keyboard.h"
#include "kprintf.h"
#include "memlayout.h"
#include "paging.h"
#include "pipe.h"
#include "pmm.h"
#include "process.h"
#include "ramfs.h"
#include "rtc.h"
#include "sched.h"
#include "serial.h"
#include "syscall.h"
#include "term.h"
#include "timer.h"

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

/* Every page of [addr, addr+len) must be a present user page in the
 * caller's address space — writable too when need_write is set. */
static int user_range_ok(uint32_t addr, uint32_t len, int need_write) {
    if (len == 0 || addr + len < addr || addr + len > KERNEL_VIRT_BASE)
        return 0;
    uint32_t dir = paging_active_directory();
    for (uint32_t a = addr & ~0xFFFu; a < addr + len; a += 4096) {
        uint32_t pte = paging_get_pte(dir, a);
        if (!(pte & 0x1) || !(pte & 0x4))
            return 0; /* present | user */
        if (need_write && !(pte & 0x2))
            return 0;
    }
    return 1;
}

#define user_range_writable(addr, len) user_range_ok(addr, len, 1)

/* Copy a validated user filename into a kernel buffer. Returns 0 on
 * success, -1 if the pointer is bad. */
static int copy_user_name(uint32_t addr, char *dst, uint32_t max) {
    if (!user_string_ok(addr))
        return -1;
    const char *src = (const char *)addr;
    uint32_t i = 0;
    while (src[i] && i + 1 < max) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
    return 0;
}

/* Raw byte write to the screen + serial, atomically. */
static void console_write(const char *s, uint32_t n) {
    uint32_t fl = irq_save();
    for (uint32_t i = 0; i < n; i++) {
        term_putchar(s[i]);
        serial_putchar(s[i]);
    }
    irq_restore(fl);
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

/* Shared by SYS_SPAWN and SYS_SPAWN_IO: copy the cmdline from user space,
 * resolve the program in the initrd, and spawn it with the given stdio.
 * Returns the child pid or -1. */
static int spawn_from_user(uint32_t cmdline_uaddr, int make_fg,
                           struct file *in, struct file *out) {
    if (!user_string_ok(cmdline_uaddr))
        return -1;

    char cmdline[SPAWN_CMDLINE_MAX];
    const char *src = (const char *)cmdline_uaddr;
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
    if (!img)
        return -1;
    return process_spawn(img, img + size, cmdline, make_fg, in, out);
}

void syscall_handle(struct registers *regs) {
    switch (regs->eax) {
    case SYS_EXIT:
        task_exit(regs->ebx);

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
        /* Foreground handoff happens inside spawn, atomically with the
         * child becoming runnable — it may run before we return. */
        int make_fg = (regs->ecx == 1 &&
                       sched_current_pid() == sched_foreground_pid());
        regs->eax = (uint32_t)spawn_from_user(regs->ebx, make_fg, NULL, NULL);
        return;
    }

    case SYS_SPAWN_IO: {
        /* ecx = stdin fd, edx = stdout fd in the CALLER's table (-1 =
         * console). The child runs in the background. */
        struct file *in = NULL, *out = NULL;
        if ((int)regs->ecx >= 0) {
            in = sched_get_fd((int)regs->ecx);
            if (!in) {
                regs->eax = (uint32_t)-1;
                return;
            }
        }
        if ((int)regs->edx >= 0) {
            out = sched_get_fd((int)regs->edx);
            if (!out) {
                regs->eax = (uint32_t)-1;
                return;
            }
        }
        regs->eax = (uint32_t)spawn_from_user(regs->ebx, 0, in, out);
        return;
    }

    case SYS_PIPE: {
        if (!user_range_writable(regs->ebx, 2 * sizeof(int))) {
            regs->eax = (uint32_t)-1;
            return;
        }
        struct file *rd, *wr;
        if (pipe_create(&rd, &wr) != 0) {
            regs->eax = (uint32_t)-1;
            return;
        }
        int rfd = sched_install_fd(rd);
        int wfd = sched_install_fd(wr);
        if (rfd < 0 || wfd < 0) {
            if (rfd >= 0)
                sched_clear_fd(rfd);
            if (wfd >= 0)
                sched_clear_fd(wfd);
            file_unref(rd);
            file_unref(wr);
            regs->eax = (uint32_t)-1;
            return;
        }
        int *out = (int *)regs->ebx;
        out[0] = rfd;
        out[1] = wfd;
        regs->eax = 0;
        return;
    }

    case SYS_WAIT: {
        uint32_t status;
        sched_wait_pid(regs->ebx, &status);
        regs->eax = status;
        return;
    }

    case SYS_OPEN: {
        char name[RAMFS_NAME_MAX];
        if (copy_user_name(regs->ebx, name, sizeof(name)) != 0) {
            regs->eax = (uint32_t)-1;
            return;
        }
        /* ramfs takes precedence over the read-only initrd. */
        struct ramfs_file *rf = ramfs_find(name);
        struct file *f;
        if (rf) {
            f = file_alloc(FILE_RAMFS);
            if (f)
                f->rfile = rf;
        } else {
            uint32_t size;
            const void *data = initrd_find(name, &size);
            if (!data) {
                regs->eax = (uint32_t)-1;
                return;
            }
            f = file_alloc(FILE_INITRD);
            if (f) {
                f->data = data;
                f->size = size;
            }
        }
        if (!f) {
            regs->eax = (uint32_t)-1;
            return;
        }
        int fd = sched_install_fd(f);
        if (fd < 0)
            file_unref(f);
        regs->eax = (uint32_t)fd;
        return;
    }

    case SYS_CREATE: {
        char name[RAMFS_NAME_MAX];
        if (copy_user_name(regs->ebx, name, sizeof(name)) != 0) {
            regs->eax = (uint32_t)-1;
            return;
        }
        struct ramfs_file *rf = ramfs_create(name);
        if (!rf) {
            regs->eax = (uint32_t)-1;
            return;
        }
        struct file *f = file_alloc(FILE_RAMFS);
        if (!f) {
            regs->eax = (uint32_t)-1;
            return;
        }
        f->rfile = rf;
        int fd = sched_install_fd(f);
        if (fd < 0)
            file_unref(f);
        regs->eax = (uint32_t)fd;
        return;
    }

    case SYS_APPEND: {
        char name[RAMFS_NAME_MAX];
        if (copy_user_name(regs->ebx, name, sizeof(name)) != 0) {
            regs->eax = (uint32_t)-1;
            return;
        }
        /* Open the existing file (preserving contents) or create it. */
        struct ramfs_file *rf = ramfs_find(name);
        if (!rf)
            rf = ramfs_create(name);
        if (!rf) {
            regs->eax = (uint32_t)-1;
            return;
        }
        struct file *f = file_alloc(FILE_RAMFS);
        if (!f) {
            regs->eax = (uint32_t)-1;
            return;
        }
        f->rfile = rf;
        f->offset = rf->size; /* writes land at end-of-file */
        int fd = sched_install_fd(f);
        if (fd < 0)
            file_unref(f);
        regs->eax = (uint32_t)fd;
        return;
    }

    case SYS_UNLINK: {
        char name[RAMFS_NAME_MAX];
        if (copy_user_name(regs->ebx, name, sizeof(name)) != 0) {
            regs->eax = (uint32_t)-1;
            return;
        }
        regs->eax = (uint32_t)ramfs_unlink(name);
        return;
    }

    case SYS_READ: {
        struct file *f = sched_get_fd((int)regs->ebx);
        uint32_t buf = regs->ecx, n = regs->edx;
        if (!f || !user_range_writable(buf, n)) {
            regs->eax = (uint32_t)-1;
            return;
        }
        if (f->kind == FILE_CONSOLE) {
            /* One edited line; same foreground rule as SYS_READLINE. */
            if (sched_current_pid() != sched_foreground_pid()) {
                regs->eax = (uint32_t)-1;
                return;
            }
            regs->eax = do_readline((char *)buf, n);
            return;
        }
        if (f->kind == FILE_PIPE_READ) {
            regs->eax = (uint32_t)pipe_read(f, (uint8_t *)buf, n);
            return;
        }
        if (f->kind == FILE_PIPE_WRITE) {
            regs->eax = (uint32_t)-1; /* wrong end */
            return;
        }
        /* FILE_INITRD / FILE_RAMFS: byte stream by offset, 0 at EOF. */
        const uint8_t *fdata;
        uint32_t fsize;
        if (f->kind == FILE_RAMFS) {
            fdata = f->rfile->data;
            fsize = f->rfile->size;
        } else {
            fdata = f->data;
            fsize = f->size;
        }
        uint32_t left = (f->offset < fsize) ? fsize - f->offset : 0;
        uint32_t take = n < left ? n : left;
        uint8_t *dst = (uint8_t *)buf;
        for (uint32_t i = 0; i < take; i++)
            dst[i] = fdata[f->offset + i];
        f->offset += take;
        regs->eax = take; /* 0 = EOF */
        return;
    }

    case SYS_WRITEFD: {
        struct file *f = sched_get_fd((int)regs->ebx);
        uint32_t buf = regs->ecx, n = regs->edx;
        if (!f || n > SYS_WRITE_MAX || !user_range_ok(buf, n, 0)) {
            regs->eax = (uint32_t)-1;
            return;
        }
        if (f->kind == FILE_PIPE_WRITE) {
            regs->eax = (uint32_t)pipe_write(f, (const uint8_t *)buf, n);
            return;
        }
        if (f->kind == FILE_RAMFS) {
            int w = ramfs_write(f->rfile, f->offset, (const uint8_t *)buf, n);
            if (w >= 0)
                f->offset += (uint32_t)w;
            regs->eax = (uint32_t)w;
            return;
        }
        if (f->kind != FILE_CONSOLE) {
            regs->eax = (uint32_t)-1; /* initrd read-only, pipe read end */
            return;
        }
        console_write((const char *)buf, n);
        regs->eax = n;
        return;
    }

    case SYS_CLOSE: {
        struct file *f = sched_get_fd((int)regs->ebx);
        if (!f) {
            regs->eax = (uint32_t)-1;
            return;
        }
        sched_clear_fd((int)regs->ebx);
        file_unref(f);
        regs->eax = 0;
        return;
    }

    case SYS_SYSINFO: {
        if (!user_range_writable(regs->ebx, sizeof(struct sysinfo))) {
            regs->eax = (uint32_t)-1;
            return;
        }
        struct sysinfo *si = (struct sysinfo *)regs->ebx;
        si->ticks = timer_ticks();
        si->free_frames = pmm_free_frames();
        si->total_frames = pmm_total_frames();
        si->tasks_alive = sched_alive_count();
        regs->eax = 0;
        return;
    }

    case SYS_TIME: {
        if (!user_range_writable(regs->ebx, sizeof(struct systime))) {
            regs->eax = (uint32_t)-1;
            return;
        }
        struct rtc_time t;
        rtc_read(&t);
        struct systime *st = (struct systime *)regs->ebx;
        st->year = t.year;
        st->month = t.month;
        st->day = t.day;
        st->hour = t.hour;
        st->minute = t.minute;
        st->second = t.second;
        regs->eax = 0;
        return;
    }

    default:
        regs->eax = (uint32_t)-1;
        return;
    }
}
