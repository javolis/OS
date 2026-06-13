/* ulibtest.c - userland: verify the ulib mem/string suite. */
#include "ulib.h"

static int passed, failed;

static void check(int cond, const char *name) {
    if (cond)
        passed++;
    else {
        failed++;
        uprintf("  FAIL: %s\n", name);
    }
}

void _start(void) {
    char buf[32];

    umemset(buf, 'x', 4);
    buf[4] = '\0';
    check(ustreq(buf, "xxxx"), "memset");

    umemcpy(buf, "abcd", 5);
    check(ustreq(buf, "abcd"), "memcpy");

    ustrcpy(buf, "hello");
    check(ustreq(buf, "hello") && ustrlen(buf) == 5, "strcpy");

    ustrcat(buf, "!");
    check(ustreq(buf, "hello!"), "strcat");

    check(ustrcmp("abc", "abc") == 0 && ustrcmp("abc", "abd") < 0, "strcmp");
    check(ustrncmp("abcXX", "abcYY", 3) == 0, "strncmp");
    check(ustrchr("a.b.c", '.') != 0 && ustrchr("abc", 'z') == 0, "strchr");
    check(uatoi("-42") == -42 && uatoi("  17") == 17, "atoi");

    char ov[8];
    umemset(ov, 'A', 8);
    umemmove(ov + 1, ov, 4); /* overlapping forward */
    check(ov[0] == 'A' && ov[4] == 'A', "memmove");

    if (failed == 0)
        uprintf("ulibtest: all ok (%d)\n", passed);
    else
        uprintf("ulibtest: %d FAILED\n", failed);
    sys_exit(failed == 0 ? 0 : 1);
}
