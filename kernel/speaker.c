/* speaker.c — PC speaker beeper.
 *
 * The PC speaker is a square-wave generator driven by PIT channel 2 and gated
 * through the low two bits of the 8042 control port 0x61 (bit 0 = timer gate,
 * bit 1 = speaker data). Program channel 2's divisor for the wanted frequency,
 * then set both gate bits so the square wave reaches the cone; clear them to
 * silence it. Channel 0 (the system tick) is untouched. */
#include <stdint.h>

#include "io.h"
#include "speaker.h"

#define PIT_CH2 0x42
#define PIT_CMD 0x43
#define PORT_B 0x61 /* 8042 control port B */
#define PIT_INPUT_HZ 1193182u

void speaker_tone(uint32_t freq) {
    if (freq < 19)
        freq = 19; /* keep the divisor inside 16 bits */
    if (freq > 20000)
        freq = 20000;
    uint32_t divisor = PIT_INPUT_HZ / freq;
    if (divisor > 0xFFFF)
        divisor = 0xFFFF;
    if (divisor == 0)
        divisor = 1;

    outb(PIT_CMD, 0xB6); /* channel 2, lobyte/hibyte, mode 3 (square wave) */
    outb(PIT_CH2, (uint8_t)(divisor & 0xFF));
    outb(PIT_CH2, (uint8_t)((divisor >> 8) & 0xFF));

    uint8_t v = inb(PORT_B);
    if ((v & 0x03) != 0x03)
        outb(PORT_B, (uint8_t)(v | 0x03)); /* open the gate + data line */
}

void speaker_off(void) {
    uint8_t v = inb(PORT_B);
    outb(PORT_B, (uint8_t)(v & ~0x03));
}

void speaker_init(void) {
    speaker_off();
}
