/* upper.c - userland: copy stdin to stdout, uppercasing ASCII letters.
 * A pipeline filter: 'cat notes.txt | upper' shouts the file. */
#include "usys.h"

void _start(void) {
    char buf[128];
    int n;

    while ((n = sys_read(0, buf, sizeof(buf))) > 0) {
        for (int i = 0; i < n; i++) {
            if (buf[i] >= 'a' && buf[i] <= 'z')
                buf[i] -= 32;
        }
        sys_writefd(1, buf, n);
    }
    sys_exit(0);
}
