/* gdt.c — Global Descriptor Table: flat 4 GiB ring-0 code/data segments.
 *
 * The Multiboot spec leaves the GDT in an undefined state after GRUB hands
 * off, so the kernel must install its own before relying on segmentation. */
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

static struct gdt_entry gdt[3];
static struct gdt_ptr gp;

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

void gdt_init(void) {
    gp.limit = sizeof(gdt) - 1;
    gp.base = (uint32_t)gdt;

    gdt_set_gate(0, 0, 0, 0, 0);             /* mandatory null descriptor */
    gdt_set_gate(1, 0, 0xFFFFF, 0x9A, 0xCF); /* 0x08: ring-0 code, flat 4 GiB */
    gdt_set_gate(2, 0, 0xFFFFF, 0x92, 0xCF); /* 0x10: ring-0 data, flat 4 GiB */

    gdt_flush(&gp);
}
