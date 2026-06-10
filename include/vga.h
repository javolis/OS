/* vga.h — 80x25 VGA text-mode terminal. */
#pragma once

void terminal_initialize(void);
void terminal_putchar(char c);
void terminal_writestring(const char *s);
