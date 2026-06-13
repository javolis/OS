/* head.c - userland: copy the first N lines of stdin to stdout (default
 * 10, or 'head -<N>'). A pipeline filter. */
#include "ulib.h"

static int parse_int(const char *s) {
    int v = 0;
    while (*s >= '0' && *s <= '9')
        v = v * 10 + (*s++ - '0');
    return v;
}

void _start(int argc, char **argv) {
    int limit = 10;
    if (argc >= 2 && argv[1][0] == '-')
        limit = parse_int(argv[1] + 1);

    char buf[128];
    int n;
    int lines = 0;
    while (lines < limit && (n = sys_read(0, buf, sizeof(buf))) > 0) {
        int start = 0;
        for (int i = 0; i < n && lines < limit; i++) {
            if (buf[i] == '\n') {
                sys_writefd(1, buf + start, i - start + 1);
                start = i + 1;
                lines++;
            }
        }
        /* Flush a trailing partial line only if we haven't hit the limit. */
        if (lines < limit && start < n)
            sys_writefd(1, buf + start, n - start);
    }
    sys_exit(0);
}
