/* multiboot.h — Multiboot 1 boot information, as handed over by GRUB.
 * Only the fields the kernel currently consumes are spelled out; see
 * https://www.gnu.org/software/grub/manual/multiboot/ for the full layout. */
#pragma once
#include <stdint.h>

/* Value GRUB leaves in EAX to prove a Multiboot-compliant handoff. */
#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002

/* multiboot_info.flags bits. */
#define MULTIBOOT_INFO_MEMORY 0x01    /* mem_lower / mem_upper valid */
#define MULTIBOOT_INFO_MODS 0x08      /* mods_count / mods_addr valid */
#define MULTIBOOT_INFO_MEM_MAP 0x40   /* mmap_length / mmap_addr valid */
#define MULTIBOOT_INFO_FRAMEBUFFER 0x1000 /* framebuffer_* valid (bit 12) */

#define MULTIBOOT_FRAMEBUFFER_TYPE_RGB 1

/* Full standard layout up to the framebuffer fields, so their offsets are
 * correct (GRUB fills the ones whose flag bit is set). */
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
    uint32_t drives_length;
    uint32_t drives_addr;
    uint32_t config_table;
    uint32_t boot_loader_name;
    uint32_t apm_table;
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;
    uint64_t framebuffer_addr; /* physical address of the linear fb */
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t framebuffer_bpp;
    uint8_t framebuffer_type; /* 1 = direct RGB */
    uint8_t color_info[6];
} __attribute__((packed));

/* One entry of the boot-module list (mods_addr points at an array). */
struct multiboot_mod {
    uint32_t mod_start; /* physical */
    uint32_t mod_end;
    uint32_t cmdline;
    uint32_t pad;
};

#define MULTIBOOT_MEMORY_AVAILABLE 1

struct multiboot_mmap_entry {
    uint32_t size; /* entry size, NOT counting this field itself */
    uint64_t addr;
    uint64_t len;
    uint32_t type;
} __attribute__((packed));
