/* sched.h — preemptive round-robin scheduler. */
#pragma once
#include <stdint.h>

void sched_init(void);

/* Register a user task: it first runs by ireting to user_eip:user_esp in
 * the given address space, on its own kernel stack. Returns pid or -1. */
int sched_spawn_user(uint32_t pd_phys, uint32_t user_eip, uint32_t user_esp);

void sched_start(void); /* enable timer-driven preemption */
void sched_stop(void);

int sched_user_tasks_alive(void);
void sched_reap(void); /* free zombie address spaces + kernel stacks */
uint32_t sched_switch_count(void);

/* Called by the timer IRQ on every tick (no-op while stopped). */
void sched_tick(void);

/* Block the calling task for at least nticks ticks (syscall context). */
void sched_sleep_current(uint32_t nticks);

uint32_t sched_current_pid(void);

/* Print the task table via kprintf (the shell's ps command). */
void sched_ps(void);

/* Mark a ready/blocked user task zombie. 0 on success, -1 if no match. */
int sched_kill(uint32_t pid);

/* Terminate the calling task (used by the exit syscall). */
void task_exit(void) __attribute__((noreturn));
