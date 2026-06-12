/* echo.c — userland: prints its arguments, proving argc/argv arrive on
 * the user stack per the C calling convention. */
#include "usys.h"

void _start(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        sys_write(argv[i]);
        if (i + 1 < argc)
            sys_write(" ");
    }
    sys_write("\n");
    sys_exit();
}
