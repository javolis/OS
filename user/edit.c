/* edit.c - Avolis text editor. A small modal (vi-flavored) editor over the
 * ramfs: type to insert, arrows to move, backspace to delete, Esc for command
 * mode where 's' saves, 'q' quits, 'x' saves and quits.
 *
 *   edit.elf <name>   edit (or create) a ramfs file
 *   edit.elf test     CI: build a buffer, save, reload and compare
 */
#include "avui.h"

#define BUFCAP 16384

static char buf[BUFCAP];
static char want[BUFCAP]; /* CI compare buffer (kept off the small stack) */
static int len, cursor;
static char fname[32];
static char status[96];

enum { INSERT, CMD };

static void copy_name(char *dst, const char *src) {
    int j = 0;
    while (j < 31 && src[j]) {
        dst[j] = src[j];
        j++;
    }
    dst[j] = '\0';
}
static char *put_i(char *p, int v) {
    char t[12];
    int n = 0;
    if (v == 0)
        t[n++] = '0';
    while (v > 0) {
        t[n++] = (char)('0' + v % 10);
        v /= 10;
    }
    while (n)
        *p++ = t[--n];
    return p;
}
static char *put_s(char *p, const char *s) {
    while (*s)
        *p++ = *s++;
    return p;
}

static int line_start(int i) {
    while (i > 0 && buf[i - 1] != '\n')
        i--;
    return i;
}
static int line_end(int i) {
    while (i < len && buf[i] != '\n')
        i++;
    return i;
}

static void ins(char c) {
    if (len >= BUFCAP - 1)
        return;
    for (int i = len; i > cursor; i--)
        buf[i] = buf[i - 1];
    buf[cursor++] = c;
    len++;
}
static void del_before(void) {
    if (cursor == 0)
        return;
    for (int i = cursor - 1; i < len - 1; i++)
        buf[i] = buf[i + 1];
    len--;
    cursor--;
}
static void cur_up(void) {
    int ls = line_start(cursor), col = cursor - ls;
    if (ls == 0)
        return;
    int pls = line_start(ls - 1), plen = (ls - 1) - pls;
    cursor = pls + (col < plen ? col : plen);
}
static void cur_down(void) {
    int le = line_end(cursor);
    if (le == len)
        return;
    int ls = line_start(cursor), col = cursor - ls;
    int nls = le + 1, nlen = line_end(nls) - nls;
    cursor = nls + (col < nlen ? col : nlen);
}

static int do_save(void) {
    int fd = sys_create(fname);
    if (fd < 0)
        return -1;
    int off = 0;
    while (off < len) {
        int chunk = len - off;
        if (chunk > 512)
            chunk = 512;
        int w = sys_writefd(fd, buf + off, chunk);
        if (w <= 0)
            break;
        off += w;
    }
    sys_close(fd);
    return off;
}

static int do_load(void) {
    int fd = sys_open(fname);
    if (fd < 0)
        return 0; /* new file */
    len = 0;
    int r;
    while (len < BUFCAP - 1 &&
           (r = sys_read(fd, buf + len, BUFCAP - 1 - len)) > 0)
        len += r;
    sys_close(fd);
    cursor = 0;
    return len;
}

