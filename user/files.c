/* files.c - Avolis Files: a graphical file browser over the ramfs + initrd.
 *
 * Lists every entry (name, type, size) in an Avolis-styled panel and opens
 * text files in a scrollable viewer. Keyboard + mouse. 'files.elf test'
 * scans, renders, opens notes.txt and exits, so CI can verify it over serial
 * (it cannot see pixels). */
#include "avui.h"

#define MAXF 64
#define VIEWCAP 8192

static char names[MAXF][32];
static unsigned sizes[MAXF];
static unsigned kinds[MAXF];
static int nfiles;

static char view[VIEWCAP];
static int vlen;
static char viewname[32];

enum { LIST, VIEW };

static void copy_name(char *dst, const char *src) {
    int j = 0;
    while (j < 31 && src[j]) {
        dst[j] = src[j];
        j++;
    }
    dst[j] = '\0';
}

static int scan(void) {
    struct dirent d;
    int n = 0;
    for (int i = 0; n < MAXF && sys_readdir(i, &d) == 0; i++) {
        copy_name(names[n], d.name);
        sizes[n] = d.size;
        kinds[n] = d.kind;
        n++;
    }
    return n;
}

static const char *kindstr(unsigned k) {
    return k == 2 ? "folder" : (k == 1 ? "file" : "system");
}

static char *put_u(char *p, unsigned v) {
    char t[12];
    int n = 0;
    if (v == 0)
        t[n++] = '0';
    while (v) {
        t[n++] = (char)('0' + v % 10);
        v /= 10;
    }
    while (n)
        *p++ = t[--n];
    return p;
}

static void open_file(int idx) {
    vlen = 0;
    copy_name(viewname, names[idx]);
    int fd = sys_open(names[idx]);
    if (fd < 0)
        return;
    int r;
    while (vlen < VIEWCAP - 1 &&
           (r = sys_read(fd, view + vlen, VIEWCAP - 1 - vlen)) > 0)
        vlen += r;
    view[vlen] = '\0';
    sys_close(fd);
}

static int list_rows(ugfx_t *g) {
    return ((int)g->height - 110 - 60) / 40;
}

static void draw_list(ugfx_t *g, int sel, int top) {
    int W = (int)g->width, H = (int)g->height;
    ugfx_clear(g, AV_BG);
    av_text_glow(g, UAFONT_HEAD, 40, 56, "Files", AV_ORANGE);
    int x = 40, y0 = 110, rowh = 40, rows = list_rows(g);
    av_panel(g, x, y0, W - 80, rows * rowh + 16, 1);
    for (int i = 0; i < rows && top + i < nfiles; i++) {
        int idx = top + i, y = y0 + 12 + i * rowh;
        if (idx == sel)
            ugfx_round_rect(g, x + 10, y, W - 100, rowh - 6, 6, AV_PANEL2);
        av_text(g, x + 28, y + rowh - 16, names[idx],
                idx == sel ? AV_ORANGE : AV_WHITE);
        av_text(g, W - 320, y + rowh - 16, kindstr(kinds[idx]), AV_GRAY);
        char sz[16];
        char *p = put_u(sz, sizes[idx]);
        *p++ = ' ';
        *p++ = 'B';
        *p = '\0';
        av_text(g, W - 150, y + rowh - 16, sz, AV_GRAY);
    }
    ua_text_center(g, UAFONT_BODY, 0, W, H - 40,
                   "up/down select    enter open    esc quit", AV_DIM);
}

static void draw_view(ugfx_t *g, int scroll) {
    int W = (int)g->width, H = (int)g->height;
    ugfx_clear(g, AV_BG);
    av_text_glow(g, UAFONT_HEAD, 40, 56, viewname, AV_ORANGE);
    int x = 48, y = 110, lh = 24, maxlines = (H - y - 50) / lh;
    int line = 0, shown = 0, i = 0;
    while (i < vlen && line < scroll) {
        if (view[i] == '\n')
            line++;
        i++;
    }
    char buf[200];
    while (i < vlen && shown < maxlines) {
        int b = 0;
        while (i < vlen && view[i] != '\n' && b < 199) {
            char c = view[i++];
            buf[b++] = (c == '\t') ? ' ' : c;
        }
        buf[b] = '\0';
        if (i < vlen && view[i] == '\n')
            i++;
        av_text(g, x, y + shown * lh + 18, buf, AV_WHITE);
        shown++;
    }
    ua_text_center(g, UAFONT_BODY, 0, W, H - 40,
                   "up/down scroll    esc back", AV_DIM);
}

void _start(int argc, char **argv) {
    ugfx_t g;
    if (ugfx_init(&g) != 0) {
        uprintf("files: no framebuffer\n");
        sys_exit(0);
    }
    int test = (argc >= 2 && argv[1][0] == 't');
    nfiles = scan();

    if (test) {
        uprintf("files: listed %d entries\n", nfiles);
        draw_list(&g, 0, 0);
        ugfx_flush(&g);
        int idx = -1;
        for (int i = 0; i < nfiles; i++)
            if (ustreq(names[i], "notes.txt")) {
                idx = i;
                break;
            }
        if (idx >= 0) {
            open_file(idx);
            draw_view(&g, 0);
            ugfx_flush(&g);
            uprintf("files: viewed %s (%d bytes)\n", viewname, vlen);
        }
        uprintf("files: ok\n");
        ugfx_free(&g);
        sys_exit(0);
    }

    int W = (int)g.width, H = (int)g.height;
    int sel = 0, top = 0, scroll = 0, state = LIST, quit = 0, dirty = 1;
    int mx = W / 2, my = H / 2, lmx = -1, lmy = -1;
    uint32_t lbtn = 0;
    int rows = list_rows(&g);

    while (!quit) {
        struct mousestate ms;
        uint32_t btn = lbtn;
        if (sys_mouse(&ms) == 0) {
            mx = ms.x;
            my = ms.y;
            btn = ms.buttons;
        }
        if (state == LIST && (mx != lmx || my != lmy)) {
            for (int i = 0; i < rows && top + i < nfiles; i++) {
                int y = 110 + 12 + i * 40;
                if (my >= y && my < y + 34 && mx >= 50 && mx < W - 60) {
                    sel = top + i;
                    dirty = 1;
                }
            }
        }
        int clicked = (btn & MOUSE_LEFT) && !(lbtn & MOUSE_LEFT);

        int k = sys_trygetkey();
        if (k >= 0 || clicked) {
            if (state == LIST) {
                if (k == 'q' || k == 27)
                    quit = 1;
                else if (k == 0x80) {
                    if (sel > 0)
                        sel--;
                } else if (k == 0x81) {
                    if (sel < nfiles - 1)
                        sel++;
                } else if (k == '\n' || k == '\r' || clicked) {
                    if (nfiles > 0) {
                        open_file(sel);
                        scroll = 0;
                        state = VIEW;
                    }
                }
                if (sel < top)
                    top = sel;
                if (sel >= top + rows)
                    top = sel - rows + 1;
            } else { /* VIEW */
                if (k == 27 || k == 'q')
                    state = LIST;
                else if (k == 0x80) {
                    if (scroll > 0)
                        scroll--;
                } else if (k == 0x81)
                    scroll++;
            }
            dirty = 1;
        }

        if (dirty) {
            if (state == LIST)
                draw_list(&g, sel, top);
            else
                draw_view(&g, scroll);
            ugfx_flush(&g);
            dirty = 0;
        }
        lmx = mx;
        lmy = my;
        lbtn = btn;
        sys_sleep(50);
    }
    ugfx_free(&g);
    sys_exit(0);
}
