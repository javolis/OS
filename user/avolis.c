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
    {"System", "sysmon.elf"},
    {"Settings", ""},
};
#define NBAR ((int)(sizeof(bar) / sizeof(bar[0])))
static const struct app pal[] = {
    {"graphics demo", "gfxdemo.elf"}, {"input demo", "inputdemo.elf"},
    {"system info", "sysinfo.elf"},   {"date", "date.elf"},
    {"files", "files.elf"},           {"network", "netcap.elf"},
    {"shell", "ush.elf"},             {"system monitor", "sysmon.elf"},
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

/* Settings categories (a sidebar, like a real OS settings app). */
enum {
    CAT_SYSTEM,
    CAT_DISPLAY,
    CAT_SOUND,
    CAT_NETWORK,
    CAT_PERSONAL,
    CAT_DATETIME,
    NCAT
};
static const char *cat_names[NCAT] = {"System",      "Display",  "Sound",
                                      "Network",     "Personalize",
                                      "Date & Time"};

/* Shell state (global so mouse and keyboard share the same actions). */
static int state = LOCK, pos = TB_BOTTOM, focus, sel, srow, wp;
static int scat;       /* selected settings category */
static int sdetail;    /* 0 = sidebar focused, 1 = detail pane focused */
static int timefmt;    /* 0 = 24-hour clock, 1 = 12-hour */
static char query[32];
static int qlen;
static int par_ox, par_oy; /* constellation parallax offset */

/* Number of interactive (changeable) rows in a category's detail pane. */
static int cat_rows(int c) {
    if (c == CAT_PERSONAL)
        return 2;
    if (c == CAT_DATETIME)
        return 1;
    return 0;
}

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
    if (sys_time(&t) != 0) {
        put_s(clk, "--:--")[0] = '\0';
        return;
    }
    int h = t.hour;
    if (timefmt) { /* 12-hour */
        int hh = h % 12;
        if (hh == 0)
            hh = 12;
        two(clk, hh);
        clk[2] = ':';
        two(clk + 3, t.minute);
        put_s(clk + 5, h < 12 ? " AM" : " PM")[0] = '\0';
    } else {
        two(clk, h);
        clk[2] = ':';
        two(clk + 3, t.minute);
        clk[5] = '\0';
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

/* --- system-info getters for the Settings app --- */
static void cpu_brand(char *out) {
    unsigned a, b, c, d;
    __asm__ volatile("cpuid"
                     : "=a"(a), "=b"(b), "=c"(c), "=d"(d)
                     : "a"(0x80000000u));
    if (a < 0x80000004u) {
        put_s(out, "x86 CPU")[0] = '\0';
        return;
    }
    unsigned regs[12];
    int i = 0;
    for (unsigned leaf = 0x80000002u; leaf <= 0x80000004u; leaf++) {
        __asm__ volatile("cpuid"
                         : "=a"(a), "=b"(b), "=c"(c), "=d"(d)
                         : "a"(leaf));
        regs[i++] = a;
        regs[i++] = b;
        regs[i++] = c;
        regs[i++] = d;
    }
    const char *src = (const char *)regs;
    while (*src == ' ')
        src++; /* Intel left-pads the brand string */
    int n = 0;
    while (src[n] && n < 47) {
        out[n] = src[n];
        n++;
    }
    out[n] = '\0';
}
static char *put_ip(char *p, unsigned ip) {
    p = put_i(p, (int)((ip >> 24) & 0xFF));
    *p++ = '.';
    p = put_i(p, (int)((ip >> 16) & 0xFF));
    *p++ = '.';
    p = put_i(p, (int)((ip >> 8) & 0xFF));
    *p++ = '.';
    p = put_i(p, (int)(ip & 0xFF));
    *p = '\0';
    return p;
}
static void mem_str(char *out) {
    struct sysinfo si;
    if (sys_sysinfo(&si) != 0) {
        put_s(out, "?")[0] = '\0';
        return;
    }
    unsigned total = si.total_frames / 256; /* *4 KiB / 1 MiB */
    unsigned used = (si.total_frames - si.free_frames) / 256;
    char *p = put_i(out, (int)used);
    p = put_s(p, " / ");
    p = put_i(p, (int)total);
    put_s(p, " MB")[0] = '\0';
}
static void uptime_str(char *out) {
    struct sysinfo si;
    if (sys_sysinfo(&si) != 0) {
        put_s(out, "?")[0] = '\0';
        return;
    }
    unsigned s = si.ticks / 100;
    char *p = put_i(out, (int)(s / 3600));
    *p++ = ':';
    two(p, (int)((s / 60) % 60));
    p += 2;
    *p++ = ':';
    two(p, (int)(s % 60));
    p += 2;
    *p = '\0';
}
static void settings_change(int c, int row, int dir) {
    if (c == CAT_PERSONAL) {
        if (row == 0) {
            wp = (wp + NWALLS + dir) % NWALLS;
            uprintf("avolis: wallpaper %s\n", walls[wp].name);
        } else {
            pos = (pos + 4 + dir) % 4;
            uprintf("avolis: taskbar %s\n", tb_names[pos]);
        }
    } else if (c == CAT_DATETIME) {
        timefmt = !timefmt;
        uprintf("avolis: timefmt %s\n", timefmt ? "12h" : "24h");
    }
}

/* Fill labels[]/values[] for a category's detail rows; returns the count.
 * The first cat_rows(c) of them are the interactive ones. */
static int settings_content(ugfx_t *g, int c, char labels[][24],
                            char values[][48]) {
    int n = 0;
    if (c == CAT_SYSTEM) {
        put_s(labels[n], "Edition");
        put_s(values[n++], "Avolis OS");
        put_s(labels[n], "Version");
        put_s(values[n++], "0.13");
        put_s(labels[n], "Kernel");
        put_s(values[n++], "javolis i686");
        put_s(labels[n], "Processor");
        cpu_brand(values[n++]);
        put_s(labels[n], "Memory");
        mem_str(values[n++]);
        put_s(labels[n], "Uptime");
        uptime_str(values[n++]);
    } else if (c == CAT_DISPLAY) {
        char *p;
        put_s(labels[n], "Resolution");
        p = put_i(values[n], (int)g->width);
        *p++ = ' ';
        *p++ = 'x';
        *p++ = ' ';
        p = put_i(p, (int)g->height);
        *p = '\0';
        n++;
        put_s(labels[n], "Color depth");
        p = put_i(values[n], (int)g->bpp);
        put_s(p, "-bit")[0] = '\0';
        n++;
        put_s(labels[n], "Set at");
        put_s(values[n++], "boot (GRUB)");
    } else if (c == CAT_SOUND) {
        int16_t probe;
        put_s(labels[n], "Output device");
        put_s(values[n++], sys_audio(&probe, 0) == 0 ? "AC'97 codec" : "None");
        put_s(labels[n], "Format");
        put_s(values[n++], "48 kHz 16-bit");
    } else if (c == CAT_NETWORK) {
        struct netinfo ni;
        int up = (sys_netinfo(&ni) == 0);
        put_s(labels[n], "Status");
        put_s(values[n++], up ? "Connected" : "Not connected");
        if (up) {
            put_s(labels[n], "IP address");
            put_ip(values[n++], ni.ip);
            put_s(labels[n], "Gateway");
            put_ip(values[n++], ni.gateway);
            put_s(labels[n], "DNS");
            put_ip(values[n++], ni.dns);
        }
    } else if (c == CAT_PERSONAL) {
        put_s(labels[n], "Wallpaper");
        put_s(values[n++], walls[wp].name);
        put_s(labels[n], "Taskbar");
        put_s(values[n++], tb_names[pos]);
    } else if (c == CAT_DATETIME) {
        struct systime t;
        char *p;
        if (sys_time(&t) == 0) {
            put_s(labels[n], "Date");
            p = put_s(values[n], months[(t.month >= 1 && t.month <= 12)
                                            ? t.month - 1
                                            : 0]);
            *p++ = ' ';
            p = put_i(p, t.day);
            *p++ = ',';
            *p++ = ' ';
            p = put_i(p, t.year);
            *p = '\0';
            n++;
            put_s(labels[n], "Time");
            two(values[n], t.hour);
            values[n][2] = ':';
            two(values[n] + 3, t.minute);
            values[n][5] = ':';
            two(values[n] + 6, t.second);
            values[n][8] = '\0';
            n++;
        }
        put_s(labels[n], "Time format");
        put_s(values[n++], timefmt ? "12-hour" : "24-hour");
    }
    return n;
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
    char clk[12];
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
    char clk[12];
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

/* Settings layout (shared by draw + hit-test). */
#define SET_SBX 36
#define SET_SBY 100
#define SET_SBW 210
#define SET_ROWH 48
#define SET_DROWH 52
static void set_detail_origin(ugfx_t *g, int *dx, int *dy) {
    (void)g;
    *dx = SET_SBX + SET_SBW + 36;
    *dy = SET_SBY + 8;
}

static void draw_settings(ugfx_t *g) {
    int W = (int)g->width, H = (int)g->height;
    wall(g);
    av_text_glow(g, UAFONT_HEAD, 40, 56, "Settings", AV_ORANGE);

    /* Sidebar of categories. */
    av_panel(g, SET_SBX, SET_SBY, SET_SBW, NCAT * SET_ROWH + 16, sdetail == 0);
    for (int i = 0; i < NCAT; i++) {
        int y = SET_SBY + 12 + i * SET_ROWH;
        if (i == scat)
            ugfx_round_rect(g, SET_SBX + 10, y, SET_SBW - 20, SET_ROWH - 6, 6,
                            AV_PANEL2);
        av_text(g, SET_SBX + 26, y + SET_ROWH - 20, cat_names[i],
                i == scat ? AV_ORANGE : AV_WHITE);
    }

    /* Detail pane for the selected category. */
    int dx, dy;
    set_detail_origin(g, &dx, &dy);
    av_head(g, dx, dy + 26, cat_names[scat], AV_WHITE);
    char labels[8][24], values[8][48];
    int n = settings_content(g, scat, labels, values);
    int ni = cat_rows(scat);
    int base = n - ni; /* the interactive rows are the trailing ni rows */
    for (int r = 0; r < n; r++) {
        int y = dy + 70 + r * SET_DROWH;
        int interactive = (r >= base);
        int focused = (sdetail == 1 && interactive && (r - base) == srow);
        if (focused)
            ugfx_round_rect(g, dx - 14, y - 2, W - dx - 40, SET_DROWH - 8, 8,
                            AV_PANEL2);
        av_text(g, dx, y + 26, labels[r], AV_GRAY);
        av_text(g, dx + 190, y + 26, values[r],
                focused ? AV_ORANGE : (interactive ? AV_WHITE : AV_GRAY));
        if (interactive)
            av_text(g, W - 72, y + 26, "<  >", focused ? AV_ORANGE : AV_DIM);
    }

    ua_text_center(g, UAFONT_BODY, 0, W, H - 44,
                   sdetail == 0
                       ? "up/down pick    enter open    esc desktop"
                       : "up/down row    a/d change    esc back",
                   AV_DIM);
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
            scat = CAT_SYSTEM;
            sdetail = 0;
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
        if (sdetail == 0) {
            if (cat_rows(scat) > 0) { /* open an interactive category */
                sdetail = 1;
                srow = 0;
            }
        } else {
            settings_change(scat, srow, +1);
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
        for (int i = 0; i < NCAT; i++) {
            int y = SET_SBY + 12 + i * SET_ROWH;
            if (inrect(mx, my, SET_SBX + 10, y, SET_SBW - 20, SET_ROWH - 6)) {
                scat = i;
                sdetail = 0;
            }
        }
        int ni = cat_rows(scat);
        if (ni > 0) {
            int dx, dy;
            set_detail_origin(g, &dx, &dy);
            char labels[8][24], values[8][48];
            int n = settings_content(g, scat, labels, values);
            int base = n - ni;
            for (int r = base; r < n; r++) {
                int y = dy + 70 + r * SET_DROWH;
                if (inrect(mx, my, dx - 14, y - 2, W - dx - 40,
                           SET_DROWH - 8)) {
                    sdetail = 1;
                    srow = r - base;
                }
            }
        }
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
            scat = CAT_SYSTEM;
            sdetail = 0;
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
        if (sdetail == 0) { /* sidebar */
            if (k == 27)
                state = DESKTOP;
            else if (k == K_UP)
                scat = (scat + NCAT - 1) % NCAT;
            else if (k == K_DOWN)
                scat = (scat + 1) % NCAT;
            else if (k == '\n' || k == '\r')
                activate(); /* open the category */
        } else { /* detail pane */
            int ni = cat_rows(scat);
            if (k == 27)
                sdetail = 0;
            else if (k == K_UP) {
                if (srow > 0)
                    srow--;
            } else if (k == K_DOWN) {
                if (srow < ni - 1)
                    srow++;
            } else if (k == 'a')
                settings_change(scat, srow, -1);
            else if (k == 'd' || k == '\n' || k == '\r')
                settings_change(scat, srow, +1);
        }
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
