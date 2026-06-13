/* true.c - userland: succeed (exit 0). */
#include "usys.h"

void _start(void) {
    sys_exit(0);
}
