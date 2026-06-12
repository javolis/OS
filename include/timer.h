/* timer.h — PIT (8253/8254) system timer on IRQ0. */
#pragma once
#include <stdint.h>

void timer_init(uint32_t frequency_hz);
uint32_t timer_ticks(void);
