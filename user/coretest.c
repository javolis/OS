/* coretest.c - userland: exercise the coreutils filters through real
 * pipes and check their output. Grows as filters are added; prints one
 * verdict line the smoke test asserts.
 *
 * run_filter spawns `cmd` with its stdin/stdout wired to pipes, sends
 * `input`, collects the output, and compares it to `expected`. Inputs are
 * kept small (well under a pipe buffer) so the write-then-read sequence in
 * a single process never deadlocks. */
#include "ulib.h"

static int passed, failed;

static int run_filter(const char *cmd, const char *input,
                      const char *expected) {
    int pin[2], pout[2];
    if (sys_pipe(pin) != 0 || sys_pipe(pout) != 0)
        return 0;
    int pid = sys_spawn_io(cmd, pin[0], pout[1]);
    sys_close(pin[0]);
    sys_close(pout[1]);
    if (pid < 0) {
        sys_close(pin[1]);
        sys_close(pout[0]);
        return 0;
    }
    sys_writefd(pin[1], input, (int)ustrlen(input));
    sys_close(pin[1]); /* EOF to the filter */

    char buf[256];
    int total = 0, n;
    while (total < (int)sizeof(buf) - 1 &&
           (n = sys_read(pout[0], buf + total, sizeof(buf) - 1 - total)) > 0)
        total += n;
    sys_close(pout[0]);
    sys_wait(pid);
    buf[total] = '\0';
    return ustreq(buf, expected);
}

static void check(const char *cmd, const char *in, const char *exp,
                  const char *name) {
    if (run_filter(cmd, in, exp))
        passed++;
    else {
        failed++;
        uprintf("  FAIL: %s\n", name);
    }
}

void _start(void) {
    check("grep.elf bet", "alpha\nbeta\ngamma\n", "beta\n", "grep");
    check("sort.elf", "banana\napple\ncherry\n", "apple\nbanana\ncherry\n",
          "sort");

    if (failed == 0)
        uprintf("coretest: all ok (%d filters)\n", passed);
    else
        uprintf("coretest: %d FAILED of %d\n", failed, passed + failed);
    sys_exit(failed == 0 ? 0 : 1);
}
