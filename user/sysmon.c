/* sysmon.c - Avolis System Monitor: a live task manager over SYS_PS.
 *
 * Shows memory, uptime and the process table (PID / state / name), refreshing
 * about once a second. Up/down selects a task, 'k' kills it (never the kernel
 * shell or itself), q/esc quits. 'sysmon.elf test' enumerates once, prints a
 * summary and exits so CI can verify it over serial. */
#include "avui.h"

#define MAXP 40

static struct procinfo procs[MAXP];
static int nprocs;

static const char *state_name(unsigned s) {
    static const char *n[8] = {"free",    "ready",    "blocked", "waitkbd",
                               "waitpid", "waitchan", "zombie",  "running"};
    return s < 8 ? n[s] : "?";
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
static char *put_s(char *p, const char *s) {
    while (*s)
        *p++ = *s++;
    return p;
}
static void two(char *b, unsigned v) {
    b[0] = (char)('0' + (v / 10) % 10);
    b[1] = (char)('0' + v % 10);
}

static int enumerate(void) {
    int n = 0;
    while (n < MAXP && sys_ps(n, &procs[n]) == 0)
        n++;
    return n;
}

static void mem_str(char *out) {
    struct sysinfo si;
    if (sys_sysinfo(&si) != 0) {
        put_s(out, "?")[0] = '\0';
        return;
    }
    char *p = put_u(out, si.total_frames ? (si.total_frames - si.free_frames) / 256 : 0);
    p = put_s(p, " / ");
    p = put_u(p, si.total_frames / 256);
    put_s(p, " MB")[0] = '\0';
}
static void uptime_str(char *out) {
    struct sysinfo si;
    if (sys_sysinfo(&si) != 0) {
        put_s(out, "?")[0] = '\0';
        return;
    }
    unsigned s = si.ticks / 100;
    char *p = put_u(out, s / 3600);
    *p++ = ':';
    two(p, (s / 60) % 60);
    p += 2;
    *p++ = ':';
    two(p, s % 60);
    p += 2;
    *p = '\0';
}

static void draw(ugfx_t *g, int sel) {
    int W = (int)g->width, H = (int)g->height;
    ugfx_clear(g, AV_BG);
    av_text_glow(g, UAFONT_HEAD, 40, 56, "System Monitor", AV_ORANGE);

    char buf[48];
    mem_str(buf);
    av_text(g, 40, 96, "Memory", AV_GRAY);
    av_text(g, 140, 96, buf, AV_WHITE);
    uptime_str(buf);
    av_text(g, 320, 96, "Uptime", AV_GRAY);
    av_text(g, 420, 96, buf, AV_WHITE);
    char *p = put_u(buf, (unsigned)nprocs);
    put_s(p, " tasks")[0] = '\0';
    av_text(g, 600, 96, buf, AV_WHITE);

    int x = 40, y0 = 130, rowh = 38;
    av_panel(g, x, y0, W - 80, (nprocs + 1) * rowh + 16, 1);
    av_text(g, x + 28, y0 + 28, "PID", AV_DIM);
    av_text(g, x + 110, y0 + 28, "STATE", AV_DIM);
    av_text(g, x + 260, y0 + 28, "NAME", AV_DIM);
    for (int i = 0; i < nprocs; i++) {
        int y = y0 + (i + 1) * rowh + 6;
        if (i == sel)
            ugfx_round_rect(g, x + 10, y - 22, W - 100, rowh - 4, 6, AV_PANEL2);
        char pid[12];
        put_u(pid, procs[i].pid)[0] = '\0';
        unsigned col = (i == sel) ? AV_ORANGE : AV_WHITE;
        av_text(g, x + 28, y, pid, col);
        av_text(g, x + 110, y, state_name(procs[i].state), AV_GRAY);
        av_text(g, x + 260, y, procs[i].name, col);
    }
    ua_text_center(g, UAFONT_BODY, 0, W, H - 40,
                   "up/down select    k end task    q quit", AV_DIM);
}

void _start(int argc, char **argv) {
    ugfx_t g;
    if (ugfx_init(&g) != 0) {
        uprintf("sysmon: no framebuffer\n");
        sys_exit(0);
    }
    int test = (argc >= 2 && argv[1][0] == 't');
    nprocs = enumerate();

    if (test) {
        int self = sys_getpid(), saw_self = 0, saw_shell = 0;
        for (int i = 0; i < nprocs; i++) {
            if (procs[i].pid == 0)
                saw_shell = 1;
            if ((int)procs[i].pid == self)
                saw_self = 1;
        }
        uprintf("sysmon: listed %d tasks\n", nprocs);
        draw(&g, 0);
        ugfx_flush(&g);
        if (saw_shell && saw_self)
            uprintf("sysmon: ok\n");
        else
            uprintf("sysmon: FAIL (shell=%d self=%d)\n", saw_shell, saw_self);
        ugfx_free(&g);
        sys_exit(0);
    }

    int sel = 0, quit = 0, dirty = 1, idle = 0;
    int self = sys_getpid();
    while (!quit) {
        int k = sys_trygetkey();
        if (k >= 0) {
            if (k == 'q' || k == 27)
                quit = 1;
            else if (k == 0x80) {
                if (sel > 0)
                    sel--;
            } else if (k == 0x81) {
                if (sel < nprocs - 1)
                    sel++;
            } else if (k == 'k') {
                if (sel < nprocs && procs[sel].pid != 0 &&
                    (int)procs[sel].pid != self)
                    sys_kill((int)procs[sel].pid);
            }
            dirty = 1;
        }
        if (++idle >= 20) { /* ~1 s live refresh */
            idle = 0;
            nprocs = enumerate();
            if (sel >= nprocs)
                sel = nprocs - 1;
            if (sel < 0)
                sel = 0;
            dirty = 1;
        }
        if (dirty) {
            draw(&g, sel);
            ugfx_flush(&g);
            dirty = 0;
        }
        sys_sleep(50);
    }
    ugfx_free(&g);
    sys_exit(0);
}
