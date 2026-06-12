/* pic.c — 8259 PIC: remap IRQs away from the CPU exception vectors.
 *
 * At power-on the master PIC delivers IRQ 0-7 on vectors 8-15, colliding
 * with CPU exceptions (a timer tick would look like a double fault). The
 * standard fix is to re-initialize both PICs with offsets 0x20/0x28. */
#include <stdint.h>

#include "io.h"
#include "pic.h"

#define PIC1_CMD 0x20
#define PIC1_DATA 0x21
#define PIC2_CMD 0xA0
#define PIC2_DATA 0xA1

#define ICW1_INIT 0x10
#define ICW1_ICW4 0x01
#define ICW4_8086 0x01
#define PIC_EOI 0x20

void pic_init(void) {
    /* ICW1: begin initialization, ICW4 will follow. */
    outb(PIC1_CMD, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC2_CMD, ICW1_INIT | ICW1_ICW4);
    io_wait();

    /* ICW2: vector offsets. */
    outb(PIC1_DATA, PIC_IRQ_BASE);
    io_wait();
    outb(PIC2_DATA, PIC_IRQ_BASE + 8);
    io_wait();

    /* ICW3: master has the slave on IRQ2; slave's cascade identity is 2. */
    outb(PIC1_DATA, 0x04);
    io_wait();
    outb(PIC2_DATA, 0x02);
    io_wait();

    /* ICW4: 8086 mode. */
    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();

    /* Mask every line; each device unmasks its own IRQ when it inits. */
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}

void pic_send_eoi(uint8_t irq) {
    if (irq >= 8)
        outb(PIC2_CMD, PIC_EOI);
    outb(PIC1_CMD, PIC_EOI);
}

void pic_unmask(uint8_t irq) {
    uint16_t port = irq < 8 ? PIC1_DATA : PIC2_DATA;
    outb(port, inb(port) & ~(uint8_t)(1 << (irq & 7)));
    /* A slave line also needs the master's cascade line (IRQ2) open. */
    if (irq >= 8)
        pic_unmask(2);
}

void pic_mask(uint8_t irq) {
    uint16_t port = irq < 8 ? PIC1_DATA : PIC2_DATA;
    outb(port, inb(port) | (uint8_t)(1 << (irq & 7)));
}
