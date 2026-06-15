/* power.c — machine power control.
 *
 * Shutdown uses the ACPI PM1a control register; QEMU/Bochs expose it at a few
 * different I/O ports across versions, so we write S5 (SLP_TYP|SLP_EN) to all
 * the common ones. Reboot pulses the 8042 keyboard-controller reset line.
 * Both fall back to halting if the firmware ignores them. */
#include <stdint.h>

#include "io.h"
#include "kprintf.h"
#include "power.h"

void power_off(void) {
    kprintf("power: off\n");
    outw(0x604, 0x2000);  /* QEMU >= 2.0 ACPI PM1a_CNT (S5 | SLP_EN) */
    outw(0xB004, 0x2000); /* older QEMU / Bochs */
    outw(0x4004, 0x3400); /* QEMU microvm */
    for (;;)
        __asm__ volatile("cli; hlt");
}

void power_reboot(void) {
    kprintf("power: reboot\n");
    /* Drain the 8042 input buffer, then pulse the CPU reset line. */
    uint8_t s;
    int guard = 100000;
    do {
        s = inb(0x64);
        if (s & 0x01)
            (void)inb(0x60);
    } while ((s & 0x02) && guard-- > 0);
    outb(0x64, 0xFE);
    for (;;)
        __asm__ volatile("cli; hlt");
}
