/* ac97.h — Intel AC'97 audio controller (ICH) PCM output. */
#pragma once
#include <stdint.h>

int ac97_init(void);    /* find + reset the codec; 0 if present, -1 if not */
int ac97_present(void); /* 1 once initialized */

/* Largest playback the internal DMA buffer holds, in 16-bit samples
 * (interleaved L,R for stereo). */
uint32_t ac97_capacity(void);

/* Copy `count` 16-bit samples into the DMA buffer and start a one-shot
 * playback (48 kHz, 16-bit stereo). Returns the number of samples queued
 * (capped at ac97_capacity()), or -1 if no codec. Does not block. */
int ac97_play(const int16_t *samples, uint32_t count);

int ac97_busy(void); /* 1 while the DMA engine is still draining the buffer */
void ac97_stop(void); /* halt playback */
