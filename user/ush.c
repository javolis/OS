/* ush.c — userland shell: a shell that is itself a ring-3 program.
 *
 * Reads lines via sys_readline (it runs as the foreground task), launches
 * initrd programs by name with sys_spawn / sys_spawn_fg, and waits on
 * foreground children — the keyboard comes back automatically because a
 * dying foreground task hands it to its parent. */
#include "usys.h"

static int streq(const char *a, const char *b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

void _start(void) {
    char line[96];

    sys_write("ush: user-mode shell (type 'help')\n");
    for (;;) {
        sys_write("ush$ ");
        int n = sys_readline(line, sizeof(line));
        if (n < 0) {
            sys_write("ush: lost the keyboard, exiting\n");
            sys_exit();
        }
        if (n == 0)
            continue;

        if (streq(line, "exit")) {
            sys_write("ush: bye\n");
            sys_exit();
        }
        if (streq(line, "help")) {
            sys_write("ush builtins: help, exit\n"
                      "anything else runs from the initrd: "
                      "<file.elf> [args...] [&]\n");
            continue;
        }

        /* Trailing '&' runs the program in the background. */
        int bg = 0;
        int end = n;
        while (end > 0 && line[end - 1] == ' ')
            end--;
        if (end > 0 && line[end - 1] == '&') {
            bg = 1;
            end--;
            while (end > 0 && line[end - 1] == ' ')
                end--;
        }
        line[end] = '\0';
        if (end == 0)
            continue;

        int pid = bg ? sys_spawn(line) : sys_spawn_fg(line);
        if (pid < 0) {
            sys_write("ush: cannot run: ");
            sys_write(line);
            sys_write("\n");
            continue;
        }
        if (!bg)
            sys_wait(pid); /* keyboard returns to us when the child dies */
    }
}
