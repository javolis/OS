/* kernel.c — freestanding kernel entry. */
#include <stdint.h>

#include "gdt.h"
#include "idt.h"
#include "io.h"
#include "keyboard.h"
#include "kprintf.h"
#include "multiboot.h"
#include "paging.h"
#include "pic.h"
#include "pmm.h"
#include "serial.h"
#include "shell.h"
#include "term.h"
#include "timer.h"

#if defined(__linux__)
#error "This kernel must be built with a cross-compiler, not the host toolchain."
#endif

static void __attribute__((noreturn)) halt_forever(void) {
    for (;;)
        __asm__ volatile("cli; hlt");
}

void kernel_main(uint32_t magic, const struct multiboot_info *mbi) {
    term_init();
    serial_init();
    gdt_init();
    pic_init();
    idt_init();

    kprintf("Hello from the kernel!\n");
    kprintf("GDT loaded; IDT loaded, exceptions handled.\n");

    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        kprintf("PANIC: bad multiboot magic %08lx\n", magic);
        halt_forever();
    }
    if (!(mbi->flags & MULTIBOOT_INFO_MEM_MAP)) {
        kprintf("PANIC: bootloader provided no memory map\n");
        halt_forever();
    }
    pmm_init(mbi);
    kprintf("Memory: %lu MiB usable (%lu of %lu frames).\n",
            pmm_free_frames() / 256, pmm_free_frames(), pmm_total_frames());

    paging_init();
    kprintf("Paging enabled: %lu MiB identity-mapped.\n",
            pmm_total_frames() / 256);

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

    /* Exits QEMU only when the isa-debug-exit device is present. Neither
     * the smoke test nor `make run` adds it (the smoke test types shell
     * commands via the QEMU monitor), so normally this is a no-op and we
     * drop into the shell below. */
    qemu_exit(0);

    shell_run();
}
