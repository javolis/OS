/* term.h — VGA text-mode terminal. */
#pragma once

void term_init(void);
void term_putchar(char c);
void term_write(const char *s);

/* Switch the console to the bootloader framebuffer (after fb_init). */
void term_use_framebuffer(void);
