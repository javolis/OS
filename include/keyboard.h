/* keyboard.h — PS/2 keyboard driver. */
#pragma once

/* Register the IRQ1 handler. Interrupts must be enabled by the caller. */
void keyboard_init(void);
