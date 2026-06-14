/* term.h — text console (VGA text or linear framebuffer). */
#pragma once
#include <stdint.h>

void term_init(void);
void term_putchar(char c);
void term_write(const char *s);

/* Switch the console to the bootloader framebuffer (after fb_init). */
void term_use_framebuffer(void);

/* Console foreground color. On the framebuffer it is the 0x00RRGGBB pixel
 * color of subsequent glyphs; on legacy VGA text it is mapped to the
 * nearest of the 16 text attributes. term_reset_color restores the default
 * light-grey. Use the TERM_* palette so both backends agree. */
void term_set_color(uint32_t rgb);
void term_reset_color(void);

#define TERM_GREY   0x00CCCCCCu /* default */
#define TERM_WHITE  0x00FFFFFFu
#define TERM_RED    0x00FF5555u
#define TERM_GREEN  0x0055FF55u
#define TERM_CYAN   0x0055FFFFu
#define TERM_YELLOW 0x00FFFF55u
