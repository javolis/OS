/* process.h — user processes: one image in a private address space. */
#pragma once

/* Stage the ELF image in a fresh address space with argc/argv built from
 * the space-separated cmdline (first word = argv[0], conventionally the
 * program name) and register it as a ready task. Returns the pid, or -1
 * on failure. The address space and kernel stack are reclaimed by
 * sched_reap() after the task exits. */
/* foreground=1 hands the new process the keyboard atomically with it
 * becoming runnable (callers must own the foreground themselves).
 * in/out become the child's fds 0/1 (NULL = the console). */
struct file;
int process_spawn(const char *image_start, const char *image_end,
                  const char *cmdline, int foreground, struct file *in,
                  struct file *out);

/* Resolve `fname` to an image (initrd first, then the FAT disk) and spawn it.
 * linux_abi=1 loads it as a Linux process (Linux entry stack + its int 0x80
 * routed to the Linux syscall layer). Returns the pid, or -1 on failure. */
int process_spawn_named(const char *fname, const char *cmdline, int foreground,
                        struct file *in, struct file *out, int linux_abi);
