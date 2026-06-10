/* isr.c — C-side dispatch for CPU exceptions (0-31) and hardware IRQs (32-47). */
#include "isr.h"
#include "pic.h"
#include "vga.h"
#include "serial.h"
#include <stdint.h>

static isr_t interrupt_handlers[256];

static const char *const exception_messages[32] = {
    "Division by zero",
    "Debug",
    "Non-maskable interrupt",
    "Breakpoint",
    "Overflow",
    "Bound range exceeded",
    "Invalid opcode",
    "Device not available",
    "Double fault",
    "Coprocessor segment overrun",
    "Invalid TSS",
    "Segment not present",
    "Stack-segment fault",
    "General protection fault",
    "Page fault",
    "Reserved",
    "x87 floating-point exception",
    "Alignment check",
    "Machine check",
    "SIMD floating-point exception",
    "Virtualization exception",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Security exception",
};

void register_interrupt_handler(uint8_t n, isr_t handler) {
    interrupt_handlers[n] = handler;
}

static void emit(const char *s) {
    terminal_writestring(s);
    serial_write(s);
}

/* Called from isr_common_stub for vectors 0-31. */
void isr_handler(registers_t *regs) {
    if (interrupt_handlers[regs->int_no]) {
        interrupt_handlers[regs->int_no](regs);
        return;
    }

    emit("\n*** CPU EXCEPTION: ");
    emit(regs->int_no < 32 ? exception_messages[regs->int_no] : "Unknown");
    emit(" -- system halted ***\n");

    for (;;)
        __asm__ volatile("cli; hlt");
}

/* Called from irq_common_stub for vectors 32-47. */
void irq_handler(registers_t *regs) {
    if (interrupt_handlers[regs->int_no])
        interrupt_handlers[regs->int_no](regs);

    /* Acknowledge the PIC so it will deliver further interrupts. */
    pic_eoi((uint8_t)(regs->int_no - 32));
}
