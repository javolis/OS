/* mouse.h - PS/2 mouse (IRQ12): absolute cursor position + button state. */
#pragma once
#include <stdint.h>

/* Button bitmask. */
#define MOUSE_LEFT 0x01
#define MOUSE_RIGHT 0x02
#define MOUSE_MIDDLE 0x04

/* Enable the 8042 auxiliary device and start tracking the mouse. */
void mouse_init(void);

int mouse_present(void);

/* Current cursor position (clamped to the screen) and button bitmask. */
void mouse_state(int *x, int *y, uint32_t *buttons);

/* Pointer speed as a percent (deltas are scaled by it). Clamped to 25-400;
 * returns the clamped value. */
int mouse_set_speed(int pct);
