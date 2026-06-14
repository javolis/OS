/* envchild.c - userland: read an environment variable set by the parent. */
#include "ulib.h"

void _start(void) {
    char v[64];
    if (sys_getenv("GREETING", v, sizeof(v)) >= 0)
        uprintf("envchild: GREETING=%s\n", v);
    else
        uprintf("envchild: GREETING unset\n");
    sys_exit(0);
}
