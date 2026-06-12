/* keyboard.c — PS/2 keyboard (IRQ1): translate scancode set 1 make codes to
 * ASCII and echo them to the terminal and serial. Releases, modifiers, and
 * extended (0xE0-prefixed) keys are ignored for now. */
#include <stdint.h>

#include "io.h"
#include "irq.h"
#include "keyboard.h"
#include "pic.h"
#include "serial.h"
#include "term.h"

#define KBD_DATA 0x60

/* US QWERTY, scancode set 1 make codes 0x00-0x39; 0 = unmapped. */
static const char keymap[] = {
    0,   0,    '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 0,
    0,   'q',  'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,   'a',  's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,   '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0,    ' ',
};

static void keyboard_irq(struct registers *regs) {
    (void)regs;
    uint8_t scancode = inb(KBD_DATA);

    if (scancode & 0x80)
        return; /* key release */

    if (scancode < sizeof(keymap) && keymap[scancode]) {
        term_putchar(keymap[scancode]);
        serial_putchar(keymap[scancode]);
    }
}

void keyboard_init(void) {
    irq_register(1, keyboard_irq);
    pic_unmask(1);
}
