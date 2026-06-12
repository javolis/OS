/* process.h — user processes: one image in a private address space. */
#pragma once

/* Stage the ELF image in a fresh address space with argc/argv built from
 * the space-separated cmdline (first word = argv[0], conventionally the
 * program name) and register it as a ready task. Returns the pid, or -1
 * on failure. The address space and kernel stack are reclaimed by
 * sched_reap() after the task exits. */
/* foreground=1 hands the new process the keyboard atomically with it
 * becoming runnable (callers must own the foreground themselves). */
int process_spawn(const char *image_start, const char *image_end,
                  const char *cmdline, int foreground);
