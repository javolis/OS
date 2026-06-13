/* wc.c - userland: count lines, words, and bytes on stdin (a pipeline
 * filter). Mirrors POSIX wc's default three-number output. */
#include "ulib.h"

void _start(void) {
    char buf[128];
    int n;
    unsigned lines = 0, words = 0, bytes = 0;
    int in_word = 0;

    while ((n = sys_read(0, buf, sizeof(buf))) > 0) {
        for (int i = 0; i < n; i++) {
            char c = buf[i];
            bytes++;
            if (c == '\n')
                lines++;
            if (c == ' ' || c == '\n' || c == '\t') {
                in_word = 0;
            } else if (!in_word) {
                in_word = 1;
                words++;
            }
        }
    }
    uprintf("%u %u %u\n", lines, words, bytes);
    sys_exit(0);
}
