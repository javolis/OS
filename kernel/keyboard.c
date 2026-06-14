/* keyboard.c — PS/2 keyboard (IRQ1): translate scancode set 1 to ASCII with
 * shift support and queue characters for consumers (see keyboard_getchar).
 * Extended (0xE0-prefixed) keys and other modifiers are ignored for now. */
#include <stddef.h>
#include <stdint.h>

#include "io.h"
#include "irq.h"
#include "keyboard.h"
#include "pic.h"
#include "sched.h"

#define KBD_DATA 0x60

#define SC_LSHIFT 0x2A
#define SC_RSHIFT 0x36
#define SC_CTRL 0x1D /* right ctrl is the same code behind an 0xE0 prefix */
#define SC_RELEASE 0x80

/* Scancode set 1 make codes for a US QWERTY layout. 0 = no printable char. */
static const char keymap[128] = {
    [0x01] = 27, /* Esc -> 0x1B, so UIs can use it to go back */
    [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4', [0x06] = '5',
    [0x07] = '6', [0x08] = '7', [0x09] = '8', [0x0A] = '9', [0x0B] = '0',
    [0x0C] = '-', [0x0D] = '=', [0x0E] = '\b', [0x0F] = '\t',
    [0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r', [0x14] = 't',
    [0x15] = 'y', [0x16] = 'u', [0x17] = 'i', [0x18] = 'o', [0x19] = 'p',
    [0x1A] = '[', [0x1B] = ']', [0x1C] = '\n',
    [0x1E] = 'a', [0x1F] = 's', [0x20] = 'd', [0x21] = 'f', [0x22] = 'g',
    [0x23] = 'h', [0x24] = 'j', [0x25] = 'k', [0x26] = 'l',
    [0x27] = ';', [0x28] = '\'', [0x29] = '`',
    [0x2B] = '\\', [0x2C] = 'z', [0x2D] = 'x', [0x2E] = 'c', [0x2F] = 'v',
    [0x30] = 'b', [0x31] = 'n', [0x32] = 'm',
    [0x33] = ',', [0x34] = '.', [0x35] = '/',
    [0x37] = '*', [0x39] = ' ',
};

static const char keymap_shift[128] = {
    [0x01] = 27, /* Esc */
    [0x02] = '!', [0x03] = '@', [0x04] = '#', [0x05] = '$', [0x06] = '%',
    [0x07] = '^', [0x08] = '&', [0x09] = '*', [0x0A] = '(', [0x0B] = ')',
    [0x0C] = '_', [0x0D] = '+', [0x0E] = '\b', [0x0F] = '\t',
    [0x10] = 'Q', [0x11] = 'W', [0x12] = 'E', [0x13] = 'R', [0x14] = 'T',
    [0x15] = 'Y', [0x16] = 'U', [0x17] = 'I', [0x18] = 'O', [0x19] = 'P',
    [0x1A] = '{', [0x1B] = '}', [0x1C] = '\n',
    [0x1E] = 'A', [0x1F] = 'S', [0x20] = 'D', [0x21] = 'F', [0x22] = 'G',
    [0x23] = 'H', [0x24] = 'J', [0x25] = 'K', [0x26] = 'L',
    [0x27] = ':', [0x28] = '"', [0x29] = '~',
    [0x2B] = '|', [0x2C] = 'Z', [0x2D] = 'X', [0x2E] = 'C', [0x2F] = 'V',
    [0x30] = 'B', [0x31] = 'N', [0x32] = 'M',
    [0x33] = '<', [0x34] = '>', [0x35] = '?',
    [0x37] = '*', [0x39] = ' ',
};

static int shift_held;
static int ctrl_held;
static int e0_prefix; /* last byte was 0xE0 (extended-key prefix) */

/* Ring buffer filled by the IRQ handler and drained by keyboard_getchar().
 * Single producer (the IRQ) / single consumer (the kernel main loop), so
 * volatile indexes are sufficient — no locking needed. */
#define KBD_BUF_SIZE 256
static char kbd_buf[KBD_BUF_SIZE];
static volatile size_t kbd_head; /* next write slot (IRQ handler) */
static volatile size_t kbd_tail; /* next read slot (consumer) */

static void push(char c) {
    size_t next = (kbd_head + 1) % KBD_BUF_SIZE;
    if (next == kbd_tail)
        return; /* buffer full: drop the keystroke */
    kbd_buf[kbd_head] = c;
    kbd_head = next;
    sched_wake_keyboard(); /* unblock tasks waiting in sys_readline */
}

static void keyboard_irq(struct registers *regs) {
    (void)regs;
    uint8_t scancode = inb(KBD_DATA);

    if (scancode == 0xE0) { /* extended key: next byte carries the code */
        e0_prefix = 1;
        return;
    }

    if (scancode & SC_RELEASE) {
        uint8_t code = scancode & ~SC_RELEASE;
        if (!e0_prefix && (code == SC_LSHIFT || code == SC_RSHIFT))
            shift_held = 0;
        if (code == SC_CTRL) /* left, or right behind the prefix */
            ctrl_held = 0;
        e0_prefix = 0;
        return;
    }

    if (e0_prefix) {
        e0_prefix = 0;
        if (scancode == 0x48)
            push(KEY_UP);
        else if (scancode == 0x50)
            push(KEY_DOWN);
        else if (scancode == SC_CTRL)
            ctrl_held = 1; /* right ctrl */
        return; /* other extended keys still ignored */
    }

    if (scancode == SC_LSHIFT || scancode == SC_RSHIFT) {
        shift_held = 1;
        return;
    }
    if (scancode == SC_CTRL) {
        ctrl_held = 1;
        return;
    }

    char c = shift_held ? keymap_shift[scancode] : keymap[scancode];
    if (!c)
        return;

    if (ctrl_held && (c == 'c' || c == 'C')) {
        sched_interrupt_foreground(); /* Ctrl+C: never reaches the buffer */
        return;
    }
    push(c);
}

int keyboard_trygetchar(void) {
    if (kbd_tail == kbd_head)
        return -1;
    char c = kbd_buf[kbd_tail];
    kbd_tail = (kbd_tail + 1) % KBD_BUF_SIZE;
    return (unsigned char)c;
}

char keyboard_getchar(void) {
    /* hlt until any interrupt arrives. If a keystroke races in between the
     * emptiness check and the hlt, the 100 Hz timer still wakes us within
     * 10 ms to re-check, so this can't deadlock. */
    int c;
    while ((c = keyboard_trygetchar()) < 0)
        __asm__ volatile("hlt");
    return (char)c;
}

void keyboard_init(void) {
    irq_register(1, keyboard_irq);
    pic_unmask(1);
}
