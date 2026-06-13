/* runtests.c - capstone self-test: exercise the syscall surface from ring
 * 3 and print a pass/fail tally. A single program that touches process,
 * file, ramfs, pipe, time, and signal calls end to end. */
#include "ulib.h"

static int passed;
static int failed;

static void check(int cond, const char *name) {
    if (cond) {
        passed++;
    } else {
        failed++;
        uprintf("  FAIL: %s\n", name);
    }
}

void _start(void) {
    /* getpid */
    check(sys_getpid() > 0, "getpid");

    /* sysinfo */
    struct sysinfo si;
    check(sys_sysinfo(&si) == 0 && si.total_frames > 0, "sysinfo");

    /* time */
    struct systime t;
    check(sys_time(&t) == 0 && t.year >= 2020, "time");

    /* ramfs create/write/read/unlink */
    int fd = sys_create("rt_a");
    sys_writefd(fd, "hello", 5);
    sys_close(fd);
    fd = sys_open("rt_a");
    char buf[32];
    int n = sys_read(fd, buf, sizeof(buf) - 1);
    sys_close(fd);
    buf[n > 0 ? n : 0] = '\0';
    check(ustreq(buf, "hello"), "ramfs read-back");
    check(sys_unlink("rt_a") == 0 && sys_open("rt_a") < 0, "ramfs unlink");

    /* append */
    fd = sys_create("rt_b");
    sys_writefd(fd, "A\n", 2);
    sys_close(fd);
    fd = sys_append("rt_b");
    sys_writefd(fd, "B\n", 2);
    sys_close(fd);
    fd = sys_open("rt_b");
    n = sys_read(fd, buf, sizeof(buf) - 1);
    sys_close(fd);
    buf[n > 0 ? n : 0] = '\0';
    check(ustreq(buf, "A\nB\n"), "append");
    sys_unlink("rt_b");

    /* spawn + wait + exit code */
    int pid = sys_spawn("exitcode.elf");
    check(pid > 0 && sys_wait(pid) == 42, "spawn/wait/exit-code");

    /* kill: spawn a sleeper, kill it, wait reports -1 */
    pid = sys_spawn("clock.elf");
    sys_kill(pid);
    check(sys_wait(pid) == -1, "kill");

    /* pipe: write then read within one process (no blocking) */
    int p[2];
    check(sys_pipe(p) == 0, "pipe create");
    sys_writefd(p[1], "xy", 2);
    n = sys_read(p[0], buf, 2);
    buf[n > 0 ? n : 0] = '\0';
    check(ustreq(buf, "xy"), "pipe read-back");
    sys_close(p[0]);
    sys_close(p[1]);

    /* readdir: there is at least one file, and notes.txt is present */
    struct dirent e;
    int found_notes = 0, count = 0;
    for (int i = 0; sys_readdir(i, &e) == 0; i++) {
        count++;
        if (ustreq(e.name, "notes.txt"))
            found_notes = 1;
    }
    check(count > 0 && found_notes, "readdir");

    if (failed == 0)
        uprintf("runtests: ALL PASS (%d tests)\n", passed);
    else
        uprintf("runtests: %d FAILED of %d\n", failed, passed + failed);
    sys_exit(failed == 0 ? 0 : 1);
}
