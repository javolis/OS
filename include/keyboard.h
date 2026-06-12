/* keyboard.h — PS/2 keyboard on IRQ1. */
#pragma once

void keyboard_init(void);

/* Blocking read of the next queued character (requires interrupts enabled). */
char keyboard_getchar(void);
