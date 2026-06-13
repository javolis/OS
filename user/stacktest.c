/* stacktest.c - userland: use a large stack buffer that spans several
 * pages, proving the multi-page user stack. (This would fault on the old
 * single 4 KiB stack page.) */
#include "ulib.h"

void _start(void) {
    volatile char buf[8192];
    for (int i = 0; i < 8192; i++)
        buf[i] = (char)(i & 0x7f);
    int ok = 1;
    for (int i = 0; i < 8192; i++)
        if (buf[i] != (char)(i & 0x7f))
            ok = 0;
    uprintf(ok ? "stacktest: big stack ok\n" : "stacktest: FAIL\n");
    sys_exit(ok ? 0 : 1);
}
