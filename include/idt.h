/* idt.h — Interrupt Descriptor Table and exception handling. */
#pragma once
#include <stdint.h>

/* CPU state captured by the ISR stubs in boot/isr.s, in stack layout order.
 * Note: `esp` is the stub's snapshot from pusha (it points into the exception
 * frame), not the interrupted context's stack pointer — there is no privilege
 * change yet, so the CPU doesn't push ss:esp. */
struct registers {
    uint32_t ds;                                     /* pushed by the stub */
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; /* pusha */
    uint32_t int_no, err_code;                       /* stub + CPU (or dummy 0) */
    uint32_t eip, cs, eflags;                        /* pushed by the CPU */
};

void idt_init(void);
