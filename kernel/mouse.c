/* mouse.c - PS/2 mouse driver (8042 auxiliary device, IRQ12).
 *
 * The mouse shares the 8042 controller with the keyboard. We enable the aux
 * port, turn on IRQ12 in the controller config, and tell the mouse to stream
 * 3-byte movement packets. IRQ12 assembles each packet (button flags + signed
 * X/Y deltas) and integrates it into an absolute cursor position clamped to
 * the framebuffer. */
#include <stddef.h>
#include <stdint.h>

#include "fb.h"
#include "io.h"
#include "irq.h"
#include "kprintf.h"
#include "mouse.h"
#include "pic.h"

#define PS2_DATA 0x60
#define PS2_STATUS 0x64
#define PS2_CMD 0x64

#define ST_OUTPUT_FULL 0x01
#define ST_INPUT_FULL 0x02
#define ST_AUX_DATA 0x20

static int present;
static volatile int mx, my;
static volatile uint32_t buttons;
static int scr_w = 800, scr_h = 600;

static uint8_t pkt[3];
static int pkt_i;

static void wait_write(void) {
    for (int i = 0; i < 100000; i++)
        if (!(inb(PS2_STATUS) & ST_INPUT_FULL))
            return;
}
static void wait_read(void) {
    for (int i = 0; i < 100000; i++)
        if (inb(PS2_STATUS) & ST_OUTPUT_FULL)
            return;
}

/* Send a command byte to the mouse (0xD4 prefix) and read its ACK. */
static void mouse_cmd(uint8_t b) {
    wait_write();
    outb(PS2_CMD, 0xD4);
    wait_write();
    outb(PS2_DATA, b);
    wait_read();
    (void)inb(PS2_DATA); /* ACK (0xFA) */
}

static void mouse_irq(struct registers *regs) {
    (void)regs;
    /* Only consume bytes the controller marks as auxiliary (mouse) data. */
    if (!(inb(PS2_STATUS) & ST_AUX_DATA))
        return;
    uint8_t b = inb(PS2_DATA);

    if (pkt_i == 0 && !(b & 0x08))
        return; /* not a valid first byte (sync bit must be set): resync */
    pkt[pkt_i++] = b;
    if (pkt_i < 3)
        return;
    pkt_i = 0;

    uint8_t flags = pkt[0];
    if (flags & 0xC0)
        return; /* X/Y overflow: drop the packet */
    int dx = (int)pkt[1] - ((flags & 0x10) ? 256 : 0);
    int dy = (int)pkt[2] - ((flags & 0x20) ? 256 : 0);
    int nx = mx + dx;
    int ny = my - dy; /* screen Y grows downward; mouse Y grows up */
    if (nx < 0)
        nx = 0;
    if (nx > scr_w - 1)
        nx = scr_w - 1;
    if (ny < 0)
        ny = 0;
    if (ny > scr_h - 1)
        ny = scr_h - 1;
    mx = nx;
    my = ny;
    buttons = flags & 0x07;
}

void mouse_init(void) {
    if (fb_available()) {
        scr_w = (int)fb_width();
        scr_h = (int)fb_height();
    }
    mx = scr_w / 2;
    my = scr_h / 2;

    wait_write();
    outb(PS2_CMD, 0xA8); /* enable the auxiliary (mouse) port */

    wait_write();
    outb(PS2_CMD, 0x20); /* read controller config byte */
    wait_read();
    uint8_t cfg = inb(PS2_DATA);
    cfg |= 0x02;  /* enable IRQ12 (aux interrupt) */
    cfg &= ~0x20; /* enable the aux clock */
    wait_write();
    outb(PS2_CMD, 0x60); /* write controller config byte */
    wait_write();
    outb(PS2_DATA, cfg);

    mouse_cmd(0xF6); /* set defaults */
    mouse_cmd(0xF4); /* enable data reporting */

    pkt_i = 0;
    present = 1;
    irq_register(12, mouse_irq);
    pic_unmask(12);
    kprintf("mouse: PS/2 ready (%dx%d)\n", scr_w, scr_h);
}

int mouse_present(void) {
    return present;
}

void mouse_state(int *x, int *y, uint32_t *b) {
    *x = mx;
    *y = my;
    *b = buttons;
}
