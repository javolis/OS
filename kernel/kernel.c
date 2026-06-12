/* kernel.c — freestanding kernel entry. */
#include <stdint.h>

#include "gdt.h"
#include "idt.h"
#include "io.h"
#include "keyboard.h"
#include "kheap.h"
#include "kprintf.h"
#include "memlayout.h"
#include "multiboot.h"
#include "paging.h"
#include "pic.h"
#include "pmm.h"
#include "serial.h"
#include "process.h"
#include "sched.h"
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

void kernel_main(uint32_t magic, uint32_t mbi_phys) {
    /* GRUB hands over a physical pointer; reach it through the
     * higher-half window. */
    const struct multiboot_info *mbi = phys_to_virt(mbi_phys);

    term_init();
    serial_init();
    gdt_init();
    pic_init();
    idt_init();

    kprintf("Hello from the higher-half kernel!\n");
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
    kprintf("Paging enabled: %lu MiB mapped in the higher half.\n",
            pmm_total_frames() / 256);

    kheap_init();
    /* Self-test: the second allocation forces the heap to grow beyond its
     * initial page, exercising pmm_alloc_frame + paging_map under PG=1. */
    uint32_t *h1 = kmalloc(64);
    uint32_t *h2 = kmalloc(8192);
    if (!h1 || !h2) {
        kprintf("PANIC: kmalloc failed during heap self-test\n");
        halt_forever();
    }
    h1[0] = 0x12345678u;
    h2[2047] = 0xCAFEBABEu;
    if (h1[0] != 0x12345678u || h2[2047] != 0xCAFEBABEu) {
        kprintf("PANIC: heap self-test read back corrupted data\n");
        halt_forever();
    }
    kfree(h2);
    kfree(h1);
    kprintf("Kernel heap online (self-test passed).\n");

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

    /* Multitasking: spawn two CPU-bound user processes and let the PIT
     * preempt between them while this boot flow acts as the idle task.
     * Their output interleaves; teardown must return every frame. */
    {
        extern char user_prog_a_start[], user_prog_a_end[];
        extern char user_prog_b_start[], user_prog_b_end[];

        /* Warm the heap to working size first: growth pages stay with the
         * heap after kfree, which would otherwise read as a frame leak. */
        void *warm_a = kmalloc(8192);
        void *warm_b = kmalloc(8192);
        kfree(warm_a);
        kfree(warm_b);

        uint32_t frames_before = pmm_free_frames();
        sched_init();
        if (process_spawn(user_prog_a_start, user_prog_a_end) < 0 ||
            process_spawn(user_prog_b_start, user_prog_b_end) < 0) {
            kprintf("PANIC: could not spawn user processes\n");
            halt_forever();
        }
        sched_start();
        while (sched_user_tasks_alive())
            __asm__ volatile("hlt"); /* idle until everything exits */
        sched_stop();
        sched_reap();
        kprintf("[sched] %lu context switches; all user tasks finished\n",
                sched_switch_count());

        if (pmm_free_frames() == frames_before)
            kprintf("Address spaces reclaimed cleanly.\n");
        else
            kprintf("WARNING: process teardown leaked %lu frames\n",
                    frames_before - pmm_free_frames());
    }

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
