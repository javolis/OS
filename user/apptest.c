/* apptest.c - app-readiness capstone: one program that leans on the whole
 * userland platform (heap, strings, stdio, files, sbrk) and prints a
 * pass/fail tally. If this is green, the OS can host real apps. */
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
    /* heap: many allocations, write/verify, free, reuse */
    char *blocks[32];
    int heap_ok = 1;
    for (int i = 0; i < 32; i++) {
        blocks[i] = umalloc(128);
        if (!blocks[i]) {
            heap_ok = 0;
            break;
        }
        umemset(blocks[i], 'A' + (i & 15), 128);
    }
    for (int i = 0; i < 32; i++)
        if (blocks[i])
            for (int j = 0; j < 128; j++)
                if (blocks[i][j] != (char)('A' + (i & 15)))
                    heap_ok = 0;
    for (int i = 0; i < 32; i++)
        ufree(blocks[i]);
    check(heap_ok, "heap many-alloc");

    /* a large allocation that forces sbrk to grow the heap */
    char *big = umalloc(50000);
    check(big != 0, "heap large-alloc");
    if (big) {
        umemset(big, 'Z', 50000);
        check(big[0] == 'Z' && big[49999] == 'Z', "heap large-rw");
        ufree(big);
    }

    /* strings */
    char s[64];
    ustrcpy(s, "app");
    ustrcat(s, "-ready");
    check(ustreq(s, "app-ready") && ustrlen(s) == 9, "strings");
    check(uatoi("12345") == 12345, "atoi");

    /* stdio file reader over an initrd file */
    struct ufile *f = ufopen("words.txt");
    check(f != 0, "fopen");
    if (f) {
        char line[32];
        char *r = ufgets(line, sizeof(line), f);
        check(r && ustrncmp(line, "pear", 4) == 0, "fgets");
        ufclose(f);
    }

    if (failed == 0)
        uprintf("apptest: PLATFORM READY (%d checks)\n", passed);
    else
        uprintf("apptest: %d FAILED of %d\n", failed, passed + failed);
    sys_exit(failed == 0 ? 0 : 1);
}
