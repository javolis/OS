/* process.c — user processes: load an ELF executable into a private
 * address space (own page directory, kernel half shared), build its
 * argc/argv on the user stack, and hand it to the scheduler. */
#include <stdint.h>

#include "elf.h"
#include "kprintf.h"
#include "memlayout.h"
#include "paging.h"
#include "pmm.h"
#include "process.h"
#include "sched.h"

#define FRAME_SIZE 4096u
#define MAX_ARGS 8

/* User stack: STACK_PAGES pages ending at the top page, well clear of the
 * executables (0x08048000+) and the heap window (ends at 0x0B000000). */
#define USER_STACK_VADDR 0x0BFFF000u /* top (argv-staging) page */
#define USER_STACK_PAGES 4           /* 16 KiB of stack */

/* Build the initial user stack inside the (kernel-visible) stack frame:
 * argument strings at the top, the argv vector below them, then the
 * argc/argv parameters and a fake return address so _start(argc, argv)
 * reads them per the C calling convention. Returns the initial user esp. */
static uint32_t build_user_stack(uint8_t *stk, const char *cmdline) {
    uint32_t top = FRAME_SIZE;
    uint32_t uargv[MAX_ARGS];
    uint32_t argc = 0;

    const char *p = cmdline;
    while (*p && argc < MAX_ARGS) {
        while (*p == ' ')
            p++;
        if (!*p)
            break;
        uint32_t len = 0;
        while (p[len] && p[len] != ' ')
            len++;

        top -= len + 1;
        for (uint32_t i = 0; i < len; i++)
            stk[top + i] = (uint8_t)p[i];
        stk[top + len] = '\0';
        uargv[argc++] = USER_STACK_VADDR + top;
        p += len;
    }

    top &= ~3u; /* align for the pointer vector */
    top -= 4 * (argc + 1);
    uint32_t *vec = (uint32_t *)(stk + top);
    for (uint32_t i = 0; i < argc; i++)
        vec[i] = uargv[i];
    vec[argc] = 0;
    uint32_t argv_uaddr = USER_STACK_VADDR + top;

    top -= 4;
    *(uint32_t *)(stk + top) = argv_uaddr;
    top -= 4;
    *(uint32_t *)(stk + top) = argc;
    top -= 4;
    *(uint32_t *)(stk + top) = 0; /* fake return address for _start */

    return USER_STACK_VADDR + top;
}

int process_spawn(const char *image_start, const char *image_end,
                  const char *cmdline, int foreground, struct file *in,
                  struct file *out) {
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
    /* Zero the recycled frame — no leaking another process's old data. */
    uint8_t *stk = phys_to_virt(stack_frame);
    for (uint32_t i = 0; i < FRAME_SIZE; i++)
        stk[i] = 0;

    uint32_t user_esp = build_user_stack(stk, cmdline);
    paging_map_user_in(dir, USER_STACK_VADDR, stack_frame, 1);

    /* Map additional zeroed pages below the top so the stack can grow. */
    for (int s = 1; s < USER_STACK_PAGES; s++) {
        uint32_t f = pmm_alloc_frame();
        if (!f) {
            paging_destroy_address_space(dir); /* frees what we mapped */
            return -1;
        }
        uint8_t *z = phys_to_virt(f);
        for (uint32_t i = 0; i < FRAME_SIZE; i++)
            z[i] = 0;
        paging_map_user_in(dir, USER_STACK_VADDR - (uint32_t)s * FRAME_SIZE,
                           f, 1);
    }

    /* argv[0] (the first cmdline word) doubles as the ps name. */
    char name[16];
    uint32_t n = 0;
    while (cmdline[n] && cmdline[n] != ' ' && n < sizeof(name) - 1) {
        name[n] = cmdline[n];
        n++;
    }
    name[n] = '\0';

    int pid =
        sched_spawn_user(dir, entry, user_esp, foreground, name, in, out);
    if (pid < 0) {
        paging_destroy_address_space(dir); /* frees segments + stack too */
        return -1;
    }
    kprintf("[pid %d] spawned (ELF entry %08lx)\n", pid, entry);
    return pid;
}
