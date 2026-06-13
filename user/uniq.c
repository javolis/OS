/* uniq.c - userland: collapse adjacent duplicate lines from stdin. A
 * streaming filter: keeps only the previous line for comparison. */
#include "ulib.h"

void _start(void) {
    char cur[256], prev[256];
    int curlen = 0;
    int have_prev = 0;
    char ch[64];
    int n;

    while ((n = sys_read(0, ch, sizeof(ch))) > 0) {
        for (int i = 0; i < n; i++) {
            if (ch[i] == '\n') {
                cur[curlen] = '\0';
                if (!have_prev || !ustreq(cur, prev)) {
                    sys_writefd(1, cur, curlen);
                    sys_writefd(1, "\n", 1);
                    int k = 0;
                    while (cur[k]) {
                        prev[k] = cur[k];
                        k++;
                    }
                    prev[k] = '\0';
                    have_prev = 1;
                }
                curlen = 0;
            } else if (curlen < (int)sizeof(cur) - 1) {
                cur[curlen++] = ch[i];
            }
        }
    }
    sys_exit(0);
}
