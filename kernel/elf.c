/* elf.c — minimal ELF32 executable loader.
 * Static ET_EXEC binaries only (no relocations, no dynamic linking) —
 * exactly what the user/ build produces. */
#include <stdint.h>

#include "elf.h"
#include "memlayout.h"
#include "paging.h"
#include "pmm.h"

#define PAGE_SIZE 4096u

/* Sanity bounds for user segments. */
#define USER_VADDR_MAX 0x40000000u /* 1 GiB, far below the kernel base */
#define SEGMENT_MAX (16u * 1024 * 1024)

static int load_segment(uint32_t dir_phys, const uint8_t *image,
                        const struct elf32_phdr *ph) {
    uint32_t seg_start = ph->p_vaddr;
    uint32_t file_end = seg_start + ph->p_filesz;
    uint32_t mem_end = seg_start + ph->p_memsz;

    int writable = (ph->p_flags & PF_W) != 0;

    for (uint32_t page = seg_start & ~(PAGE_SIZE - 1); page < mem_end;
         page += PAGE_SIZE) {
        /* Segments may share a page (e.g. rodata then data); reuse the
         * frame if one is already mapped there. */
        uint32_t frame = paging_get_phys(dir_phys, page);
        if (!frame) {
            frame = pmm_alloc_frame();
            if (!frame)
                return -1;
            uint8_t *f = phys_to_virt(frame);
            for (uint32_t i = 0; i < PAGE_SIZE; i++)
                f[i] = 0; /* covers BSS and any gap bytes */
            paging_map_user_in(dir_phys, page, frame, writable);
        } else if (writable) {
            /* Shared page where this segment needs write access: upgrade
             * (never downgrade — an earlier writable segment wins). */
            paging_map_user_in(dir_phys, page, frame, 1);
        }

        /* Copy the part of [page, page+4K) that overlaps the file data. */
        uint8_t *dst = phys_to_virt(frame);
        uint32_t lo = page > seg_start ? page : seg_start;
        uint32_t hi =
            page + PAGE_SIZE < file_end ? page + PAGE_SIZE : file_end;
        for (uint32_t a = lo; a < hi; a++)
            dst[a - page] = image[ph->p_offset + (a - seg_start)];
    }
    return 0;
}

int elf_load(uint32_t dir_phys, const uint8_t *image, uint32_t size,
             uint32_t *entry_out) {
    if (size < sizeof(struct elf32_ehdr))
        return -1;
    const struct elf32_ehdr *eh = (const struct elf32_ehdr *)image;

    if (eh->e_ident[0] != 0x7F || eh->e_ident[1] != 'E' ||
        eh->e_ident[2] != 'L' || eh->e_ident[3] != 'F')
        return -1;
    if (eh->e_ident[4] != ELF_CLASS_32 || eh->e_ident[5] != ELF_DATA_LSB)
        return -1;
    if (eh->e_type != ET_EXEC || eh->e_machine != EM_386)
        return -1;
    if (eh->e_phentsize != sizeof(struct elf32_phdr))
        return -1;
    if (eh->e_phoff > size ||
        (uint32_t)eh->e_phnum * sizeof(struct elf32_phdr) > size - eh->e_phoff)
        return -1;

    const struct elf32_phdr *phdrs =
        (const struct elf32_phdr *)(image + eh->e_phoff);
    for (uint16_t i = 0; i < eh->e_phnum; i++) {
        const struct elf32_phdr *ph = &phdrs[i];
        if (ph->p_type != PT_LOAD)
            continue;
        if (ph->p_filesz > ph->p_memsz)
            return -1;
        if (ph->p_vaddr >= USER_VADDR_MAX || ph->p_memsz > SEGMENT_MAX)
            return -1;
        if (ph->p_offset > size || ph->p_filesz > size - ph->p_offset)
            return -1;
        if (load_segment(dir_phys, image, ph) != 0)
            return -1;
    }

    *entry_out = eh->e_entry;
    return 0;
}
