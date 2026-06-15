/* kernel.c — freestanding kernel entry. */
#include <stdint.h>

#include "ac97.h"
#include "arp.h"
#include "dhcp.h"
#include "fb.h"
#include "gdt.h"
#include "icmp.h"
#include "idt.h"
#include "initrd.h"
#include "io.h"
#include "ip.h"
#include "keyboard.h"
#include "kheap.h"
#include "kprintf.h"
#include "memlayout.h"
#include "mouse.h"
#include "multiboot.h"
#include "net.h"
#include "paging.h"
#include "pci.h"
#include "pic.h"
#include "pmm.h"
#include "rtl8139.h"
#include "serial.h"
#include "process.h"
#include "sched.h"
#include "shell.h"
#include "speaker.h"
#include "tcp.h"
#include "term.h"
#include "timer.h"
#include "udp.h"

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

    initrd_init(mbi);
    if (!initrd_present()) {
        kprintf("PANIC: initrd module missing (check grub.cfg)\n");
        halt_forever();
    }

    /* Enumerate the PCI bus so device drivers (e.g. the NIC) can find their
     * hardware, BARs and IRQ line, then bring up the network card and the
     * Ethernet layer. */
    pci_init();
    if (rtl8139_init() == 0) {
        eth_init();
        arp_init();
        ip_init();
        icmp_init();
        udp_init();
        dhcp_init();
        tcp_init();
        ip_selftest();
        arp_request(net_gateway()); /* pre-resolve the gateway MAC */
    }

    /* AC'97 audio controller (PCM output via bus-master DMA). */
    ac97_init();

    /* Framebuffer: if the bootloader gave us a linear 32bpp surface, switch
     * the console to it so the shell renders graphically (and on UEFI VMs
     * that lack VGA text). Early boot lines above this point went to VGA
     * text + serial; from here output is drawn with the 8x8 font. CI can't
     * see pixels, so log geometry + a checksum over the rendered banner. */
    /* Dump exactly what the bootloader handed over, so a 'none' below is
     * diagnosable from the serial log alone (CI can't inspect the VM). */
    kprintf("mb: flags=%08lx fbaddr=%08lx %lux%lu pitch=%lu bpp=%u type=%u\n",
            mbi->flags, (uint32_t)mbi->framebuffer_addr, mbi->framebuffer_width,
            mbi->framebuffer_height, mbi->framebuffer_pitch,
            mbi->framebuffer_bpp, mbi->framebuffer_type);
    if (fb_init(mbi)) {
        term_use_framebuffer();
        term_set_color(TERM_CYAN);
        kprintf("framebuffer console: %lux%lu %lubpp\n", fb_width(),
                fb_height(), fb_bpp());
        term_reset_color();
        kprintf("fbcon checksum %08lx\n", fb_checksum(0, 0, 240, 8));

        /* Color self-test (CI can't see pixels): render the same glyph in
         * two colors at a scratch cell and confirm the framebuffer differs,
         * proving the console honors glyph color. The cell is then cleared. */
        uint32_t sx = 0, sy = fb_height() - 8;
        fb_fillrect(sx, sy, 8, 8, 0);
        fb_draw_glyph(sx, sy, 'M', TERM_RED, 0);
        uint32_t red = fb_checksum(sx, sy, 8, 8);
        fb_fillrect(sx, sy, 8, 8, 0);
        fb_draw_glyph(sx, sy, 'M', TERM_CYAN, 0);
        uint32_t cyan = fb_checksum(sx, sy, 8, 8);
        fb_fillrect(sx, sy, 8, 8, 0);
        kprintf("fbcolor red=%08lx cyan=%08lx\n", red, cyan);
    } else {
        kprintf("framebuffer: none (using VGA text console)\n");
    }

    /* Prove the IDT actually works: int3 lands in isr_handler (which prints
     * and resumes). If interrupt dispatch is broken we fault here and never
     * reach the boot marker below, failing the smoke test. */
    __asm__ volatile("int3");

    timer_init(100);
    keyboard_init();
    mouse_init();
    speaker_init(); /* silence the PC speaker (PIT channel 2) */
    __asm__ volatile("sti");

    /* Prove hardware IRQs fire: wait for the PIT to tick 10 times (100 ms).
     * If delivery is broken we hlt here forever and the smoke test times
     * out without seeing the boot marker. */
    while (timer_ticks() < 10)
        __asm__ volatile("hlt");
    kprintf("PIT timer running at 100 Hz (%lu ticks).\n", timer_ticks());

    /* Multitasking: load two CPU-bound user ELF executables from the
     * initrd and let the PIT preempt between them while this boot flow
     * acts as the idle task. Their output interleaves; teardown must
     * return every frame. */
    {
        uint32_t elf_a_size, elf_b_size;
        const char *elf_a = initrd_find("hello_a.elf", &elf_a_size);
        const char *elf_b = initrd_find("hello_b.elf", &elf_b_size);
        if (!elf_a || !elf_b) {
            kprintf("PANIC: user programs missing from the initrd\n");
            halt_forever();
        }

        /* Warm the heap to working size first: growth pages stay with the
         * heap after kfree, which would otherwise read as a frame leak. */
        void *warm_a = kmalloc(8192);
        void *warm_b = kmalloc(8192);
        kfree(warm_a);
        kfree(warm_b);

        uint32_t frames_before = pmm_free_frames();
        sched_init();
        if (process_spawn(elf_a, elf_a + elf_a_size, "hello_a.elf", 0, 0,
                          0) < 0 ||
            process_spawn(elf_b, elf_b + elf_b_size, "hello_b.elf", 0, 0,
                          0) < 0) {
            kprintf("PANIC: could not spawn user processes\n");
            halt_forever();
        }
        sched_start();
        while (sched_user_tasks_alive())
            __asm__ volatile("hlt"); /* idle until everything exits */
        /* The scheduler stays on from here: the shell (this task) is the
         * idle task, and its run command spawns programs live. */
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
