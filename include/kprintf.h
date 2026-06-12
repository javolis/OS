/* kprintf.h — minimal printf for kernel diagnostics.
 * Writes to both the VGA terminal and COM1 serial.
 * Supports %c %s %d %u %x %p %% with optional zero/space width (e.g. %08x)
 * and the 'l' length modifier (use %lx/%lu for uint32_t, which the i686-elf
 * toolchain defines as unsigned long). */
#pragma once
#include <stdarg.h>

void kprintf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void kvprintf(const char *fmt, va_list ap);
