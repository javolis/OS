/* idt.c — Interrupt Descriptor Table: route CPU exceptions to handlers.
 * Without this, any fault escalates to a triple fault and silently reboots. */
#include <stdint.h>

#include "gdt.h"
#include "idt.h"

struct idt_entry {
    uint16_t base_low;
    uint16_t selector; /* code segment to run the handler in */
    uint8_t zero;
    uint8_t flags; /* present | ring | gate type */
    uint16_t base_high;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

/* Present, ring 0, 32-bit interrupt gate. */
#define IDT_GATE_FLAGS 0x8E

static struct idt_entry idt[256];
static struct idt_ptr ip;

/* Exception entry stubs defined in boot/isr.s. */
extern void isr0(void), isr1(void), isr2(void), isr3(void), isr4(void),
    isr5(void), isr6(void), isr7(void), isr8(void), isr9(void), isr10(void),
    isr11(void), isr12(void), isr13(void), isr14(void), isr15(void),
    isr16(void), isr17(void), isr18(void), isr19(void), isr20(void),
    isr21(void), isr22(void), isr23(void), isr24(void), isr25(void),
    isr26(void), isr27(void), isr28(void), isr29(void), isr30(void),
    isr31(void);

/* Hardware IRQ stubs (vectors 32-47), also in boot/isr.s. */
extern void irq0(void), irq1(void), irq2(void), irq3(void), irq4(void),
    irq5(void), irq6(void), irq7(void), irq8(void), irq9(void), irq10(void),
    irq11(void), irq12(void), irq13(void), irq14(void), irq15(void);

static void idt_set_gate(uint8_t num, void (*handler)(void), uint16_t selector,
                         uint8_t flags) {
    uint32_t base = (uint32_t)handler;
    idt[num].base_low = base & 0xFFFF;
    idt[num].base_high = (base >> 16) & 0xFFFF;
    idt[num].selector = selector;
    idt[num].zero = 0;
    idt[num].flags = flags;
}

void idt_init(void) {
    static void (*const exception_stubs[32])(void) = {
        isr0,  isr1,  isr2,  isr3,  isr4,  isr5,  isr6,  isr7,
        isr8,  isr9,  isr10, isr11, isr12, isr13, isr14, isr15,
        isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
        isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31,
    };

    ip.limit = sizeof(idt) - 1;
    ip.base = (uint32_t)idt;

    static void (*const irq_stubs[16])(void) = {
        irq0, irq1, irq2,  irq3,  irq4,  irq5,  irq6,  irq7,
        irq8, irq9, irq10, irq11, irq12, irq13, irq14, irq15,
    };

    /* Unset entries stay zeroed (not-present); faulting on them is itself
     * caught as a GPF once the exception gates below are installed. */
    for (int i = 0; i < 32; i++)
        idt_set_gate(i, exception_stubs[i], GDT_KERNEL_CODE, IDT_GATE_FLAGS);
    for (int i = 0; i < 16; i++)
        idt_set_gate(32 + i, irq_stubs[i], GDT_KERNEL_CODE, IDT_GATE_FLAGS);

    __asm__ volatile("lidt %0" : : "m"(ip));
}
