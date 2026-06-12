/* kprintf.c — minimal printf for kernel diagnostics.
 * Output goes to both the VGA terminal and COM1 serial so messages are
 * visible on-screen and in headless (CI) captures alike. */
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include "io.h"
#include "kprintf.h"
#include "serial.h"
#include "term.h"

static void putchar_both(char c) {
    term_putchar(c);
    serial_putchar(c);
}

static void write_both(const char *s) {
    for (size_t i = 0; s[i]; i++)
        putchar_both(s[i]);
}

static void print_uint(uint32_t value, uint32_t base, int width, char pad) {
    static const char digits[] = "0123456789abcdef";
    char buf[32]; /* fits a 32-bit value in any base >= 2, plus padding */
    int len = 0;

    do {
        buf[len++] = digits[value % base];
        value /= base;
    } while (value);

    while (len < width && len < (int)sizeof(buf))
        buf[len++] = pad;

    while (len--)
        putchar_both(buf[len]);
}

void kvprintf(const char *fmt, va_list ap) {
    /* Whole-call atomicity: with the scheduler preempting between tasks,
     * this keeps kernel and user output from interleaving mid-line. */
    uint32_t flags = irq_save();

    for (size_t i = 0; fmt[i]; i++) {
        if (fmt[i] != '%') {
            putchar_both(fmt[i]);
            continue;
        }

        /* Optional zero-pad flag and field width, e.g. %08x. */
        i++;
        char pad = ' ';
        int width = 0;
        if (fmt[i] == '0') {
            pad = '0';
            i++;
        }
        while (fmt[i] >= '0' && fmt[i] <= '9') {
            width = width * 10 + (fmt[i] - '0');
            i++;
        }

        /* 'l' length modifier (long == 32-bit on i686, so same handling). */
        int is_long = 0;
        if (fmt[i] == 'l') {
            is_long = 1;
            i++;
        }

        switch (fmt[i]) {
        case 'c':
            putchar_both((char)va_arg(ap, int));
            break;
        case 's': {
            const char *s = va_arg(ap, const char *);
            write_both(s ? s : "(null)");
            break;
        }
        case 'd': {
            long v = is_long ? va_arg(ap, long) : va_arg(ap, int);
            if (v < 0) {
                putchar_both('-');
                /* Negate as 64-bit so INT32_MIN doesn't overflow. */
                print_uint((uint32_t)(-(int64_t)v), 10, width, pad);
            } else {
                print_uint((uint32_t)v, 10, width, pad);
            }
            break;
        }
        case 'u':
            print_uint(is_long ? (uint32_t)va_arg(ap, unsigned long)
                               : va_arg(ap, unsigned int),
                       10, width, pad);
            break;
        case 'x':
            print_uint(is_long ? (uint32_t)va_arg(ap, unsigned long)
                               : va_arg(ap, unsigned int),
                       16, width, pad);
            break;
        case 'p':
            write_both("0x");
            print_uint((uint32_t)va_arg(ap, void *), 16, 8, '0');
            break;
        case '%':
            putchar_both('%');
            break;
        case '\0': /* string ended mid-specifier */
            goto out;
        default: /* unknown specifier: echo it verbatim */
            putchar_both('%');
            putchar_both(fmt[i]);
            break;
        }
    }
out:
    irq_restore(flags);
}

void kprintf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    kvprintf(fmt, ap);
    va_end(ap);
}
