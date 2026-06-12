/* usermode.h — ring-3 entry (boot/usermode.s). */
#pragma once
#include <stdint.h>

/* Iret into ring 3 at `entry` with the given user stack. Never returns;
 * the task re-enters the kernel only via interrupts and syscalls. */
void user_iret(uint32_t entry, uint32_t user_stack_top)
    __attribute__((noreturn));
