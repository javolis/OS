/* false.c - userland: fail (exit 1). */
#include "usys.h"

void _start(void) {
    sys_exit(1);
}
