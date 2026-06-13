/* term.c — text console. Two backends behind one interface:
 *   - legacy VGA text (80x25 at 0xB8000), the boot default, and
 *   - a linear-framebuffer glyph console (8x8 font), switched on once the
 *     bootloader framebuffer is mapped (term_use_framebuffer).
 * Early boot messages use VGA (mirrored to serial); after the switch,
 * everything renders on the framebuffer. */
#include <stddef.h>
#include <stdint.h>

#include "fb.h"
#include "io.h"
#include "memlayout.h"
#include "term.h"

/* ---- legacy VGA text backend ---- */
static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;
static volatile uint16_t *const VGA_MEMORY =
    (uint16_t *)(KERNEL_VIRT_BASE + 0xB8000u);
static const uint8_t VGA_COLOR = 0x0F;
static size_t term_row, term_col;

/* ---- framebuffer glyph backend ---- */
#define FB_FG 0x00CCCCCC /* light grey on black */
#define FB_BG 0x00000000
static int use_fb;
static uint32_t fb_cols, fb_rows; /* console size in 8x8 cells */
static uint32_t cx, cy;           /* cursor cell */

static inline uint16_t vga_entry(char c, uint8_t color) {
    return (uint16_t)(uint8_t)c | (uint16_t)color << 8;
}

static void update_cursor(void) {
    uint16_t pos = (uint16_t)(term_row * VGA_WIDTH + term_col);
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

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

/* Switch output to the (already-mapped) framebuffer and clear it. */
void term_use_framebuffer(void) {
    if (!fb_available())
        return;
    use_fb = 1;
    fb_cols = fb_width() / 8;
    fb_rows = fb_height() / 8;
    cx = cy = 0;
    fb_fill(FB_BG);
}

static void fb_newline(void) {
    cx = 0;
    if (++cy == fb_rows) {
        fb_scroll(8, FB_BG);
        cy = fb_rows - 1;
    }
}

static void fb_putchar(char c) {
    switch (c) {
    case '\n':
        fb_newline();
        break;
    case '\r':
        cx = 0;
        break;
    case '\b':
        if (cx > 0) {
            cx--;
            fb_draw_glyph(cx * 8, cy * 8, ' ', FB_FG, FB_BG);
        }
        break;
    case '\t':
        cx = (cx + 8) & ~7u;
        if (cx >= fb_cols)
            fb_newline();
        break;
    default:
        fb_draw_glyph(cx * 8, cy * 8, c, FB_FG, FB_BG);
        if (++cx == fb_cols)
            fb_newline();
        break;
    }
}

static void term_newline(void) {
    term_col = 0;
    if (++term_row == VGA_HEIGHT)
        term_scroll();
}

void term_putchar(char c) {
    if (use_fb) {
        fb_putchar(c);
        return;
    }
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
    case '\t':
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
