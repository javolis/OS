/* ulib.h — tiny userland library: string helpers and formatted output. */
#pragma once
#include <stdint.h>

#include "usys.h"

uint32_t ustrlen(const char *s);
int ustreq(const char *a, const char *b);

/* printf-lite to the console (one atomic sys_write per call).
 * Supports %s %c %d %u %x %%. */
void uprintf(const char *fmt, ...);
