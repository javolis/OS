/* vga.c — VGA text-mode terminal with scrolling and a hardware cursor. */
#include "vga.h"
#include "io.h"
#include <stddef.h>
#include <stdint.h>

static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;
static volatile uint16_t *const VGA_MEMORY = (uint16_t *)0xB8000;

/* foreground=light grey, background=black */
static const uint8_t VGA_COLOR = 0x07;

static size_t term_row;
static size_t term_col;

static inline uint16_t vga_entry(char c) {
    return (uint16_t)(uint8_t)c | (uint16_t)VGA_COLOR << 8;
}

/* Move the blinking hardware cursor to (row, col) via the CRT controller. */
static void update_cursor(void) {
    uint16_t pos = (uint16_t)(term_row * VGA_WIDTH + term_col);
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void terminal_initialize(void) {
    for (size_t y = 0; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            VGA_MEMORY[y * VGA_WIDTH + x] = vga_entry(' ');
    term_row = 0;
    term_col = 0;
    update_cursor();
}

/* Shift every row up by one and blank the last line. */
static void scroll(void) {
    for (size_t y = 1; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            VGA_MEMORY[(y - 1) * VGA_WIDTH + x] = VGA_MEMORY[y * VGA_WIDTH + x];
    for (size_t x = 0; x < VGA_WIDTH; x++)
        VGA_MEMORY[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ');
    term_row = VGA_HEIGHT - 1;
}

static void newline(void) {
    term_col = 0;
    if (++term_row == VGA_HEIGHT)
        scroll();
}

void terminal_putchar(char c) {
    switch (c) {
    case '\n':
        newline();
        break;
    case '\r':
        term_col = 0;
        break;
    case '\b':
        if (term_col > 0) {
            term_col--;
            VGA_MEMORY[term_row * VGA_WIDTH + term_col] = vga_entry(' ');
        }
        break;
    case '\t':
        term_col = (term_col + 8) & ~(size_t)7;
        if (term_col >= VGA_WIDTH)
            newline();
        break;
    default:
        VGA_MEMORY[term_row * VGA_WIDTH + term_col] = vga_entry(c);
        if (++term_col == VGA_WIDTH)
            newline();
        break;
    }
    update_cursor();
}

void terminal_writestring(const char *s) {
    for (size_t i = 0; s[i]; i++)
        terminal_putchar(s[i]);
}
