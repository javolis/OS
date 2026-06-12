/* usermode.h — ring-3 entry (boot/usermode.s). */
#pragma once
#include <stdint.h>

/* Iret into ring 3 at `entry` with the given user stack. Returns when the
 * user program invokes the exit syscall. */
void enter_user_mode(uint32_t entry, uint32_t user_stack_top);
