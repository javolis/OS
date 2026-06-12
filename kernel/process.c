/* process.c — user processes: stage an image into a private address space
 * (own page directory, kernel half shared) and hand it to the scheduler. */
#include <stdint.h>

#include "kprintf.h"
#include "memlayout.h"
#include "paging.h"
#include "pmm.h"
#include "process.h"
#include "sched.h"

#define FRAME_SIZE 4096u

/* Fixed layout for now; ELF loading will generalize this. */
#define USER_CODE_VADDR 0x08048000u
#define USER_STACK_VADDR 0x08070000u

int process_spawn(const char *image_start, const char *image_end) {
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

    int pid = sched_spawn_user(dir, USER_CODE_VADDR,
                               USER_STACK_VADDR + FRAME_SIZE - 16);
    if (pid < 0) {
        paging_destroy_address_space(dir); /* also frees code + stack */
        return -1;
    }
    kprintf("[pid %d] spawned\n", pid);
    return pid;
}
