/* elf.h — minimal ELF32 executable loading. */
#pragma once
#include <stdint.h>

struct elf32_ehdr {
    uint8_t e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
};

struct elf32_phdr {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
};

#define ELF_CLASS_32 1
#define ELF_DATA_LSB 1
#define ET_EXEC 2
#define EM_386 3
#define PT_LOAD 1

/* Load an ELF32 executable image into the given address space: allocates
 * frames, maps them user-accessible, copies PT_LOAD file contents, and
 * zeroes BSS. Returns 0 and sets *entry_out on success, -1 on a malformed
 * or unsupported image (caller destroys the address space). */
int elf_load(uint32_t dir_phys, const uint8_t *image, uint32_t size,
             uint32_t *entry_out);
