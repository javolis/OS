/* pic.h — 8259 Programmable Interrupt Controller. */
#pragma once
#include <stdint.h>

/* Hardware IRQs are remapped to vectors 32-47 (0x20-0x2F), clear of the
 * CPU exception range. */
#define PIC_IRQ_BASE 32

void pic_init(void);
void pic_send_eoi(uint8_t irq);
void pic_unmask(uint8_t irq);
void pic_mask(uint8_t irq);
