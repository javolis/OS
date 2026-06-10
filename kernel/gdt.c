/* gdt.c — a flat (unsegmented) GDT: every segment spans the full 4 GiB. */
#include "gdt.h"
#include <stdint.h>

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_mid;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct gdt_entry gdt[3];
static struct gdt_ptr gp;

/* Defined in interrupt.s — loads gp and reloads the segment registers. */
extern void gdt_flush(uint32_t gdt_ptr_addr);

static void gdt_set_gate(int n, uint32_t base, uint32_t limit,
                         uint8_t access, uint8_t gran) {
    gdt[n].base_low = (uint16_t)(base & 0xFFFF);
    gdt[n].base_mid = (uint8_t)((base >> 16) & 0xFF);
    gdt[n].base_high = (uint8_t)((base >> 24) & 0xFF);

    gdt[n].limit_low = (uint16_t)(limit & 0xFFFF);
    gdt[n].granularity = (uint8_t)((limit >> 16) & 0x0F);
    gdt[n].granularity |= gran & 0xF0;
    gdt[n].access = access;
}

void gdt_init(void) {
    gp.limit = (uint16_t)(sizeof(gdt) - 1);
    gp.base = (uint32_t)&gdt;

    gdt_set_gate(0, 0, 0, 0, 0);                /* null descriptor          */
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF); /* ring-0 code, 4 KiB, 32b  */
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF); /* ring-0 data, 4 KiB, 32b  */

    gdt_flush((uint32_t)&gp);
}
