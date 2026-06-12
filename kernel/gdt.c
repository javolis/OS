/* gdt.c — Global Descriptor Table: flat 4 GiB code/data segments for
 * rings 0 and 3, plus a TSS.
 *
 * The Multiboot spec leaves the GDT in an undefined state after GRUB hands
 * off, so the kernel must install its own before relying on segmentation.
 * The TSS exists only for its ss0:esp0 pair — the stack the CPU switches to
 * when an interrupt or syscall arrives from ring 3. */
#include <stdint.h>

#include "gdt.h"

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t granularity; /* limit bits 16-19 + flags (4 KiB gran, 32-bit) */
    uint8_t base_high;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

/* 32-bit Task State Segment; only ss0/esp0 (and iomap_base) matter here. */
struct tss_entry {
    uint32_t prev_tss;
    uint32_t esp0;
    uint32_t ss0;
    uint32_t esp1;
    uint32_t ss1;
    uint32_t esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t eip, eflags;
    uint32_t eax, ecx, edx, ebx, esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap_base;
} __attribute__((packed));

static struct gdt_entry gdt[6];
static struct gdt_ptr gp;
static struct tss_entry tss;

/* Dedicated stack for ring-3 entries into the kernel. Separate from the
 * boot stack so a trap from user mode can't clobber the kernel context
 * that entered user mode in the first place. */
static uint8_t ring0_stack[8192] __attribute__((aligned(16)));

/* boot/gdt.s — loads the GDTR and reloads all segment registers. */
extern void gdt_flush(const struct gdt_ptr *gp);

static void gdt_set_gate(int num, uint32_t base, uint32_t limit,
                         uint8_t access, uint8_t gran) {
    gdt[num].base_low = base & 0xFFFF;
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high = (base >> 24) & 0xFF;
    gdt[num].limit_low = limit & 0xFFFF;
    gdt[num].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[num].access = access;
}

void gdt_set_kernel_stack(uint32_t esp0) {
    tss.esp0 = esp0;
}

void gdt_init(void) {
    gp.limit = sizeof(gdt) - 1;
    gp.base = (uint32_t)gdt;

    gdt_set_gate(0, 0, 0, 0, 0);             /* mandatory null descriptor */
    gdt_set_gate(1, 0, 0xFFFFF, 0x9A, 0xCF); /* 0x08: ring-0 code */
    gdt_set_gate(2, 0, 0xFFFFF, 0x92, 0xCF); /* 0x10: ring-0 data */
    gdt_set_gate(3, 0, 0xFFFFF, 0xFA, 0xCF); /* 0x18: ring-3 code */
    gdt_set_gate(4, 0, 0xFFFFF, 0xF2, 0xCF); /* 0x20: ring-3 data */

    uint8_t *t = (uint8_t *)&tss;
    for (uint32_t i = 0; i < sizeof(tss); i++)
        t[i] = 0;
    tss.ss0 = GDT_KERNEL_DATA;
    tss.esp0 = (uint32_t)(ring0_stack + sizeof(ring0_stack));
    tss.iomap_base = sizeof(tss); /* no I/O permission bitmap */
    /* 0x28: available 32-bit TSS (byte granularity) */
    gdt_set_gate(5, (uint32_t)&tss, sizeof(tss) - 1, 0x89, 0x00);

    gdt_flush(&gp);
    __asm__ volatile("ltr %0" : : "r"((uint16_t)GDT_TSS));
}
