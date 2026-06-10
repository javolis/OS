/* idt.h — Interrupt Descriptor Table setup. */
#pragma once

/* Install the IDT, remap the PIC, and unmask the keyboard IRQ. */
void idt_init(void);
