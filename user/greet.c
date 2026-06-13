/* greet.c - userland: interactive keyboard input via sys_readline. */
#include "usys.h"

void _start(void) {
    char name[64];

    sys_write("What is your name? ");
    if (sys_readline(name, sizeof(name)) > 0) {
        sys_write("Hello, ");
        sys_write(name);
        sys_write("!\n");
    } else {
        sys_write("\ngreet: no input\n");
    }
    sys_exit(0);
}
