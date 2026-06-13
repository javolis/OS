/* hello_b.c - userland: sibling of hello_a with its own message. */
#include "usys.h"

void _start(void) {
    for (int i = 0; i < 3; i++) {
        sys_write("ELF process B says hello!\n");
        for (volatile int spin = 0; spin < 10000000; spin++)
            ;
    }
    sys_exit(0);
}
