/* speaker.h — PC speaker (PIT channel 2) square-wave beeper. */
#pragma once
#include <stdint.h>

void speaker_init(void);          /* ensure the speaker is silent at boot */
void speaker_tone(uint32_t freq); /* start a continuous square wave (Hz) */
void speaker_off(void);           /* silence */
