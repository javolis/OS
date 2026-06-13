/* ush.c - userland shell: a shell that is itself a ring-3 program.
 *
 * Reads lines via sys_readline (it runs as the foreground task), launches
 * initrd programs by name with sys_spawn / sys_spawn_fg, and waits on
 * foreground children - the keyboard comes back automatically because a
 * dying foreground task hands it to its parent. */
#include "usys.h"

static int streq(const char *a, const char *b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

/* Trim leading/trailing spaces in place, returning the start. */
static char *trim(char *s) {
    while (*s == ' ')
        s++;
    int e = 0;
    while (s[e])
        e++;
    while (e > 0 && s[e - 1] == ' ')
        s[--e] = '\0';
    return s;
}

/* Run "left | right": left's stdout and right's stdin share a pipe. ush
 * drops both ends after spawning so the writer-closed EOF reaches right. */
static void run_pipeline(char *left, char *right) {
    int p[2];
    if (sys_pipe(p) != 0) {
        sys_write("ush: pipe failed\n");
        return;
    }
    int pl = sys_spawn_io(left, -1, p[1]);  /* stdout -> pipe write */
    int pr = sys_spawn_io(right, p[0], -1); /* stdin  <- pipe read */
    sys_close(p[0]);
    sys_close(p[1]);
    if (pl < 0 || pr < 0) {
        sys_write("ush: cannot run pipeline\n");
        return;
    }
    sys_wait(pl);
    sys_wait(pr);
}

void _start(void) {
    char line[96];

    sys_write("ush: user-mode shell (type 'help')\n");
    for (;;) {
        sys_write("ush$ ");
        int n = sys_readline(line, sizeof(line));
        if (n < 0) {
            sys_write("ush: lost the keyboard, exiting\n");
            sys_exit(0);
        }
        if (n == 0)
            continue;

        if (streq(line, "exit")) {
            sys_write("ush: bye\n");
            sys_exit(0);
        }
        if (streq(line, "help")) {
            sys_write("ush builtins: help, exit\n"
                      "run initrd programs: <file.elf> [args...] [&]\n"
                      "pipelines: <a> | <b>\n");
            continue;
        }

        /* A single '|' splits the line into a two-stage pipeline. */
        int bar = -1;
        for (int i = 0; i < n; i++) {
            if (line[i] == '|') {
                bar = i;
                break;
            }
        }
        if (bar >= 0) {
            line[bar] = '\0';
            char *left = trim(line);
            char *right = trim(line + bar + 1);
            if (*left && *right)
                run_pipeline(left, right);
            else
                sys_write("ush: malformed pipeline\n");
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
