/* date.c - userland: print the wall-clock time from the RTC. */
#include "ulib.h"

static void pad2(char *p, int v) {
    p[0] = '0' + (v / 10) % 10;
    p[1] = '0' + v % 10;
}

void _start(void) {
    struct systime t;
    if (sys_time(&t) != 0) {
        uprintf("date: clock unavailable\n");
        sys_exit(1);
    }

    /* ISO-ish: YYYY-MM-DD HH:MM:SS with zero padding. */
    char buf[20] = "0000-00-00 00:00:00";
    buf[0] = '0' + (t.year / 1000) % 10;
    buf[1] = '0' + (t.year / 100) % 10;
    buf[2] = '0' + (t.year / 10) % 10;
    buf[3] = '0' + t.year % 10;
    pad2(buf + 5, t.month);
    pad2(buf + 8, t.day);
    pad2(buf + 11, t.hour);
    pad2(buf + 14, t.minute);
    pad2(buf + 17, t.second);

    uprintf("date: %s\n", buf);
    sys_exit(0);
}
