/* shell.c — tiny interactive shell: line-buffered input with backspace
 * editing and arrow-key history on top of keyboard_getchar(), and a few
 * built-in commands. History lines live on the kernel heap. */
#include <stddef.h>
#include <stdint.h>

#include "initrd.h"
#include "io.h"
#include "keyboard.h"
#include "kheap.h"
#include "kprintf.h"
#include "pmm.h"
#include "process.h"
#include "sched.h"
#include "serial.h"
#include "shell.h"
#include "term.h"
#include "timer.h"

#define LINE_MAX 80
#define HISTORY_MAX 16

static char *history[HISTORY_MAX]; /* oldest first, kmalloc'd copies */
static uint32_t history_len;

static int streq(const char *a, const char *b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

static uint32_t copy_str(char *dst, const char *src, uint32_t max) {
    uint32_t n = 0;
    while (src[n] && n + 1 < max) {
        dst[n] = src[n];
        n++;
    }
    dst[n] = '\0';
    return n;
}

/* Parse an unsigned decimal number; returns 0 on any non-digit input. */
static int parse_u32(const char *s, uint32_t *out) {
    if (!*s)
        return 0;
    uint32_t v = 0;
    for (; *s; s++) {
        if (*s < '0' || *s > '9')
            return 0;
        v = v * 10 + (uint32_t)(*s - '0');
    }
    *out = v;
    return 1;
}

static void history_add(const char *line) {
    if (!*line)
        return;
    if (history_len > 0 && streq(history[history_len - 1], line))
        return; /* skip consecutive duplicates */

    uint32_t n = 0;
    while (line[n])
        n++;
    char *copy = kmalloc(n + 1);
    if (!copy)
        return; /* heap full: silently keep no history for this line */
    copy_str(copy, line, n + 1);

    if (history_len == HISTORY_MAX) {
        kfree(history[0]);
        for (uint32_t i = 1; i < HISTORY_MAX; i++)
            history[i - 1] = history[i];
        history_len--;
    }
    history[history_len++] = copy;
}

static void echo_char(char c) {
    kprintf("%c", c);
}

static void erase_chars(size_t count) {
    uint32_t flags = irq_save(); /* don't interleave with task output */
    for (size_t i = 0; i < count; i++) {
        term_putchar('\b');    /* erases the cell on VGA */
        serial_write("\b \b"); /* erase on a serial terminal */
    }
    irq_restore(flags);
}

/* Read one line with echo, backspace editing, and up/down history
 * recall; NUL-terminates buf. */
static void readline(char *buf, size_t size) {
    size_t len = 0;
    uint32_t hist_pos = history_len; /* == history_len means a fresh line */

    for (;;) {
        char c = keyboard_getchar();

        if (c == '\n') {
            kprintf("\n");
            buf[len] = '\0';
            return;
        }
        if (c == '\b') {
            if (len > 0) {
                len--;
                erase_chars(1);
            }
            continue;
        }
        if (c == KEY_UP || c == KEY_DOWN) {
            if (c == KEY_UP) {
                if (hist_pos == 0)
                    continue;
                hist_pos--;
            } else {
                if (hist_pos == history_len)
                    continue;
                hist_pos++;
            }
            erase_chars(len);
            if (hist_pos == history_len) {
                len = 0; /* back below the newest entry: empty line */
            } else {
                len = copy_str(buf, history[hist_pos], size);
                for (size_t i = 0; i < len; i++)
                    echo_char(buf[i]);
            }
            continue;
        }
        if (c == '\t')
            c = ' ';
        if ((unsigned char)c < 0x80 && len + 1 < size) {
            buf[len++] = c;
            echo_char(c);
        }
    }
}

void shell_run(void) {
    char line[LINE_MAX];

    kprintf("Tiny shell ready. Type 'help'.\n");
    for (;;) {
        sched_reap(); /* collect any tasks that exited since last prompt */
        kprintf("> ");
        readline(line, sizeof(line));
        history_add(line);

        /* Split off the first word; rest points at the arguments, if any. */
        char *cmd = line;
        while (*cmd == ' ')
            cmd++;
        char *rest = cmd;
        while (*rest && *rest != ' ')
            rest++;
        if (*rest) {
            *rest++ = '\0';
            while (*rest == ' ')
                rest++;
        }

        if (*cmd == '\0')
            continue;

        if (streq(cmd, "help"))
            kprintf("commands: help echo clear ticks meminfo sleep uptime "
                    "history ls run ps kill\n");
        else if (streq(cmd, "ps"))
            sched_ps();
        else if (streq(cmd, "ls"))
            initrd_list();
        else if (streq(cmd, "run")) {
            if (*rest == '\0') {
                kprintf("usage: run <file> [args...] [&] (see 'ls')\n");
            } else {
                /* A trailing '&' runs the program in the background;
                 * otherwise the shell waits and the program gets the
                 * keyboard (foreground, Unix-style). */
                int background = 0;
                uint32_t end = 0;
                while (rest[end])
                    end++;
                while (end > 0 && rest[end - 1] == ' ')
                    end--;
                if (end > 0 && rest[end - 1] == '&') {
                    background = 1;
                    end--;
                    while (end > 0 && rest[end - 1] == ' ')
                        end--;
                }
                rest[end] = '\0';

                /* First word is the file name; the full rest (file name
                 * included) becomes the program's argv. */
                char fname[32];
                uint32_t n = 0;
                while (rest[n] && rest[n] != ' ' && n + 1 < sizeof(fname)) {
                    fname[n] = rest[n];
                    n++;
                }
                fname[n] = '\0';

                uint32_t size;
                const char *img = initrd_find(fname, &size);
                if (!img) {
                    kprintf("run: %s: not found (try 'ls')\n", fname);
                } else {
                    /* Foreground is assigned inside spawn, atomically with
                     * the task becoming runnable; exit hands it back. */
                    int pid = process_spawn(img, img + size, rest,
                                            !background);
                    if (pid < 0) {
                        kprintf("run: %s: spawn failed\n", fname);
                    } else if (!background) {
                        while (sched_pid_alive((uint32_t)pid))
                            __asm__ volatile("hlt");
                        sched_reap();
                    }
                }
            }
        } else if (streq(cmd, "kill")) {
            uint32_t pid;
            if (!parse_u32(rest, &pid))
                kprintf("usage: kill <pid> (see 'ps')\n");
            else if (sched_kill(pid) == 0)
                kprintf("killed pid %lu\n", pid);
            else
                kprintf("kill: no such task: %lu\n", pid);
        }
        else if (streq(cmd, "echo"))
            kprintf("%s\n", rest);
        else if (streq(cmd, "clear"))
            term_init();
        else if (streq(cmd, "ticks"))
            kprintf("%lu ticks since boot (100 Hz)\n", timer_ticks());
        else if (streq(cmd, "meminfo")) {
            kprintf("%lu/%lu frames free (%lu/%lu MiB)\n", pmm_free_frames(),
                    pmm_total_frames(), pmm_free_frames() / 256,
                    pmm_total_frames() / 256);
            uint32_t heap_used, heap_total;
            kheap_stats(&heap_used, &heap_total);
            kprintf("heap: %lu of %lu bytes used\n", heap_used, heap_total);
        } else if (streq(cmd, "sleep")) {
            uint32_t ms;
            if (parse_u32(rest, &ms)) {
                timer_sleep(ms);
                kprintf("slept %lu ms\n", ms);
            } else {
                kprintf("usage: sleep <milliseconds>\n");
            }
        } else if (streq(cmd, "uptime")) {
            uint32_t t = timer_ticks();
            kprintf("up %lu.%02lu s (%lu ticks at 100 Hz)\n", t / 100,
                    t % 100, t);
        } else if (streq(cmd, "history")) {
            for (uint32_t i = 0; i < history_len; i++)
                kprintf("%2lu  %s\n", i + 1, history[i]);
        } else
            kprintf("unknown command: %s (try 'help')\n", cmd);
    }
}
