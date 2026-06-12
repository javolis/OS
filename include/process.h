/* process.h — user processes: one image in a private address space. */
#pragma once

/* Stage the image in a fresh address space and register it with the
 * scheduler as a ready task. Returns the pid, or -1 on failure. The
 * address space and kernel stack are reclaimed by sched_reap() after
 * the task exits. */
int process_spawn(const char *image_start, const char *image_end);
