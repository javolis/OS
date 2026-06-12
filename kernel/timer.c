/* timer.c — PIT (8253/8254) channel 0: periodic tick on IRQ0. */
#include <stdint.h>

#include "io.h"
#include "irq.h"
#include "pic.h"
#include "sched.h"
#include "timer.h"

#define PIT_CH0 0x40
#define PIT_CMD 0x43
#define PIT_INPUT_HZ 1193182u /* PIT input clock */

static volatile uint32_t ticks;

static void timer_irq(struct registers *regs) {
    (void)regs;
    ticks++;
    sched_tick(); /* may context-switch away and return much later */
}

void timer_init(uint32_t frequency_hz) {
    uint32_t divisor = PIT_INPUT_HZ / frequency_hz;
    if (divisor == 0 || divisor > 0xFFFF)
        divisor = 0xFFFF; /* clamp to the slowest rate (~18.2 Hz) */

    outb(PIT_CMD, 0x36); /* channel 0, lobyte/hibyte, mode 3 (square wave) */
    outb(PIT_CH0, divisor & 0xFF);
    outb(PIT_CH0, (divisor >> 8) & 0xFF);

    irq_register(0, timer_irq);
    pic_unmask(0);
}

uint32_t timer_ticks(void) {
    return ticks;
}

void timer_sleep(uint32_t ms) {
    /* 100 Hz -> 10 ms per tick; round up so short sleeps aren't zero. */
    uint32_t target = ticks + (ms + 9) / 10;
    /* Signed difference keeps this correct across tick-counter wraparound. */
    while ((int32_t)(ticks - target) < 0)
        __asm__ volatile("hlt");
}
