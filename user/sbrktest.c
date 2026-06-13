/* sbrktest.c - userland: grow the heap with sys_sbrk and use the memory
 * across a page boundary, proving on-demand page mapping. */
#include "ulib.h"

void _start(void) {
    unsigned char *p = sys_sbrk(8192); /* two pages */
    if (p == (unsigned char *)-1) {
        uprintf("sbrktest: sbrk failed\n");
        sys_exit(1);
    }
    for (int i = 0; i < 8192; i++)
        p[i] = (unsigned char)(i & 0xff);
    int ok = 1;
    for (int i = 0; i < 8192; i++)
        if (p[i] != (unsigned char)(i & 0xff))
            ok = 0;

    /* A second sbrk should hand back a higher, distinct region. */
    unsigned char *q = sys_sbrk(16);
    if (q != p + 8192)
        ok = 0;

    uprintf(ok ? "sbrktest: heap grows ok\n" : "sbrktest: FAIL\n");
    sys_exit(ok ? 0 : 1);
}
