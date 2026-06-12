/* io.h — x86 port I/O primitives. */
#pragma once
#include <stdint.h>

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* Brief delay for slow devices (e.g. between 8259 PIC init writes): a write
 * to port 0x80, unused except by POST diagnostics. */
static inline void io_wait(void) {
    outb(0x80, 0);
}

/* Disable interrupts, returning the previous EFLAGS for irq_restore. */
static inline uint32_t irq_save(void) {
    uint32_t flags;
    __asm__ volatile("pushf; pop %0; cli" : "=r"(flags) : : "memory");
    return flags;
}

static inline void irq_restore(uint32_t flags) {
    __asm__ volatile("push %0; popf" : : "r"(flags) : "memory", "cc");
}

/* Ask QEMU's isa-debug-exit device to power off. Harmless on real hardware
 * (the port write is simply ignored when the device is absent). */
static inline void qemu_exit(uint8_t code) {
    outl(0xF4, code);
}
