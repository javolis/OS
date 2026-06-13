/* exitcode.c - userland: exits with a distinctive status code so a parent
 * can prove sys_wait reports it. */
#include "usys.h"

void _start(void) {
    sys_exit(42);
}
