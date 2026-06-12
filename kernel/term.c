/* term.c — VGA text-mode terminal: 80x25, memory-mapped at 0xB8000. */
#include <stddef.h>
#include <stdint.h>

#include "term.h"

static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;
static volatile uint16_t *const VGA_MEMORY = (uint16_t *)0xB8000;

/* VGA color attribute: foreground=white, background=black. */
static const uint8_t VGA_COLOR = 0x0F;

static size_t term_row;
static size_t term_col;

static inline uint16_t vga_entry(char c, uint8_t color) {
    return (uint16_t)c | (uint16_t)color << 8;
}

/* Shift every row up by one and blank the bottom row. */
static void term_scroll(void) {
    for (size_t y = 1; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            VGA_MEMORY[(y - 1) * VGA_WIDTH + x] = VGA_MEMORY[y * VGA_WIDTH + x];
    for (size_t x = 0; x < VGA_WIDTH; x++)
        VGA_MEMORY[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', VGA_COLOR);
    term_row = VGA_HEIGHT - 1;
}

void term_init(void) {
    for (size_t y = 0; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            VGA_MEMORY[y * VGA_WIDTH + x] = vga_entry(' ', VGA_COLOR);
    term_row = 0;
    term_col = 0;
}

void term_putchar(char c) {
    if (c == '\n') {
        term_col = 0;
        if (++term_row == VGA_HEIGHT)
            term_scroll();
        return;
    }
    VGA_MEMORY[term_row * VGA_WIDTH + term_col] = vga_entry(c, VGA_COLOR);
    if (++term_col == VGA_WIDTH) {
        term_col = 0;
        if (++term_row == VGA_HEIGHT)
            term_scroll();
    }
}

void term_write(const char *s) {
    for (size_t i = 0; s[i]; i++)
        term_putchar(s[i]);
}
