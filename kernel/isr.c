/* isr.c — C-level CPU exception handling.
 * Breakpoints (int3) are reported and resumed; everything else panics with a
 * register dump so faults are debuggable instead of silent triple-faults. */
#include <stdint.h>

#include "idt.h"
#include "io.h"
#include "kprintf.h"
#include "sched.h"
#include "syscall.h"

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
    if (regs->int_no == SYSCALL_VECTOR) {
        syscall_handle(regs);
        return;
    }

    /* Breakpoints are non-fatal: report and resume after the int3. */
    if (regs->int_no == 3) {
        kprintf("breakpoint at eip=%08lx\n", regs->eip);
        return;
    }

    uint32_t cr2 = 0;
    if (regs->int_no == 14) /* page fault: CR2 holds the faulting address */
        __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));

    /* A fault raised in ring 3 is the task's problem, not the kernel's:
     * report it, kill the task, and keep the system running. */
    if ((regs->cs & 3) == 3) {
        kprintf("[pid %lu] killed: %s at eip=%08lx", sched_current_pid(),
                exception_names[regs->int_no & 31], regs->eip);
        if (regs->int_no == 14)
            kprintf(" (%s %s at %08lx)",
                    (regs->err_code & 2) ? "write" : "read",
                    (regs->err_code & 1) ? "protection violation"
                                         : "to unmapped address",
                    cr2);
        kprintf("\n");
        task_exit();
    }

    kprintf("\nKERNEL PANIC: exception %lu (%s), error code %08lx\n",
            regs->int_no, exception_names[regs->int_no & 31], regs->err_code);
    if (regs->int_no == 14)
        kprintf("  page fault at %08lx (%s, %s, %s mode)\n", cr2,
                (regs->err_code & 1) ? "protection violation" : "not present",
                (regs->err_code & 2) ? "write" : "read",
                (regs->err_code & 4) ? "user" : "kernel");
    kprintf("  eip=%08lx cs=%04lx eflags=%08lx\n", regs->eip, regs->cs,
            regs->eflags);
    kprintf("  eax=%08lx ebx=%08lx ecx=%08lx edx=%08lx\n", regs->eax,
            regs->ebx, regs->ecx, regs->edx);
    kprintf("  esi=%08lx edi=%08lx ebp=%08lx ds=%04lx\n", regs->esi,
            regs->edi, regs->ebp, regs->ds);

    /* Fail fast in CI; on real hardware the port write is ignored and we
     * fall through to a halt loop. */
    qemu_exit(1);

    for (;;)
        __asm__ volatile("cli; hlt");
}
