/* avolis.c - the Avolis desktop shell.
 *
 * Lock -> desktop (constellation wallpaper + a Windows-11-style taskbar on any
 * edge) -> applications grid / command palette / settings. Fully traversable
 * by mouse (hover highlights, click activates) or keyboard; the constellation
 * follows the cursor (parallax). A rendered arrow cursor sits on top. The
 * event loop redraws only on change (mouse move, key, click, or a ~1s clock
 * tick) so it stays light.
 *
 * "avolis.elf test" bounds the loop and quits on 'q' for CI. */
#include "avui.h"
#include "avwall.h"

enum { LOCK, DESKTOP, PALETTE, APPS, SETTINGS };
enum { TB_BOTTOM, TB_LEFT, TB_RIGHT, TB_TOP };

#define TB_THICK 60
#define APPS_COLS 4
#define K_UP 0x80
#define K_DOWN 0x81

static const char *tb_names[4] = {"bottom", "left", "right", "top"};

struct app {
    const char *label;
    const char *elf; /* "" = opens a launcher (Apps) or settings */
};
static const struct app bar[] = {
    {"Apps", ""},
    {"Graphics", "gfxdemo.elf"},
    {"System", "sysinfo.elf"},
    {"Settings", ""},
};
#define NBAR ((int)(sizeof(bar) / sizeof(bar[0])))
static const struct app pal[] = {
    {"graphics demo", "gfxdemo.elf"}, {"input demo", "inputdemo.elf"},
    {"system info", "sysinfo.elf"},   {"date", "date.elf"},
    {"files", "ls.elf"},              {"network", "netcap.elf"},
    {"shell", "ush.elf"},
};
#define NPAL ((int)(sizeof(pal) / sizeof(pal[0])))

struct wp {
    const char *name;
    unsigned seed;
    unsigned warm;
};
static const struct wp walls[] = {
    {"nebula", 0x1a2b3c4d, 0x281404},
    {"ember", 0x9f331107, 0x3a1c08},
    {"carbon", 0x55aa1133, 0x140d06},
    {"dusk", 0x33cc55aa, 0x1e1024},
};
#define NWALLS ((int)(sizeof(walls) / sizeof(walls[0])))

static const char *months[12] = {
    "January", "February", "March",     "April",   "May",      "June",
    "July",    "August",   "September", "October", "November", "December"};
static const char *wdays[7] = {"Sunday",    "Monday", "Tuesday", "Wednesday",
                               "Thursday", "Friday", "Saturday"};

/* Shell state (global so mouse and keyboard share the same actions). */
static int state = LOCK, pos = TB_BOTTOM, focus, sel, srow, wp;
static char query[32];
static int qlen;
static int par_ox, par_oy; /* constellation parallax offset */

/* A classic 12x17 arrow cursor: 0 transparent, 1 white, 2 dark outline. */
static const unsigned char cursor_bmp[17][12] = {
    {2},
    {2, 2},
    {2, 1, 2},
    {2, 1, 1, 2},
    {2, 1, 1, 1, 2},
    {2, 1, 1, 1, 1, 2},
    {2, 1, 1, 1, 1, 1, 2},
    {2, 1, 1, 1, 1, 1, 1, 2},
    {2, 1, 1, 1, 1, 1, 1, 1, 2},
    {2, 1, 1, 1, 1, 1, 1, 1, 1, 2},
    {2, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2},
    {2, 1, 1, 2, 1, 1, 2},
    {2, 1, 2, 0, 2, 1, 1, 2},
    {2, 2, 0, 0, 2, 1, 1, 2},
    {2, 0, 0, 0, 0, 2, 1, 1, 2},
    {0, 0, 0, 0, 0, 2, 1, 1, 2},
    {0, 0, 0, 0, 0, 0, 2, 2, 2},
};

