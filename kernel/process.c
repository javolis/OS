/* process.c — minimal processes: each runs one user image to completion in
 * a private address space (own page directory, kernel half shared). Real
 * multitasking needs a scheduler; this establishes creation, isolation,
 * and teardown first. */
#include <stdint.h>

#include "kprintf.h"
#include "memlayout.h"
#include "paging.h"
#include "pmm.h"
#include "process.h"
#include "usermode.h"

#define FRAME_SIZE 4096u

/* Fixed layout for now; ELF loading will generalize this. */
#define USER_CODE_VADDR 0x08048000u
#define USER_STACK_VADDR 0x08070000u

static uint32_t next_pid = 1;

int process_run(const char *image_start, const char *image_end) {
    uint32_t size = (uint32_t)(image_end - image_start);
    if (size > FRAME_SIZE)
        return -1;

    uint32_t code_frame = pmm_alloc_frame();
    uint32_t stack_frame = pmm_alloc_frame();
    if (!code_frame || !stack_frame) {
        if (code_frame)
            pmm_free_frame(code_frame);
        if (stack_frame)
            pmm_free_frame(stack_frame);
        return -1;
    }

    uint8_t *dst = phys_to_virt(code_frame);
    for (uint32_t i = 0; i < size; i++)
        dst[i] = image_start[i];

    uint32_t dir = paging_new_address_space();
    paging_map_user_in(dir, USER_CODE_VADDR, code_frame);
    paging_map_user_in(dir, USER_STACK_VADDR, stack_frame);

    uint32_t pid = next_pid++;
    kprintf("[pid %lu] entering ring 3\n", pid);

    paging_switch(dir);
    enter_user_mode(USER_CODE_VADDR, USER_STACK_VADDR + FRAME_SIZE - 16);
    /* Back via the exit syscall; its interrupt gate left IF cleared. */
    __asm__ volatile("sti");

    paging_switch(paging_kernel_directory());
    paging_destroy_address_space(dir); /* frees code, stack, tables, dir */
    kprintf("[pid %lu] exited; address space reclaimed\n", pid);
    return 0;
}
