/* process.c — user processes: load an ELF executable into a private
 * address space (own page directory, kernel half shared) and hand it to
 * the scheduler. */
#include <stdint.h>

#include "elf.h"
#include "kprintf.h"
#include "memlayout.h"
#include "paging.h"
#include "pmm.h"
#include "process.h"
#include "sched.h"

#define FRAME_SIZE 4096u

/* One stack page, well clear of where the executables link (0x08048000+). */
#define USER_STACK_VADDR 0x0BFFF000u

int process_spawn(const char *image_start, const char *image_end) {
    uint32_t size = (uint32_t)(image_end - image_start);

    uint32_t dir = paging_new_address_space();
    uint32_t entry;
    if (elf_load(dir, (const uint8_t *)image_start, size, &entry) != 0) {
        paging_destroy_address_space(dir);
        return -1;
    }

    uint32_t stack_frame = pmm_alloc_frame();
    if (!stack_frame) {
        paging_destroy_address_space(dir);
        return -1;
    }
    paging_map_user_in(dir, USER_STACK_VADDR, stack_frame, 1);

    int pid = sched_spawn_user(dir, entry,
                               USER_STACK_VADDR + FRAME_SIZE - 16);
    if (pid < 0) {
        paging_destroy_address_space(dir); /* frees segments + stack too */
        return -1;
    }
    kprintf("[pid %d] spawned (ELF entry %08lx)\n", pid, entry);
    return pid;
}