static int dow(int y, int m, int d) {
    static const int t[12] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (m < 1 || m > 12)
        return 0;
    if (m < 3)
        y -= 1;
    int r = (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
    return r < 0 ? r + 7 : r;
}
static char *put_s(char *p, const char *s) {
    while (*s)
        *p++ = *s++;
    return p;
}
static char *put_i(char *p, int v) {
    char tmp[12];
    int n = 0;
    if (v == 0)
        tmp[n++] = '0';
    while (v > 0) {
        tmp[n++] = (char)('0' + v % 10);
        v /= 10;
    }
    while (n)
        *p++ = tmp[--n];
    return p;
}
static void two(char *b, int v) {
    b[0] = (char)('0' + (v / 10) % 10);
    b[1] = (char)('0' + v % 10);
}
static void clock_str(char *clk) {
    struct systime t;
    if (sys_time(&t) == 0) {
        two(clk, t.hour);
        clk[2] = ':';
        two(clk + 3, t.minute);
        clk[5] = '\0';
    } else {
        put_s(clk, "--:--")[0] = '\0';
    }
}
static int lc(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }
static int contains(const char *s, const char *q) {
    if (!*q)
        return 1;
    for (; *s; s++) {
        const char *a = s, *b = q;
        while (*a && *b && lc(*a) == lc(*b)) {
            a++;
            b++;
        }
        if (!*b)
            return 1;
    }
    return 0;
}
static int pal_filter(int *out) {
    int n = 0;
    for (int i = 0; i < NPAL; i++)
        if (contains(pal[i].label, query))
            out[n++] = i;
    return n;
}
static void wall(ugfx_t *g) {
    avwall_render(g, walls[wp].seed, walls[wp].warm, par_ox, par_oy);
}

/* --- taskbar geometry (shared by draw + hit-test) --- */
static void tb_rect(int W, int H, int *bx, int *by, int *bw, int *bh,
                    int *horiz) {
    *horiz = (pos == TB_BOTTOM || pos == TB_TOP);
    if (pos == TB_BOTTOM) { *bx = 0; *by = H - TB_THICK; *bw = W; *bh = TB_THICK; }
    else if (pos == TB_TOP) { *bx = 0; *by = 0; *bw = W; *bh = TB_THICK; }
    else if (pos == TB_LEFT) { *bx = 0; *by = 0; *bw = TB_THICK; *bh = H; }
    else { *bx = W - TB_THICK; *by = 0; *bw = TB_THICK; *bh = H; }
}
static void tb_item_rect(int W, int H, int i, int *ix, int *iy, int *iw,
                         int *ih) {
    int bx, by, bw, bh, horiz;
    tb_rect(W, H, &bx, &by, &bw, &bh, &horiz);
    *iw = horiz ? 104 : (TB_THICK - 12);
    *ih = horiz ? (TB_THICK - 14) : 38;
    int gap = 10;
    int total = NBAR * (horiz ? *iw : *ih) + (NBAR - 1) * gap;
    int run = horiz ? bx + (bw - total) / 2 : by + (bh - total) / 2;
    if (horiz) { *ix = run + i * (*iw + gap); *iy = by + 7; }
    else { *ix = bx + 6; *iy = run + i * (*ih + gap); }
}
static void apps_tile_rect(int W, int i, int *x, int *y, int *w, int *h) {
    int tw = 150, th = 92, gap = 24;
    int gwid = APPS_COLS * tw + (APPS_COLS - 1) * gap;
    int gx = (W - gwid) / 2, gy = 120;
    *x = gx + (i % APPS_COLS) * (tw + gap);
    *y = gy + (i / APPS_COLS) * (th + gap);
    *w = tw;
    *h = th;
}
static int inrect(int px, int py, int x, int y, int w, int h) {
    return px >= x && px < x + w && py >= y && py < y + h;
}

static void draw_cursor(ugfx_t *g, int x, int y) {
    for (int r = 0; r < 17; r++)
        for (int c = 0; c < 12; c++) {
            unsigned char v = cursor_bmp[r][c];
            if (v == 1)
                ugfx_putpixel(g, x + c, y + r, 0x00FFFFFF);
            else if (v == 2)
                ugfx_blend_pixel(g, x + c, y + r, 0x00000000, 220);
        }
}

static void draw_lock(ugfx_t *g) {
    int W = (int)g->width, H = (int)g->height;
    struct systime t;
    int have = (sys_time(&t) == 0);
    wall(g);
    ugfx_glow_dot(g, 30, 46, 5, AV_ORANGE);
    av_text_glow(g, UAFONT_HEAD, 52, 56, "AVOLIS", AV_ORANGE);
    char clk[6];
    clock_str(clk);
    int cw = ua_text_width(UAFONT_DISPLAY, clk);
    av_text_glow(g, UAFONT_DISPLAY, (W - cw) / 2, H / 2, clk, AV_WHITE);
    if (have) {
        char date[48], *p = date;
        int mi = (t.month >= 1 && t.month <= 12) ? t.month - 1 : 0;
        p = put_s(p, wdays[dow(t.year, t.month, t.day)]);
        p = put_s(p, ", ");
        p = put_s(p, months[mi]);
        *p++ = ' ';
        p = put_i(p, t.day);
        *p = '\0';
        ua_text_center(g, UAFONT_BODY, 0, W, H / 2 + 34, date, AV_GRAY);
    }
    ua_text_center(g, UAFONT_BODY, 0, W, H - 56,
                   "press enter or click to unlock", AV_DIM);
}

static void draw_desktop(ugfx_t *g) {
    int W = (int)g->width, H = (int)g->height;
    wall(g);
    ugfx_glow_dot(g, 30, 46, 5, AV_ORANGE);
    av_text_glow(g, UAFONT_HEAD, 52, 56, "AVOLIS", AV_ORANGE);

    int bx, by, bw, bh, horiz;
    tb_rect(W, H, &bx, &by, &bw, &bh, &horiz);
    ugfx_blend_rect(g, bx, by, bw, bh, AV_PANEL, 240);
    if (pos == TB_BOTTOM) ugfx_fillrect(g, bx, by, bw, 2, AV_ORANGE);
    else if (pos == TB_TOP) ugfx_fillrect(g, bx, by + bh - 2, bw, 2, AV_ORANGE);
    else if (pos == TB_LEFT) ugfx_fillrect(g, bx + bw - 2, by, 2, bh, AV_ORANGE);
    else ugfx_fillrect(g, bx, by, 2, bh, AV_ORANGE);
    for (int i = 0; i < NBAR; i++) {
        int ix, iy, iw, ih;
        tb_item_rect(W, H, i, &ix, &iy, &iw, &ih);
        av_button(g, ix, iy, iw, ih, bar[i].label, i == focus);
    }
    char clk[6];
    clock_str(clk);
    if (horiz)
        ua_text(g, UAFONT_BODY, bx + bw - 64, by + bh / 2 + 6, clk, AV_GRAY);
    else
        ua_text_center(g, UAFONT_BODY, bx, bw, by + bh - 18, clk, AV_GRAY);
}

static void draw_apps(ugfx_t *g) {
    int W = (int)g->width, H = (int)g->height;
    wall(g);
    av_text_glow(g, UAFONT_HEAD, 40, 56, "Applications", AV_ORANGE);
    for (int i = 0; i < NPAL; i++) {
        int x, y, w, h;
        apps_tile_rect(W, i, &x, &y, &w, &h);
        av_panel(g, x, y, w, h, i == sel);
        ua_text_center(g, UAFONT_BODY, x, w, y + h / 2 + 6, pal[i].label,
                       i == sel ? AV_ORANGE : AV_WHITE);
    }
    ua_text_center(g, UAFONT_BODY, 0, W, H - 48,
                   "click or enter to open    -    esc back", AV_DIM);
}

static void palette_geom(ugfx_t *g, int *px, int *py, int *pw, int *ph) {
    *pw = 560;
    *ph = 360;
    *px = ((int)g->width - *pw) / 2;
    *py = ((int)g->height - *ph) / 2;
}
static void draw_palette(ugfx_t *g) {
    int W = (int)g->width, H = (int)g->height;
    draw_desktop(g);
    ugfx_blend_rect(g, 0, 0, W, H, ugfx_rgb(0, 0, 0), 150);
    int px, py, pw, ph;
    palette_geom(g, &px, &py, &pw, &ph);
    av_panel(g, px, py, pw, ph, 1);
    char line[40];
    char *p = put_s(line, query);
    *p++ = '_';
    *p = '\0';
    ua_text(g, UAFONT_HEAD, px + 28, py + 56,
            query[0] ? line : "search...", query[0] ? AV_WHITE : AV_DIM);
    ugfx_fillrect(g, px + 24, py + 74, pw - 48, 2, AV_BORDER);
    int match[NPAL];
    int n = pal_filter(match);
    if (n == 0) {
        av_text(g, px + 28, py + 116, "no matches", AV_DIM);
        return;
    }
    for (int i = 0; i < n; i++) {
        int ry = py + 92 + i * 38;
        if (i == sel)
            ugfx_round_rect(g, px + 16, ry, pw - 32, 34, 6, AV_PANEL2);
        av_text(g, px + 30, ry + 24, pal[match[i]].label,
                i == sel ? AV_ORANGE : AV_WHITE);
    }
}

static void draw_settings(ugfx_t *g) {
    int W = (int)g->width, H = (int)g->height;
    wall(g);
    av_text_glow(g, UAFONT_HEAD, 40, 56, "Settings", AV_ORANGE);
    const char *labels[2] = {"Wallpaper", "Taskbar position"};
    const char *values[2] = {walls[wp].name, tb_names[pos]};
    int rx = 80, ry0 = 170, rh = 66;
    for (int r = 0; r < 2; r++) {
        int y = ry0 + r * rh;
        if (r == srow)
            ugfx_round_rect(g, rx - 18, y - 30, W - 2 * (rx - 18), 46, 8,
                            AV_PANEL2);
        av_text(g, rx, y, labels[r], AV_GRAY);
        av_text(g, rx + 340, y, values[r], r == srow ? AV_ORANGE : AV_WHITE);
    }
    ua_text_center(g, UAFONT_BODY, 0, W, H - 48,
                   "click/enter or a/d change    -    esc back", AV_DIM);
}

static void render(ugfx_t *g, int mx, int my) {
    if (state == LOCK)
        draw_lock(g);
    else if (state == DESKTOP)
        draw_desktop(g);
    else if (state == APPS)
        draw_apps(g);
    else if (state == SETTINGS)
        draw_settings(g);
    else
        draw_palette(g);
    draw_cursor(g, mx, my);
    ugfx_flush(g);
}

static void launch(const char *elf) {
    if (!elf[0])
        return;
    uprintf("avolis: run %s\n", elf);
    int pid = sys_spawn_fg(elf);
    if (pid >= 0)
        sys_wait(pid);
}

static void open_palette(void) {
    qlen = 0;
    query[0] = '\0';
    sel = 0;
    state = PALETTE;
    uprintf("avolis: palette\n");
}

/* Activate the current selection (shared by Enter and click). */
static void activate(void) {
    if (state == LOCK) {
        state = DESKTOP;
        uprintf("avolis: unlocked\n");
    } else if (state == DESKTOP) {
        if (bar[focus].elf[0])
            launch(bar[focus].elf);
        else if (ustreq(bar[focus].label, "Settings")) {
            srow = 0;
            state = SETTINGS;
            uprintf("avolis: settings\n");
        } else {
            sel = 0;
            state = APPS;
            uprintf("avolis: apps\n");
        }
    } else if (state == APPS) {
        launch(pal[sel].elf);
        state = DESKTOP;
    } else if (state == PALETTE) {
        int match[NPAL];
        int n = pal_filter(match);
        if (n > 0) {
            if (sel >= n)
                sel = n - 1;
            launch(pal[match[sel]].elf);
        }
        state = DESKTOP;
    } else if (state == SETTINGS) {
        if (srow == 0) {
            wp = (wp + 1) % NWALLS;
            uprintf("avolis: wallpaper %s\n", walls[wp].name);
        } else {
            pos = (pos + 1) % 4;
            uprintf("avolis: taskbar %s\n", tb_names[pos]);
        }
    }
}

/* Move focus/selection to whatever element the cursor is over. */
static void hover(ugfx_t *g, int mx, int my) {
    int W = (int)g->width, H = (int)g->height;
    if (state == DESKTOP) {
        for (int i = 0; i < NBAR; i++) {
            int ix, iy, iw, ih;
            tb_item_rect(W, H, i, &ix, &iy, &iw, &ih);
            if (inrect(mx, my, ix, iy, iw, ih))
                focus = i;
        }
    } else if (state == APPS) {
        for (int i = 0; i < NPAL; i++) {
            int x, y, w, h;
            apps_tile_rect(W, i, &x, &y, &w, &h);
            if (inrect(mx, my, x, y, w, h))
                sel = i;
        }
    } else if (state == PALETTE) {
        int px, py, pw, ph;
        palette_geom(g, &px, &py, &pw, &ph);
        int match[NPAL];
        int n = pal_filter(match);
        for (int i = 0; i < n; i++)
            if (inrect(mx, my, px + 16, py + 92 + i * 38, pw - 32, 34))
                sel = i;
    } else if (state == SETTINGS) {
        int rx = 80, ry0 = 170, rh = 66;
        for (int r = 0; r < 2; r++)
            if (inrect(mx, my, rx - 18, ry0 + r * rh - 30, W - 2 * (rx - 18),
                       46))
                srow = r;
    }
}

static void handle_key(int k, int *quit) {
    if (k == 'q' && (state == DESKTOP || state == LOCK)) {
        *quit = 1;
    } else if (state == LOCK) {
        if (k == '\n' || k == '\r')
            activate();
    } else if (state == DESKTOP) {
        if (k == 'p' || k == 'P') {
            pos = (pos + 1) % 4;
            uprintf("avolis: taskbar %s\n", tb_names[pos]);
        } else if (k == '/')
            open_palette();
        else if (k == 's' || k == 'S') {
            srow = 0;
            state = SETTINGS;
            uprintf("avolis: settings\n");
        } else if (k == K_UP || k == 'a')
            focus = (focus + NBAR - 1) % NBAR;
        else if (k == K_DOWN || k == 'd')
            focus = (focus + 1) % NBAR;
        else if (k == '\n' || k == '\r')
            activate();
    } else if (state == APPS) {
        if (k == 27)
            state = DESKTOP;
        else if (k == K_UP) {
            if (sel >= APPS_COLS)
                sel -= APPS_COLS;
        } else if (k == K_DOWN) {
            if (sel + APPS_COLS < NPAL)
                sel += APPS_COLS;
        } else if (k == 'a') {
            if (sel > 0)
                sel--;
        } else if (k == 'd') {
            if (sel < NPAL - 1)
                sel++;
        } else if (k == '\n' || k == '\r')
            activate();
    } else if (state == SETTINGS) {
        if (k == 27)
            state = DESKTOP;
        else if (k == K_UP) {
            if (srow > 0)
                srow--;
        } else if (k == K_DOWN) {
            if (srow < 1)
                srow++;
        } else if (k == 'a') {
            if (srow == 0)
                wp = (wp + NWALLS - 1) % NWALLS;
            else
                pos = (pos + 3) % 4;
            uprintf("avolis: %s\n", srow == 0 ? walls[wp].name : tb_names[pos]);
        } else if (k == 'd' || k == '\n' || k == '\r')
            activate();
    } else { /* PALETTE */
        if (k == 27)
            state = DESKTOP;
        else if (k == K_UP) {
            if (sel > 0)
                sel--;
        } else if (k == K_DOWN)
            sel++;
        else if (k == '\b' || k == 0x7F) {
            if (qlen > 0)
                query[--qlen] = '\0';
            sel = 0;
        } else if (k == '\n' || k == '\r')
            activate();
        else if (k >= 0x20 && k < 0x7F && qlen < (int)sizeof(query) - 1) {
            query[qlen++] = (char)k;
            query[qlen] = '\0';
            sel = 0;
        }
        int match[NPAL];
        int n = pal_filter(match);
        if (n > 0 && sel >= n)
            sel = n - 1;
    }
}

void _start(int argc, char **argv) {
    ugfx_t g;
    if (ugfx_init(&g) != 0) {
        uprintf("avolis: no framebuffer\n");
        sys_exit(0);
    }
    int W = (int)g.width, H = (int)g.height;
    int test = (argc >= 2 && argv[1][0] == 't');
    int quit = 0, cycles = 0, idle = 0, dirty = 1;
    int mx = W / 2, my = H / 2, last_mx = -1, last_my = -1;
    uint32_t btn = 0, last_btn = 0;

    struct mousestate dummy;
    int have_mouse = (sys_mouse(&dummy) == 0);
    uprintf("avolis: lock screen\n");
    if (have_mouse)
        uprintf("avolis: mouse ready\n");

    while (!quit) {
        struct mousestate ms;
        if (have_mouse && sys_mouse(&ms) == 0) {
            mx = ms.x;
            my = ms.y;
            btn = ms.buttons;
        }
        par_ox = (mx - W / 2) / 24;
        par_oy = (my - H / 2) / 24;
        if (mx != last_mx || my != last_my) {
            hover(&g, mx, my);
            dirty = 1;
        }

        int clicked = (btn & MOUSE_LEFT) && !(last_btn & MOUSE_LEFT);
        if (clicked) {
            if (state == LOCK || state == DESKTOP || state == APPS ||
                state == PALETTE || state == SETTINGS) {
                uprintf("avolis: click\n");
                activate();
                dirty = 1;
            }
        }

        int k = sys_trygetkey();
        if (k >= 0) {
            handle_key(k, &quit);
            dirty = 1;
        }

        if (++idle >= 20) { /* ~1s: refresh the clock */
            idle = 0;
            dirty = 1;
        }
        if (dirty && !quit) {
            render(&g, mx, my);
            dirty = 0;
        }
        last_mx = mx;
        last_my = my;
        last_btn = btn;
        sys_sleep(50);
        if (test && ++cycles > 160)
            break;
    }
    uprintf("avolis: bye\n");
    ugfx_free(&g);
    sys_exit(0);
}
