/* initrd.h — read-only initial ramdisk (USTAR tar, GRUB boot module). */
#pragma once
#include <stdint.h>

struct multiboot_info;

/* Locate the first boot module and adopt it as the initrd. */
void initrd_init(const struct multiboot_info *mbi);
int initrd_present(void);

/* Returns a pointer to the file's data and sets *size_out, or NULL. */
const void *initrd_find(const char *name, uint32_t *size_out);

/* Print a directory listing via kprintf (used by the shell's ls). */
void initrd_list(void);
