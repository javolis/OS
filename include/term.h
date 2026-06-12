/* term.h — VGA text-mode terminal. */
#pragma once

void term_init(void);
void term_putchar(char c);
void term_write(const char *s);
