/* avolis.c - the Avolis desktop shell. v1: a lock screen (wallpaper, a large
 * anti-aliased clock from the RTC, the date, press-enter-to-unlock) and a
 * placeholder desktop (the real taskbar/launcher come next). The event loop
 * polls SYS_TRYGETKEY so the clock ticks while it waits for input.
 *
 * Run "avolis.elf test" to enable a bounded loop + quit-on-q for CI; plain
 * "avolis.elf" runs as a normal shell (only quits on q). */
#include "avui.h"

enum { LOCK, DESKTOP };

static const char *months[12] = {
    "January", "February", "March",     "April",   "May",      "June",
    "July",    "August",   "September", "October", "November", "December"};
static const char *wdays[7] = {"Sunday",    "Monday", "Tuesday", "Wednesday",
                               "Thursday", "Friday", "Saturday"};

/* Sakamoto's day-of-week: 0 = Sunday. */
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

static void draw_lock(ugfx_t *g) {
    int W = (int)g->width, H = (int)g->height;
    struct systime t;
    int have = (sys_time(&t) == 0);
    ugfx_vgradient(g, 0, 0, W, H, ugfx_rgb(26, 18, 10), AV_BG);
    av_head(g, 40, 56, "AVOLIS", AV_ORANGE);

    char clk[6];
    if (have) {
        two(clk, t.hour);
        clk[2] = ':';
        two(clk + 3, t.minute);
        clk[5] = '\0';
    } else {
        put_s(clk, "--:--")[0] = '\0';
    }
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

static void draw_desktop(ugfx_t *g) {
    int W = (int)g->width, H = (int)g->height;
    ugfx_vgradient(g, 0, 0, W, H, ugfx_rgb(10, 12, 20), AV_BG);
    av_head(g, 40, 56, "AVOLIS", AV_ORANGE);
    ua_text_center(g, UAFONT_HEAD, 0, W, H / 2, "welcome", AV_WHITE);
    ua_text_center(g, UAFONT_BODY, 0, W, H / 2 + 30, "desktop coming next",
                   AV_GRAY);
    ua_text_center(g, UAFONT_BODY, 0, W, H - 56, "esc lock   -   q quit",
                   AV_DIM);
}

void _start(int argc, char **argv) {
    ugfx_t g;
    if (ugfx_init(&g) != 0) {
        uprintf("avolis: no framebuffer\n");
        sys_exit(0);
    }
    int test = (argc >= 2 && argv[1][0] == 't');
    int state = LOCK, quit = 0, cycles = 0;
    uprintf("avolis: lock screen\n");
    while (!quit) {
        if (state == LOCK)
            draw_lock(&g);
        else
            draw_desktop(&g);
        ugfx_flush(&g);

        int k = -1;
        for (int i = 0; i < 10 && k < 0; i++) {
            k = sys_trygetkey();
            if (k < 0)
                sys_sleep(50);
        }
        if (k >= 0) {
            if (k == 'q' || k == 'Q')
                quit = 1;
            else if (state == LOCK && (k == '\n' || k == '\r')) {
                state = DESKTOP;
                uprintf("avolis: unlocked\n");
            } else if (state == DESKTOP && k == 27) {
                state = LOCK;
                uprintf("avolis: locked\n");
            }
        }
        if (test && ++cycles > 60) /* CI failsafe: never hang the smoke */
            break;
    }
    uprintf("avolis: bye\n");
    ugfx_free(&g);
    sys_exit(0);
}
