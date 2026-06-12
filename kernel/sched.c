/* sched.c — preemptive round-robin scheduler.
 *
 * Each task owns a kernel stack; switching tasks means switching kernel
 * stacks (switch_context saves/restores the callee-saved registers and
 * esp). A task's user-mode state lives in the interrupt frame on its own
 * kernel stack: the TSS esp0 is pointed at the running task's stack, so a
 * trap from ring 3 lands there, and a context switch can park the whole
 * task mid-interrupt to be unwound later.
 *
 * Slot 0 is the boot flow (kernel_main), which doubles as the idle task:
 * it is picked only when no user task is ready. schedule() must only be
 * called with interrupts off (IRQ or syscall-gate context). */
#include <stddef.h>
#include <stdint.h>

#include "gdt.h"
#include "io.h"
#include "kheap.h"
#include "kprintf.h"
#include "paging.h"
#include "sched.h"
#include "timer.h"
#include "usermode.h"

#define MAX_TASKS 8
#define KSTACK_SIZE 8192u

enum task_state {
    TASK_FREE = 0,
    TASK_READY,
    TASK_BLOCKED, /* sleeping until wake_at */
    TASK_WAITKBD, /* blocked on keyboard input */
    TASK_WAITPID, /* blocked until wait_pid exits */
    TASK_ZOMBIE,
};

struct task {
    volatile enum task_state state;
    uint32_t pid;
    uint32_t pd_phys;
    uint8_t *kstack; /* kmalloc base; NULL for the boot task */
    uint32_t kstack_top;
    uint32_t kesp;
    uint32_t user_eip;
    uint32_t user_esp;
    uint32_t wake_at;    /* tick at which a BLOCKED task becomes READY */
    uint32_t wait_pid;   /* pid a WAITPID task is waiting on */
    uint32_t parent_pid; /* spawner; foreground reverts here on exit */
};

static struct task tasks[MAX_TASKS]; /* slot 0 = boot/idle task */
static struct task *current;
static volatile int preempt_on;
static uint32_t switch_count;
static uint32_t next_pid = 1;
static volatile uint32_t fg_pid; /* keyboard owner; 0 = kernel shell */

/* boot/usermode.s */
extern void switch_context(uint32_t *save_esp, uint32_t new_esp);

void sched_init(void) {
    tasks[0].state = TASK_READY;
    tasks[0].pid = 0;
    tasks[0].pd_phys = paging_kernel_directory();
    current = &tasks[0];
}

/* First activation of a fresh task: switch_context "returns" here. IF is
 * off (we arrived via interrupt-gate context); the iret frame built by
 * user_iret turns it back on as user code starts. */
static void task_entry(void) {
    user_iret(current->user_eip, current->user_esp);
}

int sched_spawn_user(uint32_t pd_phys, uint32_t user_eip, uint32_t user_esp,
                     int make_foreground) {
    /* Slot selection and pid assignment must not race with other spawners
     * (kernel shell with IF on vs. tasks inside the spawn syscall). */
    uint32_t flags = irq_save();
    struct task *t = NULL;
    for (int i = 1; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_FREE) {
            t = &tasks[i];
            t->state = TASK_ZOMBIE; /* claim the slot; READY published below */
            break;
        }
    }
    irq_restore(flags);
    if (!t)
        return -1;
    uint8_t *stack = kmalloc(KSTACK_SIZE);
    if (!stack) {
        t->state = TASK_FREE;
        return -1;
    }

    t->kstack = stack;
    t->kstack_top = ((uint32_t)stack + KSTACK_SIZE) & ~15u;
    t->pd_phys = pd_phys;
    t->user_eip = user_eip;
    t->user_esp = user_esp;
    t->parent_pid = current->pid;
    uint32_t pf = irq_save();
    t->pid = next_pid++;
    irq_restore(pf);

    /* Forge the stack switch_context expects: callee-saved registers
     * (zeroed) below a return address pointing at task_entry. */
    uint32_t *sp = (uint32_t *)t->kstack_top;
    *--sp = (uint32_t)task_entry;
    *--sp = 0; /* ebx */
    *--sp = 0; /* esi */
    *--sp = 0; /* edi */
    *--sp = 0; /* ebp */
    t->kesp = (uint32_t)sp;

    /* Foreground must be assigned BEFORE the task becomes runnable: the
     * scheduler may run it on the very next tick, and its first readline
     * must already see itself as the keyboard owner. */
    if (make_foreground)
        fg_pid = t->pid;

    /* Publish last: a timer IRQ can run pick_next at any moment, and it
     * must not see READY before the fields above are in place. */
    __asm__ volatile("" : : : "memory");
    t->state = TASK_READY;
    return (int)t->pid;
}

static struct task *pick_next(void) {
    int start = (int)(current - tasks);
    for (int off = 1; off <= MAX_TASKS; off++) {
        int i = (start + off) % MAX_TASKS;
        if (i == 0)
            continue; /* the idle task is the fallback, not a peer */
        if (tasks[i].state == TASK_READY)
            return &tasks[i];
    }
    return &tasks[0];
}

/* Interrupts must be off. */
static void schedule(void) {
    struct task *prev = current;
    struct task *next = pick_next();
    if (next == prev)
        return;

    current = next;
    switch_count++;
    paging_switch(next->pd_phys);
    if (next != &tasks[0]) {
        /* Ring-3 traps for this task must land on its own kernel stack
         * (only possible while it sits empty, i.e. task is in user mode). */
        gdt_set_kernel_stack(next->kstack_top);
    }
    switch_context(&prev->kesp, next->kesp);
    /* prev resumes here on a later schedule(). */
}

