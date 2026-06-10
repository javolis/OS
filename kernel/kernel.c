/* kernel.c — freestanding kernel entry point. */
#include "vga.h"
#include "serial.h"
#include "gdt.h"
#include "idt.h"
#include "keyboard.h"

#if defined(__linux__)
#error "This kernel must be built with a cross-compiler, not the host toolchain."
#endif

/* Write to both the VGA console and the serial port. */
static void kprint(const char *s) {
    terminal_writestring(s);
    serial_write(s);
}

void kernel_main(void) {
    terminal_initialize();
    serial_init();

    gdt_init();       /* our own flat segment descriptors        */
    idt_init();       /* exception + IRQ vectors, PIC remapped    */
    keyboard_init();  /* IRQ1 -> keyboard_callback                */

    kprint("Hello from the kernel!\n");
    /* Marker the headless smoke test greps for; keep in sync with test/smoke.sh. */
    serial_write("KERNEL_BOOT_OK\n");
    kprint("Interrupts enabled. Type something:\n> ");

    __asm__ volatile("sti"); /* unmask interrupts; keystrokes start flowing */

    for (;;)
        __asm__ volatile("hlt"); /* sleep until the next interrupt */
}
