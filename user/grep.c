/* grep.c - userland: print stdin lines that contain argv[1] as a
 * substring. A pipeline filter. Reads the whole stream, splits on '\n'. */
#include "ulib.h"

/* Does haystack contain needle as a substring? */
static int contains(const char *hay, int haylen, const char *needle) {
    int nl = (int)ustrlen(needle);
    if (nl == 0)
        return 1;
    for (int i = 0; i + nl <= haylen; i++) {
        int j = 0;
        while (j < nl && hay[i + j] == needle[j])
            j++;
        if (j == nl)
            return 1;
    }
    return 0;
}

void _start(int argc, char **argv) {
    if (argc < 2) {
        uprintf("usage: grep <pattern>\n");
        sys_exit(1);
    }
    const char *pat = argv[1];

    char line[256];
    int len = 0;
    char ch[64];
    int n;
    while ((n = sys_read(0, ch, sizeof(ch))) > 0) {
        for (int i = 0; i < n; i++) {
            if (ch[i] == '\n') {
                if (contains(line, len, pat)) {
                    line[len] = '\n';
                    sys_writefd(1, line, len + 1);
                }
                len = 0;
            } else if (len < (int)sizeof(line) - 1) {
                line[len++] = ch[i];
            }
        }
    }
    /* A trailing line with no newline. */
    if (len > 0 && contains(line, len, pat))
        sys_writefd(1, line, len);
    sys_exit(0);
}
