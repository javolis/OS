/* irq.h — hardware interrupt (IRQ 0-15) dispatch. */
#pragma once
#include <stdint.h>

#include "idt.h"

typedef void (*irq_handler_t)(struct registers *regs);

void irq_register(uint8_t irq, irq_handler_t handler);
