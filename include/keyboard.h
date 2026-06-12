/* keyboard.h — PS/2 keyboard on IRQ1. */
#pragma once

/* Non-ASCII keys are delivered as values above 0x7F (the keymaps only
 * produce ASCII, so these can't collide with typed characters). */
#define KEY_UP ((char)0x80)
#define KEY_DOWN ((char)0x81)

void keyboard_init(void);

/* Blocking read of the next queued character (requires interrupts enabled). */
char keyboard_getchar(void);
