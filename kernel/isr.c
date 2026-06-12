/* isr.c — C-level CPU exception handling.
 * Breakpoints (int3) are reported and resumed; everything else panics with a
 * register dump so faults are debuggable instead of silent triple-faults. */
#include <stdint.h>

#include "idt.h"
#include "io.h"
#include "kprintf.h"

static const char *const exception_names[32] = {
    "Divide Error",
    "Debug",
    "Non-Maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "BOUND Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack-Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "x87 Floating-Point Exception",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point Exception",
    "Virtualization Exception",
    "Control Protection Exception",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Hypervisor Injection Exception",
    "VMM Communication Exception",
    "Security Exception",
    "Reserved",
};

void isr_handler(struct registers *regs);

void isr_handler(struct registers *regs) {
    /* Breakpoints are non-fatal: report and resume after the int3. */
    if (regs->int_no == 3) {
        kprintf("breakpoint at eip=%08x\n", regs->eip);
        return;
    }

    kprintf("\nKERNEL PANIC: exception %u (%s), error code %08x\n",
            regs->int_no, exception_names[regs->int_no & 31], regs->err_code);
    kprintf("  eip=%08x cs=%04x eflags=%08x\n", regs->eip, regs->cs,
            regs->eflags);
    kprintf("  eax=%08x ebx=%08x ecx=%08x edx=%08x\n", regs->eax, regs->ebx,
            regs->ecx, regs->edx);
    kprintf("  esi=%08x edi=%08x ebp=%08x ds=%04x\n", regs->esi, regs->edi,
            regs->ebp, regs->ds);

    /* Fail fast in CI; on real hardware the port write is ignored and we
     * fall through to a halt loop. */
    qemu_exit(1);

    for (;;)
        __asm__ volatile("cli; hlt");
}
