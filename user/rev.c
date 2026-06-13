/* rev.c - userland: reverse the characters of each stdin line. A
 * line-oriented filter. */
#include "ulib.h"

static void flush_reversed(char *line, int len) {
    for (int i = 0; i < len / 2; i++) {
        char t = line[i];
        line[i] = line[len - 1 - i];
        line[len - 1 - i] = t;
    }
    if (len > 0)
        sys_writefd(1, line, len);
    sys_writefd(1, "\n", 1);
}

void _start(void) {
    char line[256];
    int len = 0;
    char ch[64];
    int n;

    while ((n = sys_read(0, ch, sizeof(ch))) > 0) {
        for (int i = 0; i < n; i++) {
            if (ch[i] == '\n') {
                flush_reversed(line, len);
                len = 0;
            } else if (len < (int)sizeof(line)) {
                line[len++] = ch[i];
            }
        }
    }
    if (len > 0)
        flush_reversed(line, len); /* trailing line, no newline */
    sys_exit(0);
}
