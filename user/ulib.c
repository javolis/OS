/* ulib.c - tiny userland library. */
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

void *umemset(void *dst, int c, uint32_t n) {
    uint8_t *d = dst;
    for (uint32_t i = 0; i < n; i++)
        d[i] = (uint8_t)c;
    return dst;
}

void *umemcpy(void *dst, const void *src, uint32_t n) {
    uint8_t *d = dst;
    const uint8_t *s = src;
    for (uint32_t i = 0; i < n; i++)
        d[i] = s[i];
    return dst;
}

void *umemmove(void *dst, const void *src, uint32_t n) {
    uint8_t *d = dst;
    const uint8_t *s = src;
    if (d < s) {
        for (uint32_t i = 0; i < n; i++)
            d[i] = s[i];
    } else {
        for (uint32_t i = n; i > 0; i--)
            d[i - 1] = s[i - 1];
    }
    return dst;
}

int ustrcmp(const char *a, const char *b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

int ustrncmp(const char *a, const char *b, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) {
        if (a[i] != b[i])
            return (int)(unsigned char)a[i] - (int)(unsigned char)b[i];
        if (a[i] == '\0')
            return 0;
    }
    return 0;
}

char *ustrcpy(char *dst, const char *src) {
    char *d = dst;
    while ((*d++ = *src++))
        ;
    return dst;
}

char *ustrncpy(char *dst, const char *src, uint32_t n) {
    uint32_t i = 0;
    for (; i < n && src[i]; i++)
        dst[i] = src[i];
    for (; i < n; i++)
        dst[i] = '\0';
    return dst;
}

char *ustrcat(char *dst, const char *src) {
    char *d = dst;
    while (*d)
        d++;
    while ((*d++ = *src++))
        ;
    return dst;
}

char *ustrchr(const char *s, int c) {
    for (; *s; s++)
        if (*s == (char)c)
            return (char *)s;
    return (c == '\0') ? (char *)s : 0;
}

int uatoi(const char *s) {
    int sign = 1, v = 0;
    while (*s == ' ')
        s++;
    if (*s == '-') {
        sign = -1;
        s++;
    } else if (*s == '+') {
        s++;
    }
    while (*s >= '0' && *s <= '9')
        v = v * 10 + (*s++ - '0');
    return sign * v;
}

/* --- heap allocator: first-fit free list over sys_sbrk --- */
struct ublock {
    uint32_t size; /* payload bytes, excluding this header */
    uint32_t free;
    struct ublock *next; /* next block by address */
};

#define UHDR ((uint32_t)sizeof(struct ublock))
#define UMIN_SPLIT (UHDR + 8)

static struct ublock *uheap;

/* Grow the heap by at least min_bytes (page-rounded) via sbrk, appending a
 * free block. sbrk hands back contiguous regions, so the new block abuts
 * the old tail and the list stays address-ordered and gap-free. */
static int ugrow(uint32_t min_bytes) {
    uint32_t bytes = (min_bytes + 4095u) & ~4095u;
    void *p = sys_sbrk((int)bytes);
    if (p == (void *)-1)
        return 0;
    struct ublock *nb = (struct ublock *)p;
    nb->size = bytes - UHDR;
    nb->free = 1;
    nb->next = 0;
    if (!uheap) {
        uheap = nb;
        return 1;
    }
    struct ublock *t = uheap;
    while (t->next)
        t = t->next;
    t->next = nb;
    if (t->free) { /* adjacent by construction: merge */
        t->size += UHDR + nb->size;
        t->next = 0;
    }
    return 1;
}

void *umalloc(uint32_t size) {
    if (size == 0)
        return 0;
    size = (size + 3u) & ~3u; /* 4-byte aligned payloads */

    for (;;) {
        for (struct ublock *b = uheap; b; b = b->next) {
            if (!b->free || b->size < size)
                continue;
            if (b->size >= size + UMIN_SPLIT) {
                struct ublock *nb =
                    (struct ublock *)((uint8_t *)b + UHDR + size);
                nb->size = b->size - size - UHDR;
                nb->free = 1;
                nb->next = b->next;
                b->size = size;
                b->next = nb;
            }
            b->free = 0;
            return (uint8_t *)b + UHDR;
        }
        if (!ugrow(size + UHDR))
            return 0;
    }
}

void ufree(void *ptr) {
    if (!ptr)
        return;
    struct ublock *b = (struct ublock *)((uint8_t *)ptr - UHDR);
    b->free = 1;
    /* address-ordered, gap-free: merge every run of free neighbours. */
    for (struct ublock *cur = uheap; cur; cur = cur->next)
        while (cur->free && cur->next && cur->next->free) {
            cur->size += UHDR + cur->next->size;
            cur->next = cur->next->next;
        }
}

/* --- stdio-lite --- */
void uputc(char c) {
    sys_writefd(1, &c, 1);
}

void uputs(const char *s) {
    sys_writefd(1, s, (int)ustrlen(s));
    sys_writefd(1, "\n", 1);
}

int ugetline(char *buf, int n) {
    return sys_read(0, buf, n); /* fd 0 returns one edited line */
}

struct ufile {
    int fd;
    int pos;
    int len;
    char buf[256];
};

struct ufile *ufopen(const char *name) {
    int fd = sys_open(name);
    if (fd < 0)
        return 0;
    struct ufile *f = umalloc(sizeof(struct ufile));
    if (!f) {
        sys_close(fd);
        return 0;
    }
    f->fd = fd;
    f->pos = 0;
    f->len = 0;
    return f;
}

int ufgetc(struct ufile *f) {
    if (f->pos >= f->len) {
        f->len = sys_read(f->fd, f->buf, (int)sizeof(f->buf));
        f->pos = 0;
        if (f->len <= 0)
            return -1; /* EOF or error */
    }
    return (unsigned char)f->buf[f->pos++];
}

char *ufgets(char *s, int n, struct ufile *f) {
    int i = 0;
    int c;
    while (i < n - 1 && (c = ufgetc(f)) >= 0) {
        s[i++] = (char)c;
        if (c == '\n')
            break;
    }
    if (i == 0)
        return 0; /* nothing read: EOF */
    s[i] = '\0';
    return s;
}

void ufclose(struct ufile *f) {
    if (!f)
        return;
    sys_close(f->fd);
    ufree(f);
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
