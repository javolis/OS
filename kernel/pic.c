/* pic.c — 8259A PIC initialization and acknowledgement. */
#include "pic.h"
#include "io.h"
#include <stdint.h>

#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1

#define ICW1_INIT 0x11 /* begin init, expect ICW4 */
#define ICW4_8086 0x01 /* 8086/88 mode */
#define PIC_EOI   0x20

void pic_remap(int off1, int off2) {
    outb(PIC1_CMD, ICW1_INIT);
    outb(PIC2_CMD, ICW1_INIT);
    outb(PIC1_DATA, (uint8_t)off1); /* master vector offset */
    outb(PIC2_DATA, (uint8_t)off2); /* slave vector offset  */
    outb(PIC1_DATA, 0x04);          /* tell master: slave at IRQ2 */
    outb(PIC2_DATA, 0x02);          /* tell slave its cascade identity */
    outb(PIC1_DATA, ICW4_8086);
    outb(PIC2_DATA, ICW4_8086);
}

void pic_set_masks(uint8_t master_mask, uint8_t slave_mask) {
    outb(PIC1_DATA, master_mask);
    outb(PIC2_DATA, slave_mask);
}

void pic_eoi(uint8_t irq) {
    if (irq >= 8)
        outb(PIC2_CMD, PIC_EOI);
    outb(PIC1_CMD, PIC_EOI);
}
