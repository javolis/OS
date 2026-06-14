/* ls.c - userland: list every file (initrd + ramfs) with size and a r/w
 * flag, the 'ls -l' of this system. */
#include "ulib.h"

void _start(void) {
    struct dirent e;
    /* uprintf has no width specifiers, so format the columns plainly.
     * Type: r = read-only initrd, w = ramfs file, d = ramfs directory. */
    for (int i = 0; sys_readdir(i, &e) == 0; i++) {
        char t = e.kind == 0 ? 'r' : (e.kind == 2 ? 'd' : 'w');
        uprintf("%u %c %s%s\n", e.size, t, e.name, e.kind == 2 ? "/" : "");
    }
    sys_exit(0);
}
