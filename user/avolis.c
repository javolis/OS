/* avolis.c - the Avolis desktop shell.
 *
 * Lock screen (wallpaper, anti-aliased RTC clock, press enter) -> desktop:
 * mostly wallpaper with a Windows-11-style taskbar that can sit on any edge
 * (the user cycles it with 'p'), centered app buttons, an orange accent line
 * and a clock. Keyboard-first: up/down (or a/d) move focus, enter activates.
 * The event loop polls SYS_TRYGETKEY so the clock ticks while it waits.
 *
 * "avolis.elf test" bounds the loop and quits on 'q' for CI; plain
 * "avolis.elf" runs as a normal shell. */
#include "avui.h"

enum { LOCK, DESKTOP };
enum { TB_BOTTOM, TB_LEFT, TB_RIGHT, TB_TOP };

#define TB_THICK 60
#define K_UP 0x80
#define K_DOWN 0x81

static const char *tb_names[4] = {"bottom", "left", "right", "top"};

struct app {
    const char *label;
    const char *elf; /* "" = a shell area (launcher/settings), wired later */
};
static const struct app apps[] = {
    {"Apps", ""},          {"Graphics", "gfxdemo.elf"},
    {"System", "sysinfo.elf"}, {"Network", "netcap.elf"},
    {"Settings", ""},
};
#define NAPPS ((int)(sizeof(apps) / sizeof(apps[0])))

static const char *months[12] = {
    "January", "February", "March",     "April",   "May",      "June",
    "July",    "August",   "September", "October", "November", "December"};
static const char *wdays[7] = {"Sunday",    "Monday", "Tuesday", "Wednesday",
                               "Thursday", "Friday", "Saturday"};

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

static void draw_lock(ugfx_t *g) {
    int W = (int)g->width, H = (int)g->height;
    struct systime t;
    int have = (sys_time(&t) == 0);
    ugfx_vgradient(g, 0, 0, W, H, ugfx_rgb(26, 18, 10), AV_BG);
    av_head(g, 40, 56, "AVOLIS", AV_ORANGE);
    char clk[6];
    clock_str(clk);
    int cw = ua_text_width(UAFONT_DISPLAY, clk);
    ua_text(g, UAFONT_DISPLAY, (W - cw) / 2, H / 2, clk, AV_WHITE);
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
    ua_text_center(g, UAFONT_BODY, 0, W, H - 56, "press enter to unlock",
                   AV_DIM);
}

static void draw_desktop(ugfx_t *g, int pos, int focus) {
    int W = (int)g->width, H = (int)g->height;
    ugfx_vgradient(g, 0, 0, W, H, ugfx_rgb(10, 12, 22), AV_BG);
    ua_text_center(g, UAFONT_HEAD, 0, W, H / 2 - 10, "Avolis", AV_DIM);

    int horiz = (pos == TB_BOTTOM || pos == TB_TOP);
    int bx, by, bw, bh;
    if (pos == TB_BOTTOM) { bx = 0; by = H - TB_THICK; bw = W; bh = TB_THICK; }
    else if (pos == TB_TOP) { bx = 0; by = 0; bw = W; bh = TB_THICK; }
    else if (pos == TB_LEFT) { bx = 0; by = 0; bw = TB_THICK; bh = H; }
    else { bx = W - TB_THICK; by = 0; bw = TB_THICK; bh = H; }

    ugfx_blend_rect(g, bx, by, bw, bh, AV_PANEL, 240); /* taskbar surface */
    /* thin orange accent on the bar's inner edge */
    if (pos == TB_BOTTOM) ugfx_fillrect(g, bx, by, bw, 2, AV_ORANGE);
    else if (pos == TB_TOP) ugfx_fillrect(g, bx, by + bh - 2, bw, 2, AV_ORANGE);
    else if (pos == TB_LEFT) ugfx_fillrect(g, bx + bw - 2, by, 2, bh, AV_ORANGE);
    else ugfx_fillrect(g, bx, by, 2, bh, AV_ORANGE);

    int iw = horiz ? 104 : (TB_THICK - 12);
    int ih = horiz ? (TB_THICK - 14) : 38;
    int gap = 10;
    int total = NAPPS * (horiz ? iw : ih) + (NAPPS - 1) * gap;
    int run = (horiz ? bx + (bw - total) / 2 : by + (bh - total) / 2);
    for (int i = 0; i < NAPPS; i++) {
        int ix, iy;
        if (horiz) { ix = run + i * (iw + gap); iy = by + 7; }
        else { ix = bx + 6; iy = run + i * (ih + gap); }
        av_button(g, ix, iy, iw, ih, apps[i].label, i == focus);
    }

    char clk[6];
    clock_str(clk);
    if (horiz)
        ua_text(g, UAFONT_BODY, bx + bw - 64, by + bh / 2 + 6, clk, AV_GRAY);
    else
        ua_text_center(g, UAFONT_BODY, bx, bw, by + bh - 18, clk, AV_GRAY);
}

void _start(int argc, char **argv) {
    ugfx_t g;
    if (ugfx_init(&g) != 0) {
        uprintf("avolis: no framebuffer\n");
        sys_exit(0);
    }
    int test = (argc >= 2 && argv[1][0] == 't');
    int state = LOCK, pos = TB_BOTTOM, focus = 0, quit = 0, cycles = 0;
    uprintf("avolis: lock screen\n");
    while (!quit) {
        if (state == LOCK)
            draw_lock(&g);
        else
            draw_desktop(&g, pos, focus);
        ugfx_flush(&g);

        int k = -1;
        for (int i = 0; i < 10 && k < 0; i++) {
            k = sys_trygetkey();
            if (k < 0)
                sys_sleep(50);
        }
        if (k >= 0) {
            if (k == 'q' || k == 'Q') {
                quit = 1;
            } else if (state == LOCK) {
                if (k == '\n' || k == '\r') {
                    state = DESKTOP;
                    uprintf("avolis: unlocked\n");
                }
            } else { /* DESKTOP */
                if (k == 27) {
                    state = LOCK;
                    uprintf("avolis: locked\n");
                } else if (k == 'p' || k == 'P') {
                    pos = (pos + 1) % 4;
                    uprintf("avolis: taskbar %s\n", tb_names[pos]);
                } else if (k == K_UP || k == 'a') {
                    focus = (focus + NAPPS - 1) % NAPPS;
                } else if (k == K_DOWN || k == 'd') {
                    focus = (focus + 1) % NAPPS;
                } else if (k == '\n' || k == '\r') {
                    uprintf("avolis: launch %s\n", apps[focus].label);
                }
            }
        }
        if (test && ++cycles > 80) /* CI failsafe: never hang the smoke */
            break;
    }
    uprintf("avolis: bye\n");
    ugfx_free(&g);
    sys_exit(0);
}
