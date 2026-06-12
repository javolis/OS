/* shell.c — tiny interactive shell: line-buffered input with backspace
 * editing on top of keyboard_getchar(), and a few built-in commands. */
#include <stddef.h>
#include <stdint.h>

#include "keyboard.h"
#include "kprintf.h"
#include "pmm.h"
#include "serial.h"
#include "shell.h"
#include "term.h"
#include "timer.h"

#define LINE_MAX 80

static int streq(const char *a, const char *b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

/* Read one line with echo and backspace editing; NUL-terminates buf. */
static void readline(char *buf, size_t size) {
    size_t len = 0;
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
                term_putchar('\b');    /* erases the cell on VGA */
                serial_write("\b \b"); /* erase on a serial terminal */
            }
            continue;
        }
        if (c == '\t')
            c = ' ';
        if (len + 1 < size) {
            buf[len++] = c;
            kprintf("%c", c);
        }
    }
}

void shell_run(void) {
    char line[LINE_MAX];

    kprintf("Tiny shell ready. Type 'help'.\n");
    for (;;) {
        kprintf("> ");
        readline(line, sizeof(line));

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
            kprintf("commands: help echo clear ticks meminfo\n");
        else if (streq(cmd, "echo"))
            kprintf("%s\n", rest);
        else if (streq(cmd, "clear"))
            term_init();
        else if (streq(cmd, "ticks"))
            kprintf("%lu ticks since boot (100 Hz)\n", timer_ticks());
        else if (streq(cmd, "meminfo"))
            kprintf("%lu/%lu frames free (%lu/%lu MiB)\n", pmm_free_frames(),
                    pmm_total_frames(), pmm_free_frames() / 256,
                    pmm_total_frames() / 256);
        else
            kprintf("unknown command: %s (try 'help')\n", cmd);
    }
}
