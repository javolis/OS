/* irq.c — C-level hardware interrupt dispatch (vectors 32-47). */
#include <stddef.h>
#include <stdint.h>

#include "irq.h"
#include "pic.h"

static irq_handler_t handlers[16];

void irq_register(uint8_t irq, irq_handler_t handler) {
    if (irq < 16)
        handlers[irq] = handler;
}

void irq_handler(struct registers *regs);

void irq_handler(struct registers *regs) {
    uint8_t irq = (uint8_t)(regs->int_no - PIC_IRQ_BASE);

    /* Acknowledge BEFORE dispatching: the timer handler may context-switch
     * away mid-interrupt, and a pending EOI would silence that IRQ line
     * for good. Re-entry is still impossible here — IF stays clear until
     * some task irets. */
    pic_send_eoi(irq);

    if (irq < 16 && handlers[irq])
        handlers[irq](regs);
}
