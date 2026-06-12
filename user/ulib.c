/* ulib.c — tiny userland library. */
#include <stdarg.h>
#include <stdint.h>

#include "ulib.h"

uint32_t ustrlen(const char *s) {
    uint32_t n = 0;
    while (s[n])
        n++;
    return n;
}

int ustreq(const char *a, const char *b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

static char *emit_uint(char *p, const char *end, uint32_t v, uint32_t base) {
    char tmp[12];
    int n = 0;
    do {
        tmp[n++] = "0123456789abcdef"[v % base];
        v /= base;
    } while (v);
    while (n-- > 0 && p < end)
        *p++ = tmp[n];
    return p;
}

void uprintf(const char *fmt, ...) {
    char buf[256];
    char *p = buf;
    const char *end = buf + sizeof(buf) - 1; /* reserve the NUL slot */
    va_list ap;

    va_start(ap, fmt);
    for (uint32_t i = 0; fmt[i] && p < end; i++) {
        if (fmt[i] != '%') {
            *p++ = fmt[i];
            continue;
        }
        i++;
        switch (fmt[i]) {
        case 's': {
            const char *s = va_arg(ap, const char *);
            while (*s && p < end)
                *p++ = *s++;
            break;
        }
        case 'c':
            *p++ = (char)va_arg(ap, int);
            break;
        case 'd': {
            int v = va_arg(ap, int);
            uint32_t u;
            if (v < 0) {
                *p++ = '-';
                u = (uint32_t) - (int64_t)v; /* INT_MIN-safe */
            } else {
                u = (uint32_t)v;
            }
            p = emit_uint(p, end, u, 10);
            break;
        }
        case 'u':
            p = emit_uint(p, end, va_arg(ap, uint32_t), 10);
            break;
        case 'x':
            p = emit_uint(p, end, va_arg(ap, uint32_t), 16);
            break;
        case '%':
            *p++ = '%';
            break;
        case '\0': /* trailing % */
            i--;
            break;
        default: /* unknown: emit verbatim */
            *p++ = '%';
            if (p < end)
                *p++ = fmt[i];
            break;
        }
    }
    va_end(ap);
    *p = '\0';
    sys_write(buf);
}