void sched_tick(void) {
    if (!preempt_on)
        return;

    /* Wake sleepers whose deadline passed (wraparound-safe comparison). */
    uint32_t now = timer_ticks();
    for (int i = 1; i < MAX_TASKS; i++)
        if (tasks[i].state == TASK_BLOCKED &&
            (int32_t)(now - tasks[i].wake_at) >= 0)
            tasks[i].state = TASK_READY;

    schedule();
}

/* Block the calling task for at least nticks timer ticks. Must be called
 * with interrupts off (syscall context); returns once the sleep elapsed
 * and the scheduler picked the task again. */
void sched_sleep_current(uint32_t nticks) {
    current->wake_at = timer_ticks() + nticks;
    current->state = TASK_BLOCKED;
    schedule();
}

/* Block the calling task until the keyboard IRQ delivers input. Syscall
 * context only: IF is off, so no keystroke can slip in between the
 * caller's buffer check and this block — no lost-wakeup race. */
void sched_block_on_keyboard(void) {
    current->state = TASK_WAITKBD;
    schedule();
}

/* Called from the keyboard IRQ whenever a character is queued. */
void sched_wake_keyboard(void) {
    for (int i = 1; i < MAX_TASKS; i++)
        if (tasks[i].state == TASK_WAITKBD)
            tasks[i].state = TASK_READY;
}

void sched_set_foreground(uint32_t pid) {
    fg_pid = pid;
}

/* Wake tasks blocked in sys_wait on this pid. */
static void wake_waiters(uint32_t pid) {
    for (int i = 1; i < MAX_TASKS; i++)
        if (tasks[i].state == TASK_WAITPID && tasks[i].wait_pid == pid)
            tasks[i].state = TASK_READY;
}

/* Block the calling task until `pid` exits (syscall context; IF off, so
 * the alive-check cannot race an exit). No-op if it's already gone. */
void sched_wait_pid(uint32_t pid) {
    if (pid == current->pid || !sched_pid_alive(pid))
        return;
    current->wait_pid = pid;
    current->state = TASK_WAITPID;
    schedule();
}

uint32_t sched_foreground_pid(void) {
    return fg_pid;
}

int sched_pid_alive(uint32_t pid) {
    for (int i = 1; i < MAX_TASKS; i++)
        if (tasks[i].pid == pid &&
            (tasks[i].state == TASK_READY || tasks[i].state == TASK_BLOCKED ||
             tasks[i].state == TASK_WAITKBD ||
             tasks[i].state == TASK_WAITPID))
            return 1;
    return 0;
}

uint32_t sched_current_pid(void) {
    return current->pid;
}

/* Kill a (non-running) user task by pid: marked zombie, never scheduled
 * again, reclaimed by the next sched_reap. Only the shell calls this and
 * the shell is the running task, so the target is always off-CPU. */
int sched_kill(uint32_t pid) {
    for (int i = 1; i < MAX_TASKS; i++) {
        if (tasks[i].pid == pid &&
            (tasks[i].state == TASK_READY || tasks[i].state == TASK_BLOCKED ||
             tasks[i].state == TASK_WAITKBD ||
             tasks[i].state == TASK_WAITPID)) {
            tasks[i].state = TASK_ZOMBIE;
            wake_waiters(pid);
            if (fg_pid == pid)
                fg_pid = tasks[i].parent_pid; /* keyboard to the parent */
            return 0;
        }
    }
    return -1;
}

void sched_ps(void) {
    static const char *const names[] = {"free",    "ready",   "blocked",
                                        "waitkbd", "waitpid", "zombie"};
    kprintf("  PID  STATE\n");
    for (int i = 0; i < MAX_TASKS; i++) {
        if (i != 0 && tasks[i].state == TASK_FREE)
            continue;
        const char *st =
            (&tasks[i] == current) ? "running" : names[tasks[i].state];
        kprintf("%5lu  %s%s\n", tasks[i].pid, st,
                i == 0 ? " (shell/idle)" : "");
    }
}

void sched_start(void) {
    switch_count = 0;
    preempt_on = 1;
}

void sched_stop(void) {
    preempt_on = 0;
}

int sched_user_tasks_alive(void) {
    for (int i = 1; i < MAX_TASKS; i++)
        if (tasks[i].state == TASK_READY || tasks[i].state == TASK_BLOCKED)
            return 1;
    return 0;
}

uint32_t sched_switch_count(void) {
    return switch_count;
}

void sched_reap(void) {
    for (int i = 1; i < MAX_TASKS; i++) {
        if (tasks[i].state != TASK_ZOMBIE)
            continue;
        paging_destroy_address_space(tasks[i].pd_phys);
        kfree(tasks[i].kstack);
        tasks[i].kstack = NULL;
        tasks[i].state = TASK_FREE;
    }
}

void task_exit(void) {
    kprintf("[pid %lu] exited\n", current->pid);
    if (fg_pid == current->pid)
        fg_pid = current->parent_pid; /* keyboard back to the parent */
    wake_waiters(current->pid);
    current->state = TASK_ZOMBIE;
    schedule(); /* never returns: zombies aren't picked again */
    for (;;)
        __asm__ volatile("hlt");
}
