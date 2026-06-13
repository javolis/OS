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

/* --- shell variables: a tiny fixed table, set NAME=VALUE and $NAME --- */
#define VAR_MAX 8
#define VAR_NAME_MAX 16
#define VAR_VAL_MAX 64

static char var_name[VAR_MAX][VAR_NAME_MAX];
static char var_val[VAR_MAX][VAR_VAL_MAX];
static int var_used[VAR_MAX];

static int ident_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_';
}

static const char *var_get(const char *name) {
    for (int i = 0; i < VAR_MAX; i++)
        if (var_used[i] && streq(var_name[i], name))
            return var_val[i];
    return 0;
}

static void var_set(const char *name, const char *val) {
    int slot = -1;
    for (int i = 0; i < VAR_MAX; i++) {
        if (var_used[i] && streq(var_name[i], name)) {
            slot = i;
            break;
        }
        if (slot < 0 && !var_used[i])
            slot = i;
    }
    if (slot < 0)
        return; /* table full */
    var_used[slot] = 1;
    int i = 0;
    while (name[i] && i < VAR_NAME_MAX - 1) {
        var_name[slot][i] = name[i];
        i++;
    }
    var_name[slot][i] = '\0';
    i = 0;
    while (val[i] && i < VAR_VAL_MAX - 1) {
        var_val[slot][i] = val[i];
        i++;
    }
    var_val[slot][i] = '\0';
}

/* Expand $NAME references in src into dst (size dstmax). Unset vars
 * expand to nothing; a lone '$' or '$<non-ident>' is copied verbatim. */
static void expand_vars(const char *src, char *dst, int dstmax) {
    int di = 0;
    for (int i = 0; src[i] && di < dstmax - 1;) {
        if (src[i] == '$' && ident_char(src[i + 1])) {
            i++;
            char name[VAR_NAME_MAX];
            int ni = 0;
            while (ident_char(src[i]) && ni < VAR_NAME_MAX - 1)
                name[ni++] = src[i++];
            name[ni] = '\0';
            const char *v = var_get(name);
            while (v && *v && di < dstmax - 1)
                dst[di++] = *v++;
        } else {
            dst[di++] = src[i++];
        }
    }
    dst[di] = '\0';
}

/* Strip '< in', '> out', and '>> out' redirections from cmd in place,
 * setting the infile and outfile outputs to the filenames (NULL if
 * absent) and *append to 1 for '>>'. Returns 0, or -1 if a redirection
 * operator has no filename. */
static int parse_redir(char *cmd, char **infile, char **outfile,
                       int *append) {
    *infile = 0;
    *outfile = 0;
    *append = 0;

    int i = 0;
    while (cmd[i] && cmd[i] != '<' && cmd[i] != '>')
        i++;
    int cmd_end = i;
    if (!cmd[i])
        return 0; /* no redirection */

    while (cmd[i]) {
        char op = cmd[i];
        if (op != '<' && op != '>') {
            i++;
            continue;
        }
        i++;
        int is_append = 0;
        if (op == '>' && cmd[i] == '>') { /* '>>' append */
            is_append = 1;
            i++;
        }
        while (cmd[i] == ' ')
            i++;
        if (!cmd[i])
            return -1; /* operator with no target */
        char *name = &cmd[i];
        while (cmd[i] && cmd[i] != ' ' && cmd[i] != '<' && cmd[i] != '>')
            i++;
        if (cmd[i])
            cmd[i++] = '\0';
        if (op == '<') {
            *infile = name;
        } else {
            *outfile = name;
            *append = is_append;
        }
    }
    cmd[cmd_end] = '\0'; /* truncate the command before the first operator */
    return 0;
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
    char raw[128];
    char line[256];

    sys_write("ush: user-mode shell (type 'help')\n");
    for (;;) {
        sys_write("ush$ ");
        int rn = sys_readline(raw, sizeof(raw));
        if (rn < 0) {
            sys_write("ush: lost the keyboard, exiting\n");
            sys_exit(0);
        }
        if (rn == 0)
            continue;

        if (streq(raw, "exit")) {
            sys_write("ush: bye\n");
            sys_exit(0);
        }
        if (streq(raw, "help")) {
            sys_write("ush builtins: help, exit, rm <file>, set NAME=VAL, "
                      "kill <pid>\n"
                      "run: <file.elf> [args...] [&]\n"
                      "pipelines: <a> | <b> | <c> ...\n"
                      "redirection: <cmd> > out, >> out (append), < in\n"
                      "variables: set X=val then $X\n");
            continue;
        }
        /* 'set NAME=VALUE' stores a variable (value taken raw). */
        if (raw[0] == 's' && raw[1] == 'e' && raw[2] == 't' &&
            raw[3] == ' ') {
            char *a = trim(raw + 4);
            int e = 0;
            while (a[e] && a[e] != '=')
                e++;
            if (a[e] != '=' || e == 0) {
                sys_write("ush: usage: set NAME=VALUE\n");
            } else {
                a[e] = '\0';
                var_set(a, a + e + 1);
            }
            continue;
        }

        /* Expand $VARs into the working buffer used by everything below. */
        expand_vars(raw, line, sizeof(line));
        int n = 0;
        while (line[n])
            n++;

        if (line[0] == 'r' && line[1] == 'm' && line[2] == ' ') {
            char *f = trim(line + 3);
            if (*f == '\0')
                sys_write("ush: rm needs a filename\n");
            else if (sys_unlink(f) != 0)
                sys_write("ush: rm: no such file\n");
            continue;
        }
        if (line[0] == 'k' && line[1] == 'i' && line[2] == 'l' &&
            line[3] == 'l' && line[4] == ' ') {
            char *a = trim(line + 5);
            int pid = 0, ok = (*a != '\0');
            for (int i = 0; a[i]; i++) {
                if (a[i] < '0' || a[i] > '9') {
                    ok = 0;
                    break;
                }
                pid = pid * 10 + (a[i] - '0');
            }
            if (!ok)
                sys_write("ush: usage: kill <pid>\n");
            else if (sys_kill(pid) != 0)
                sys_write("ush: kill: no such task\n");
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

        /* Redirection: '> out', '>> out' (append), and '< in'. */
        char *infile, *outfile;
        int append;
        if (parse_redir(line, &infile, &outfile, &append) != 0) {
            sys_write("ush: redirection needs a filename\n");
            continue;
        }
        char *cmd0 = trim(line);
        if (*cmd0 == '\0')
            continue;

        if (infile || outfile) {
            int infd = -1, outfd = -1;
            if (infile) {
                infd = sys_open(trim(infile));
                if (infd < 0) {
                    sys_write("ush: cannot open input file\n");
                    continue;
                }
            }
            if (outfile) {
                char *o = trim(outfile);
                outfd = append ? sys_append(o) : sys_create(o);
                if (outfd < 0) {
                    sys_write("ush: cannot create output file\n");
                    if (infd >= 0)
                        sys_close(infd);
                    continue;
                }
            }
            int pid = sys_spawn_io(cmd0, infd, outfd);
            if (infd >= 0)
                sys_close(infd);
            if (outfd >= 0)
                sys_close(outfd);
            if (pid < 0)
                sys_write("ush: cannot run redirected command\n");
            else if (!bg)
                sys_wait(pid);
            continue;
        }

        int pid = bg ? sys_spawn(cmd0) : sys_spawn_fg(cmd0);
        if (pid < 0) {
            sys_write("ush: cannot run: ");
            sys_write(cmd0);
            sys_write("\n");
            continue;
        }
        if (!bg)
            sys_wait(pid); /* keyboard returns to us when the child dies */
    }
}
