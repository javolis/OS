/* ls.c - userland: list every file (initrd + ramfs) with size and a r/w
 * flag, the 'ls -l' of this system. */
#include "ulib.h"

void _start(void) {
    struct dirent e;
    /* uprintf has no width specifiers, so format the columns plainly. */
    for (int i = 0; sys_readdir(i, &e) == 0; i++)
        uprintf("%u %c %s\n", e.size, e.kind ? 'w' : 'r', e.name);
    sys_exit(0);
}
