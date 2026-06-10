/* isr.h — interrupt service routine plumbing. */
#pragma once
#include <stdint.h>

/* Register state pushed by the asm stubs; layout must match interrupt.s. */
typedef struct {
    uint32_t ds;                                     /* saved data segment      */
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; /* pusha                   */
    uint32_t int_no, err_code;                       /* interrupt # + err code  */
    uint32_t eip, cs, eflags, useresp, ss;           /* pushed by the CPU       */
} registers_t;

typedef void (*isr_t)(registers_t *);

/* Register a C handler for a given interrupt/IRQ vector. */
void register_interrupt_handler(uint8_t n, isr_t handler);

/* Hardware IRQs are remapped to vectors 32..47. */
#define IRQ0 32
#define IRQ1 33
