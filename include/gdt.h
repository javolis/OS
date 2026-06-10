/* gdt.h — Global Descriptor Table setup. */
#pragma once

/* Install a flat GDT (null + ring-0 code + ring-0 data) and reload segments. */
void gdt_init(void);