static void draw(ugfx_t *g, int mode) {
    int W = (int)g->width, H = (int)g->height;
    ugfx_clear(g, AV_BG);
    char title[48];
    char *p = put_s(title, "Editor  -  ");
    put_s(p, fname)[0] = '\0';
    av_text_glow(g, UAFONT_HEAD, 40, 52, title, AV_ORANGE);

    int x = 40, top = 92, lh = 24;
    int maxlines = (H - top - 50) / lh;

    /* cursor line/col */
    int cl = 0;
    for (int i = 0; i < cursor; i++)
        if (buf[i] == '\n')
            cl++;
    int ccol = cursor - line_start(cursor);
    int scroll = 0;
    if (cl >= maxlines)
        scroll = cl - maxlines + 1;

    int li = 0, i = 0;
    char tmp[256];
    while (i <= len && li < scroll + maxlines) {
        int b = 0;
        int lstart = i;
        while (i < len && buf[i] != '\n') {
            if (b < 255)
                tmp[b++] = buf[i];
            i++;
        }
        tmp[b] = '\0';
        int shown = li - scroll;
        if (li >= scroll) {
            int y = top + shown * lh + 18;
            av_text(g, x, y, tmp, AV_WHITE);
            if (li == cl) { /* draw the caret */
                char pre[256];
                int pc = cursor - lstart;
                for (int j = 0; j < pc && j < 255; j++)
                    pre[j] = tmp[j];
                pre[pc < 255 ? pc : 255] = '\0';
                int cx = x + ua_text_width(UAFONT_BODY, pre);
                ugfx_fillrect(g, cx, y - 16, 2, 20, AV_ORANGE);
            }
        }
        if (i < len && buf[i] == '\n')
            i++;
        else if (i >= len)
            break;
        li++;
    }

    /* status line */
    char *s = status;
    s = put_s(s, mode == INSERT ? "-- INSERT --   " : "-- CMD (s save  q quit  x save+quit) --   ");
    s = put_i(s, len);
    s = put_s(s, " bytes   Ln ");
    s = put_i(s, cl + 1);
    s = put_s(s, ", Col ");
    s = put_i(s, ccol + 1);
    *s = '\0';
    ugfx_fillrect(g, 0, H - 34, W, 34, AV_PANEL);
    av_text(g, 20, H - 12, status, mode == INSERT ? AV_WHITE : AV_ORANGE);
}

void _start(int argc, char **argv) {
    ugfx_t g;
    if (ugfx_init(&g) != 0) {
        uprintf("edit: no framebuffer\n");
        sys_exit(0);
    }
    int test = (argc >= 2 && ustreq(argv[1], "test"));

    if (test) {
        copy_name(fname, "edit_test.txt");
        const char *sample = "hello avolis\nthis file was written\nby the editor\n";
        len = 0;
        cursor = 0;
        for (const char *q = sample; *q; q++)
            ins(*q);
        int wrote = do_save();
        uprintf("edit: saved %d bytes\n", wrote);
        /* reload into a check and compare */
        int wl = len;
        for (int i = 0; i < len; i++)
            want[i] = buf[i];
        len = 0;
        do_load();
        int ok = (len == wl);
        for (int i = 0; ok && i < len; i++)
            if (buf[i] != want[i])
                ok = 0;
        draw(&g, INSERT);
        ugfx_flush(&g);
        uprintf(ok ? "edit: ok\n" : "edit: FAIL\n");
        ugfx_free(&g);
        sys_exit(ok ? 0 : 1);
    }

    if (argc >= 2)
        copy_name(fname, argv[1]);
    else
        copy_name(fname, "untitled.txt");
    do_load();

    int mode = INSERT, quit = 0, dirty = 1;
    while (!quit) {
        int k = sys_trygetkey();
        if (k >= 0) {
            if (k == 0x82) { /* left */
                if (cursor > 0)
                    cursor--;
            } else if (k == 0x83) { /* right */
                if (cursor < len)
                    cursor++;
            } else if (k == 0x80) /* up */
                cur_up();
            else if (k == 0x81) /* down */
                cur_down();
            else if (mode == INSERT) {
                if (k == 27)
                    mode = CMD;
                else if (k == '\b' || k == 0x7F)
                    del_before();
                else if (k == '\n' || k == '\r')
                    ins('\n');
                else if (k >= 0x20 && k < 0x7F)
                    ins((char)k);
            } else { /* CMD */
                if (k == 's' || k == 'w') {
                    int w = do_save();
                    uprintf("edit: saved %s (%d bytes)\n", fname, w);
                    mode = INSERT;
                } else if (k == 'q')
                    quit = 1;
                else if (k == 'x') {
                    do_save();
                    quit = 1;
                } else
                    mode = INSERT;
            }
            dirty = 1;
        }
        if (dirty) {
            draw(&g, mode);
            ugfx_flush(&g);
            dirty = 0;
        }
        sys_sleep(30);
    }
    ugfx_free(&g);
    sys_exit(0);
}
