/* kernel.c — freestanding kernel entry. */
#include "gdt.h"
#include "idt.h"
#include "io.h"
#include "keyboard.h"
#include "kprintf.h"
#include "pic.h"
#include "serial.h"
#include "term.h"
#include "timer.h"

#if defined(__linux__)
#error "This kernel must be built with a cross-compiler, not the host toolchain."
#endif

void kernel_main(void) {
    term_init();
    serial_init();
    gdt_init();
    pic_init();
    idt_init();

    kprintf("Hello from the kernel!\n");
    kprintf("GDT loaded; IDT loaded, exceptions handled.\n");

    /* Prove the IDT actually works: int3 lands in isr_handler (which prints
     * and resumes). If interrupt dispatch is broken we fault here and never
     * reach the boot marker below, failing the smoke test. */
    __asm__ volatile("int3");

    timer_init(100);
    keyboard_init();
    __asm__ volatile("sti");

    /* Prove hardware IRQs fire: wait for the PIT to tick 10 times (100 ms).
     * If delivery is broken we hlt here forever and the smoke test times
     * out without seeing the boot marker. */
    while (timer_ticks() < 10)
        __asm__ volatile("hlt");
    kprintf("PIT timer running at 100 Hz (%lu ticks).\n", timer_ticks());

    /* Marker the headless smoke test greps for; keep in sync with test/smoke.sh. */
    serial_write("KERNEL_BOOT_OK\n");
    kprintf("Boot succeeded.\n");

    /* Under the smoke test this exits QEMU. Without the isa-debug-exit
     * device (make run, real hardware) it's a no-op and we idle below,
     * echoing keystrokes via the keyboard IRQ. */
    qemu_exit(0);

    kprintf("Type away; keys echo:\n");
    for (;;)
        __asm__ volatile("hlt");
}
