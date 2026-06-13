/* hello_a.c - userland: a real ELF executable, compiled separately and
 * loaded by the kernel's ELF loader. The spin keeps it CPU-bound long
 * enough for the scheduler to interleave it with its sibling. */
#include "usys.h"

void _start(void) {
    for (int i = 0; i < 3; i++) {
        sys_write("ELF process A says hello!\n");
        for (volatile int spin = 0; spin < 10000000; spin++)
            ;
    }
    sys_exit(0);
}
