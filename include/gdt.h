/* gdt.h — Global Descriptor Table. */
#pragma once
#include <stdint.h>

/* Segment selectors, fixed by the entry order in kernel/gdt.c.
 * User selectors carry RPL 3 in their low bits when loaded. */
#define GDT_KERNEL_CODE 0x08
#define GDT_KERNEL_DATA 0x10
#define GDT_USER_CODE 0x18 /* 0x1B with RPL 3 */
#define GDT_USER_DATA 0x20 /* 0x23 with RPL 3 */
#define GDT_TSS 0x28

void gdt_init(void);

/* Stack the CPU switches to when an interrupt arrives from ring 3. */
void gdt_set_kernel_stack(uint32_t esp0);
