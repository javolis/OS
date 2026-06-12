/* kernel.c — freestanding kernel entry. */
#include "gdt.h"
#include "idt.h"
#include "io.h"
#include "kprintf.h"
#include "serial.h"
#include "term.h"

#if defined(__linux__)
#error "This kernel must be built with a cross-compiler, not the host toolchain."
#endif

void kernel_main(void) {
    term_init();
    serial_init();
    gdt_init();
    idt_init();

    kprintf("Hello from the kernel!\n");
    kprintf("GDT loaded; IDT loaded, exceptions handled.\n");

    /* Prove the IDT actually works: int3 lands in isr_handler (which prints
     * and resumes). If interrupt dispatch is broken we fault here and never
     * reach the boot marker below, failing the smoke test. */
    __asm__ volatile("int3");

    /* Marker the headless smoke test greps for; keep in sync with test/smoke.sh. */
    serial_write("KERNEL_BOOT_OK\n");
    kprintf("Boot succeeded. Halting.\n");

    qemu_exit(0);
}
