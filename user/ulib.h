/* ulib.h — tiny userland library: string helpers and formatted output. */
#pragma once
#include <stdint.h>

#include "usys.h"

uint32_t ustrlen(const char *s);
int ustreq(const char *a, const char *b);

/* mem/string suite (freestanding mini-libc). */
void *umemset(void *dst, int c, uint32_t n);
void *umemcpy(void *dst, const void *src, uint32_t n);
void *umemmove(void *dst, const void *src, uint32_t n);
int ustrcmp(const char *a, const char *b);
int ustrncmp(const char *a, const char *b, uint32_t n);
char *ustrcpy(char *dst, const char *src);
char *ustrncpy(char *dst, const char *src, uint32_t n);
char *ustrcat(char *dst, const char *src);
char *ustrchr(const char *s, int c);
int uatoi(const char *s);

/* printf-lite to the console (one atomic sys_write per call).
 * Supports %s %c %d %u %x %%. */
void uprintf(const char *fmt, ...);

/* Heap allocator over sys_sbrk: a first-fit free list with split/coalesce. */
void *umalloc(uint32_t size); /* NULL on exhaustion or size 0 */
void ufree(void *ptr);        /* NULL is a no-op */

/* stdio-lite. Console output goes to fd 1; line input from fd 0. */
void uputc(char c);
void uputs(const char *s); /* writes s then a newline */
int ugetline(char *buf, int n); /* one edited stdin line; length or -1 */

/* Buffered file reader over the fd layer. */
struct ufile;
struct ufile *ufopen(const char *name); /* read-only; NULL on failure */
int ufgetc(struct ufile *f);            /* next byte, or -1 at EOF */
char *ufgets(char *s, int n, struct ufile *f); /* a line (keeps '\n'); NULL
                                                * at EOF with nothing read */
void ufclose(struct ufile *f);
