/* serial.c — COM1 serial port, used for headless output (e.g. in CI / QEMU). */
#include <stddef.h>
#include <stdint.h>

#include "io.h"
#include "serial.h"

#define COM1 0x3F8

void serial_init(void) {
    outb(COM1 + 1, 0x00); /* disable interrupts */
    outb(COM1 + 3, 0x80); /* enable DLAB to set baud divisor */
    outb(COM1 + 0, 0x03); /* divisor low  (38400 baud) */
    outb(COM1 + 1, 0x00); /* divisor high */
    outb(COM1 + 3, 0x03); /* 8 bits, no parity, one stop bit */
    outb(COM1 + 2, 0xC7); /* enable + clear FIFO, 14-byte threshold */
    outb(COM1 + 4, 0x0B); /* IRQs enabled, RTS/DSR set */
}

void serial_putchar(char c) {
    while ((inb(COM1 + 5) & 0x20) == 0) /* wait for transmit buffer empty */
        ;
    outb(COM1, (uint8_t)c);
}

void serial_write(const char *s) {
    for (size_t i = 0; s[i]; i++)
        serial_putchar(s[i]);
}
