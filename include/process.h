/* process.h — minimal processes: one user image in a private address space. */
#pragma once

/* Stage the image in a fresh address space, run it in ring 3 until it
 * invokes the exit syscall, then reclaim the address space.
 * Returns 0 on success, -1 if staging failed. */
int process_run(const char *image_start, const char *image_end);
