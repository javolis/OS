/* kernel.c — freestanding kernel entry. Writes to the VGA text buffer. */
#include <stddef.h>
#include <stdint.h>

#include "io.h"

#if defined(__linux__)
#error "This kernel must be built with a cross-compiler, not the host toolchain."
#endif

/* Hardware VGA text mode: 80x25, memory-mapped at 0xB8000. */
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

static void term_clear(void) {
    for (size_t y = 0; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            VGA_MEMORY[y * VGA_WIDTH + x] = vga_entry(' ', VGA_COLOR);
    term_row = 0;
    term_col = 0;
}

static void term_putchar(char c) {
    if (c == '\n') {
        term_col = 0;
        if (++term_row == VGA_HEIGHT)
            term_row = 0;
        return;
    }
    VGA_MEMORY[term_row * VGA_WIDTH + term_col] = vga_entry(c, VGA_COLOR);
    if (++term_col == VGA_WIDTH) {
        term_col = 0;
        if (++term_row == VGA_HEIGHT)
            term_row = 0;
    }
}

static void term_write(const char *s) {
    for (size_t i = 0; s[i]; i++)
        term_putchar(s[i]);
}

/* COM1 serial port — used for headless output (e.g. in CI / QEMU). */
#define COM1 0x3F8

static void serial_init(void) {
    outb(COM1 + 1, 0x00); /* disable interrupts */
    outb(COM1 + 3, 0x80); /* enable DLAB to set baud divisor */
    outb(COM1 + 0, 0x03); /* divisor low  (38400 baud) */
    outb(COM1 + 1, 0x00); /* divisor high */
    outb(COM1 + 3, 0x03); /* 8 bits, no parity, one stop bit */
    outb(COM1 + 2, 0xC7); /* enable + clear FIFO, 14-byte threshold */
    outb(COM1 + 4, 0x0B); /* IRQs enabled, RTS/DSR set */
}

static void serial_putchar(char c) {
    while ((inb(COM1 + 5) & 0x20) == 0) /* wait for transmit buffer empty */
        ;
    outb(COM1, (uint8_t)c);
}

static void serial_write(const char *s) {
    for (size_t i = 0; s[i]; i++)
        serial_putchar(s[i]);
}

/* Ask QEMU's isa-debug-exit device to power off. Harmless on real hardware
 * (the port write is simply ignored when the device is absent). */
static void qemu_exit(uint8_t code) {
    outl(0xF4, code);
}

void kernel_main(void) {
    term_clear();
    serial_init();

    term_write("Hello from the kernel!\n");
    /* Marker the headless smoke test greps for; keep in sync with test/smoke.sh. */
    serial_write("KERNEL_BOOT_OK\n");
    term_write("Boot succeeded. Halting.\n");

    qemu_exit(0);
}
