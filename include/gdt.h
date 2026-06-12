/* gdt.h — Global Descriptor Table. */
#pragma once

/* Segment selectors, fixed by the entry order in kernel/gdt.c. */
#define GDT_KERNEL_CODE 0x08
#define GDT_KERNEL_DATA 0x10

void gdt_init(void);
