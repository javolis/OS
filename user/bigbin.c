/* bigbin.c - userland: a binary with a 64 KiB BSS (16 pages). Proves the
 * ELF loader maps and zero-fills multi-page segments. */
#include "ulib.h"

static unsigned char bigbss[65536]; /* uninitialized -> .bss, 16 pages */

void _start(void) {
    /* BSS must arrive zero-initialized across all its pages. */
    int allzero = 1;
    for (int i = 0; i < 65536; i++)
        if (bigbss[i] != 0)
            allzero = 0;

    /* And be usable across every page. */
    for (int i = 0; i < 65536; i++)
        bigbss[i] = (unsigned char)(i & 0x3f);
    int ok = allzero;
    for (int i = 0; i < 65536; i++)
        if (bigbss[i] != (unsigned char)(i & 0x3f))
            ok = 0;

    uprintf(ok ? "bigbin: 64k bss ok\n" : "bigbin: FAIL\n");
    sys_exit(ok ? 0 : 1);
}
