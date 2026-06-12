/* multiboot.h — Multiboot 1 boot information, as handed over by GRUB.
 * Only the fields the kernel currently consumes are spelled out; see
 * https://www.gnu.org/software/grub/manual/multiboot/ for the full layout. */
#pragma once
#include <stdint.h>

/* Value GRUB leaves in EAX to prove a Multiboot-compliant handoff. */
#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002

/* multiboot_info.flags bits. */
#define MULTIBOOT_INFO_MEMORY 0x01  /* mem_lower / mem_upper valid */
#define MULTIBOOT_INFO_MEM_MAP 0x40 /* mmap_length / mmap_addr valid */

struct multiboot_info {
    uint32_t flags;
    uint32_t mem_lower; /* KiB of conventional memory below 1 MiB */
    uint32_t mem_upper; /* KiB of memory above 1 MiB */
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length; /* total size of the memory map buffer */
    uint32_t mmap_addr;   /* physical address of the first entry */
    /* further fields exist but are not needed yet */
};

#define MULTIBOOT_MEMORY_AVAILABLE 1

struct multiboot_mmap_entry {
    uint32_t size; /* entry size, NOT counting this field itself */
    uint64_t addr;
    uint64_t len;
    uint32_t type;
} __attribute__((packed));
