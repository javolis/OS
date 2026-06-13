/* malloctest.c - userland: exercise umalloc/ufree, including reuse and a
 * large allocation that forces the heap to grow via sbrk. */
#include "ulib.h"

void _start(void) {
    int ok = 1;

    /* basic alloc + write + free */
    char *a = umalloc(100);
    char *b = umalloc(200);
    if (!a || !b || a == b)
        ok = 0;
    umemset(a, 'A', 100);
    umemset(b, 'B', 200);
    for (int i = 0; i < 100; i++)
        if (a[i] != 'A')
            ok = 0;
    for (int i = 0; i < 200; i++)
        if (b[i] != 'B')
            ok = 0;

    /* free a, re-allocate a similar size: should reuse the freed block */
    ufree(a);
    char *c = umalloc(80);
    if (c != a)
        ok = 0; /* first-fit reuse of the just-freed block */

    /* a large allocation forces multiple sbrk-backed pages */
    char *big = umalloc(20000);
    if (!big)
        ok = 0;
    umemset(big, 'Z', 20000);
    if (big[0] != 'Z' || big[19999] != 'Z')
        ok = 0;
    ufree(big);
    ufree(b);
    ufree(c);

    uprintf(ok ? "malloctest: heap ok\n" : "malloctest: FAIL\n");
    sys_exit(ok ? 0 : 1);
}
