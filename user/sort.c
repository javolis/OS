/* sort.c - userland: read all of stdin, sort the lines lexicographically,
 * and print them. A whole-input filter (bounded buffers). */
#include "ulib.h"

#define MAX_LINES 128
#define BUF_SIZE 4096

static int strless(const char *a, const char *b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return (unsigned char)*a < (unsigned char)*b;
}

void _start(void) {
    static char buf[BUF_SIZE];
    int total = 0, n;
    while (total < BUF_SIZE - 1 &&
           (n = sys_read(0, buf + total, BUF_SIZE - 1 - total)) > 0)
        total += n;
    buf[total] = '\0';

    /* Split into lines in place (newline -> NUL). */
    char *lines[MAX_LINES];
    int nlines = 0;
    int start = 0;
    for (int i = 0; i < total && nlines < MAX_LINES; i++) {
        if (buf[i] == '\n') {
            buf[i] = '\0';
            lines[nlines++] = buf + start;
            start = i + 1;
        }
    }
    if (start < total && nlines < MAX_LINES) /* trailing line, no newline */
        lines[nlines++] = buf + start;

    /* Insertion sort (small inputs). */
    for (int i = 1; i < nlines; i++) {
        char *key = lines[i];
        int j = i - 1;
        while (j >= 0 && strless(key, lines[j])) {
            lines[j + 1] = lines[j];
            j--;
        }
        lines[j + 1] = key;
    }

    for (int i = 0; i < nlines; i++) {
        sys_writefd(1, lines[i], (int)ustrlen(lines[i]));
        sys_writefd(1, "\n", 1);
    }
    sys_exit(0);
}
