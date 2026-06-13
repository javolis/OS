/* sched.h — preemptive round-robin scheduler. */
#pragma once
#include <stdint.h>

void sched_init(void);

/* Register a user task: it first runs by ireting to user_eip:user_esp in
 * the given address space, on its own kernel stack. make_foreground hands
 * it the keyboard atomically with becoming runnable; name (argv[0]) is
 * kept for ps; in/out become fds 0/1 (NULL = the console). Returns pid
 * or -1. */
int sched_spawn_user(uint32_t pd_phys, uint32_t user_eip, uint32_t user_esp,
                     int make_foreground, const char *name,
                     struct file *in, struct file *out);

void sched_start(void); /* enable timer-driven preemption */
void sched_stop(void);

int sched_user_tasks_alive(void);
void sched_reap(void); /* free zombie address spaces + kernel stacks */
uint32_t sched_switch_count(void);

/* Called by the timer IRQ on every tick (no-op while stopped). */
void sched_tick(void);

/* Block the calling task for at least nticks ticks (syscall context). */
void sched_sleep_current(uint32_t nticks);

/* Block the calling task until keyboard input arrives (syscall context). */
void sched_block_on_keyboard(void);

/* Block on / wake an opaque channel — pipes, and future blocking objects. */
void sched_block_on_chan(void *chan);
void sched_wake_chan(void *chan);

/* Block the calling task until `pid` exits, then collect its exit status
 * and reclaim it (syscall context). *status_out = (uint32_t)-1 if the pid
 * is unknown or already collected. */
void sched_wait_pid(uint32_t pid, uint32_t *status_out);

/* Wake keyboard-blocked tasks (keyboard IRQ context). */
void sched_wake_keyboard(void);

/* The foreground task owns the keyboard; pid 0 = the kernel shell. */
void sched_set_foreground(uint32_t pid);
uint32_t sched_foreground_pid(void);

/* Ctrl+C (keyboard IRQ context): kill the foreground task, if any. */
void sched_interrupt_foreground(void);

int sched_pid_alive(uint32_t pid);

/* Number of live user tasks. */
uint32_t sched_alive_count(void);

uint32_t sched_current_pid(void);

/* Current task's file-descriptor table (syscall context). */
struct file;
struct file *sched_get_fd(int fd);
int sched_install_fd(struct file *f); /* first free slot, or -1 */
void sched_clear_fd(int fd);

/* Print the task table via kprintf (the shell's ps command). */
void sched_ps(void);

/* Mark a ready/blocked user task zombie. 0 on success, -1 if no match. */
int sched_kill(uint32_t pid);

/* Terminate the calling task with an exit code (exit syscall, faults). */
void task_exit(uint32_t code) __attribute__((noreturn));
