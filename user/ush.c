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

/* Up to 4 stages: each boundary needs a pipe (2 fds) open in ush at once,
 * and ush's 8-slot table already spends 2 on the console. */
#define MAX_STAGES 4

/* Run cmds[0] | cmds[1] | ... | cmds[n-1]: stage i's stdout feeds stage
 * i+1's stdin through a pipe; the first reads the console, the last writes
 * it. ush opens every pipe, spawns every stage (each dups the ends it
 * needs), then drops all its own ends so each writer-closed EOF lands. */
static void run_pipeline(char *cmds[], int n) {
    int pipes[MAX_STAGES - 1][2];
    int np = n - 1;

    for (int i = 0; i < np; i++) {
        if (sys_pipe(pipes[i]) != 0) {
            sys_write("ush: pipe failed\n");
            for (int j = 0; j < i; j++) {
                sys_close(pipes[j][0]);
                sys_close(pipes[j][1]);
            }
            return;
        }
    }

    int pids[MAX_STAGES];
    for (int i = 0; i < n; i++) {
        int in_fd = (i == 0) ? -1 : pipes[i - 1][0];
        int out_fd = (i == n - 1) ? -1 : pipes[i][1];
        pids[i] = sys_spawn_io(cmds[i], in_fd, out_fd);
    }

    for (int i = 0; i < np; i++) {
        sys_close(pipes[i][0]);
        sys_close(pipes[i][1]);
    }
    for (int i = 0; i < n; i++)
        if (pids[i] >= 0)
            sys_wait(pids[i]);
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
                      "pipelines: <a> | <b> | <c> ...\n");
            continue;
        }

        /* Split on '|' into pipeline stages. */
        char *cmds[MAX_STAGES];
        int nstage = 0;
        int ok = 1;
        int start = 0;
        for (int i = 0; i <= n; i++) {
            if (i == n || line[i] == '|') {
                line[i] = '\0';
                if (nstage >= MAX_STAGES) {
                    ok = 0;
                    break;
                }
                cmds[nstage] = trim(line + start);
                if (*cmds[nstage] == '\0') {
                    ok = 0; /* empty stage, e.g. 'a |' or 'a || b' */
                    break;
                }
                nstage++;
                start = i + 1;
            }
        }
        if (!ok) {
            sys_write("ush: malformed pipeline\n");
            continue;
        }
        if (nstage > 1) {
            run_pipeline(cmds, nstage);
            continue;
        }
        /* Single stage: fall through with the trimmed command. */
        {
            char *c = cmds[0];
            int j = 0;
            while (c[j]) {
                line[j] = c[j];
                j++;
            }
            line[j] = '\0';
            n = j;
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
