/* nl.c - userland: number stdin lines on stdout ("<n>\t<line>"). Writes
 * through fd 1 (not uprintf, which targets the console) so it works in a
 * pipeline. */
#include "ulib.h"

static int utoa(unsigned v, char *buf) {
    char tmp[12];
    int n = 0;
    do {
        tmp[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v);
    for (int i = 0; i < n; i++)
        buf[i] = tmp[n - 1 - i];
    return n;
}

void _start(void) {
    char ch[64], num[12];
    unsigned line = 1;
    int at_start = 1;
    int n;

    while ((n = sys_read(0, ch, sizeof(ch))) > 0) {
        for (int i = 0; i < n; i++) {
            if (at_start) {
                int l = utoa(line++, num);
                sys_writefd(1, num, l);
                sys_writefd(1, "\t", 1);
                at_start = 0;
            }
            sys_writefd(1, &ch[i], 1);
            if (ch[i] == '\n')
                at_start = 1;
        }
    }
    sys_exit(0);
}
