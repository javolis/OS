/* term.c — VGA text-mode terminal: 80x25, memory-mapped at 0xB8000, with
 * scrolling and a hardware cursor. */
#include <stddef.h>
#include <stdint.h>

#include "io.h"
#include "memlayout.h"
#include "term.h"

static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;
/* The VGA buffer sits at physical 0xB8000, reached via the higher-half
 * offset mapping. */
static volatile uint16_t *const VGA_MEMORY =
    (uint16_t *)(KERNEL_VIRT_BASE + 0xB8000u);

/* VGA color attribute: foreground=white, background=black. */
static const uint8_t VGA_COLOR = 0x0F;

static size_t term_row;
static size_t term_col;

static inline uint16_t vga_entry(char c, uint8_t color) {
    return (uint16_t)(uint8_t)c | (uint16_t)color << 8;
}

/* Move the blinking hardware cursor to the current position via the CRT
 * controller's cursor-location registers. */
static void update_cursor(void) {
    uint16_t pos = (uint16_t)(term_row * VGA_WIDTH + term_col);
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
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
    update_cursor();
}

static void term_newline(void) {
    term_col = 0;
    if (++term_row == VGA_HEIGHT)
        term_scroll();
}

void term_putchar(char c) {
    switch (c) {
    case '\n':
        term_newline();
        break;
    case '\r':
        term_col = 0;
        break;
    case '\b':
        if (term_col > 0) {
            term_col--;
            VGA_MEMORY[term_row * VGA_WIDTH + term_col] =
                vga_entry(' ', VGA_COLOR);
        }
        break;
    case '\t': /* advance to the next 8-column tab stop */
        term_col = (term_col + 8) & ~(size_t)7;
        if (term_col >= VGA_WIDTH)
            term_newline();
        break;
    default:
        VGA_MEMORY[term_row * VGA_WIDTH + term_col] = vga_entry(c, VGA_COLOR);
        if (++term_col == VGA_WIDTH)
            term_newline();
        break;
    }
    update_cursor();
}

void term_write(const char *s) {
    for (size_t i = 0; s[i]; i++)
        term_putchar(s[i]);
}
