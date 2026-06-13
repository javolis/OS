/* stdiotest.c - userland: read a file's first line through the buffered
 * ufile reader and emit it, exercising ufopen/ufgets/ufclose + uputs. */
#include "ulib.h"

void _start(void) {
    struct ufile *f = ufopen("words.txt");
    if (!f) {
        uputs("stdiotest: open failed");
        sys_exit(1);
    }
    char line[64];
    char *r = ufgets(line, sizeof(line), f);
    ufclose(f);
    if (!r) {
        uputs("stdiotest: empty");
        sys_exit(1);
    }
    /* words.txt's first line is "pear"; ufgets keeps the trailing newline. */
    uprintf("stdiotest: first=%s", line);
    sys_exit(0);
}
