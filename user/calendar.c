/* calendar.c - Avolis Clock & Calendar: a large live clock plus a month grid
 * with today highlighted, from the RTC (sys_time). 't' toggles 12/24-hour,
 * q/esc quits. 'calendar.elf test' renders one frame, prints the date and
 * exits so CI can verify it over serial. */
#include "avui.h"

static const char *months[12] = {
    "January", "February", "March",     "April",   "May",      "June",
    "July",    "August",   "September", "October", "November", "December"};
static const char *wdays[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

static int dow(int y, int m, int d) {
    static const int t[12] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (m < 3)
        y -= 1;
    int r = (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
    return r < 0 ? r + 7 : r;
}
static int days_in_month(int y, int m) {
    static const int dm[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0))
        return 29;
    return dm[m - 1];
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
static void two(char *b, int v) {
    b[0] = (char)('0' + (v / 10) % 10);
    b[1] = (char)('0' + v % 10);
}

static void clock_text(struct systime *t, int fmt12, char *out) {
    int h = t->hour;
    if (fmt12) {
        int hh = h % 12;
        if (hh == 0)
            hh = 12;
        two(out, hh);
        out[2] = ':';
        two(out + 3, t->minute);
        out[5] = ':';
        two(out + 6, t->second);
        put_s(out + 8, h < 12 ? " AM" : " PM")[0] = '\0';
    } else {
        two(out, h);
        out[2] = ':';
        two(out + 3, t->minute);
        out[5] = ':';
        two(out + 6, t->second);
        out[8] = '\0';
    }
}

static void draw(ugfx_t *g, int fmt12) {
    int W = (int)g->width;
    struct systime t;
    int have = (sys_time(&t) == 0);
    ugfx_clear(g, AV_BG);
    ugfx_glow_dot(g, 30, 46, 5, AV_ORANGE);
    av_text_glow(g, UAFONT_HEAD, 52, 56, "Clock", AV_ORANGE);
    if (!have) {
        ua_text_center(g, UAFONT_HEAD, 0, W, 200, "no clock", AV_GRAY);
        return;
    }

    char buf[16];
    clock_text(&t, fmt12, buf);
    int cw = ua_text_width(UAFONT_DISPLAY, buf);
    av_text_glow(g, UAFONT_DISPLAY, (W - cw) / 2, 150, buf, AV_WHITE);

    char date[48], *p = date;
    p = put_s(p, wdays[dow(t.year, t.month, t.day)]);
    p = put_s(p, ", ");
    p = put_s(p, months[t.month - 1]);
    *p++ = ' ';
    p = put_i(p, t.day);
    *p++ = ',';
    *p++ = ' ';
    p = put_i(p, t.year);
    *p = '\0';
    ua_text_center(g, UAFONT_HEAD, 0, W, 210, date, AV_GRAY);

    /* Month calendar grid, today highlighted. */
    int cols = 7, cellw = 92, cellh = 48;
    int gw = cols * cellw;
    int gx = (W - gw) / 2, gy = 270;
    for (int c = 0; c < cols; c++)
        ua_text_center(g, UAFONT_BODY, gx + c * cellw, cellw, gy, wdays[c],
                       AV_DIM);
    int first = dow(t.year, t.month, 1);
    int ndays = days_in_month(t.year, t.month);
    for (int d = 1; d <= ndays; d++) {
        int cell = first + d - 1;
        int col = cell % 7, row = cell / 7;
        int x = gx + col * cellw, y = gy + 24 + row * cellh;
        if (d == t.day)
            ugfx_round_rect(g, x + 12, y + 6, cellw - 24, cellh - 12, 8,
                            AV_ORANGE);
        char ds[4];
        put_i(ds, d)[0] = '\0';
        ua_text_center(g, UAFONT_BODY, x, cellw, y + cellh - 14, ds,
                       d == t.day ? AV_BG : AV_WHITE);
    }
    ua_text_center(g, UAFONT_BODY, 0, W, (int)g->height - 40,
                   "t  12/24-hour    q  quit", AV_DIM);
}

void _start(int argc, char **argv) {
    ugfx_t g;
    if (ugfx_init(&g) != 0) {
        uprintf("calendar: no framebuffer\n");
        sys_exit(0);
    }
    int test = (argc >= 2 && argv[1][0] == 't');
    int fmt12 = 0;

    if (test) {
        struct systime t;
        draw(&g, 0);
        ugfx_flush(&g);
        if (sys_time(&t) == 0)
            uprintf("calendar: %s %d, %d\n", months[t.month - 1], t.day,
                    t.year);
        uprintf("calendar: ok\n");
        ugfx_free(&g);
        sys_exit(0);
    }

    int quit = 0, dirty = 1, idle = 0;
    while (!quit) {
        int k = sys_trygetkey();
        if (k >= 0) {
            if (k == 'q' || k == 27)
                quit = 1;
            else if (k == 't' || k == 'T')
                fmt12 = !fmt12;
            dirty = 1;
        }
        if (++idle >= 20) { /* ~1 s: tick the seconds */
            idle = 0;
            dirty = 1;
        }
        if (dirty) {
            draw(&g, fmt12);
            ugfx_flush(&g);
            dirty = 0;
        }
        sys_sleep(50);
    }
    ugfx_free(&g);
    sys_exit(0);
}
