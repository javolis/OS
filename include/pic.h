/* pic.h — 8259A Programmable Interrupt Controller. */
#pragma once
#include <stdint.h>

/* Remap the master/slave PICs so IRQs land at off1/off2 instead of
 * colliding with CPU exception vectors. */
void pic_remap(int off1, int off2);

/* Send end-of-interrupt for the given IRQ line (0-15). */
void pic_eoi(uint8_t irq);

/* Set the interrupt mask: a 1 bit disables that IRQ line. */
void pic_set_masks(uint8_t master_mask, uint8_t slave_mask);
