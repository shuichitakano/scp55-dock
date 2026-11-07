#ifndef SCP55_AUDIO_RING_H
#define SCP55_AUDIO_RING_H

#include <stdatomic.h>
#include <stdint.h>

#define SCP55_AUDIO_RING_FRAMES 1024u

typedef struct {
    int16_t left;
    int16_t right;
} scp55_audio_frame_t;

/* Single producer (capture) / single consumer (I2S feed) ring. */
typedef struct {
    scp55_audio_frame_t frames[SCP55_AUDIO_RING_FRAMES];
    _Atomic uint32_t head;
    _Atomic uint32_t tail;
    _Atomic uint32_t overrun_frames;
    _Atomic uint32_t underrun_frames;
} scp55_audio_ring_t;

void scp55_audio_ring_init(scp55_audio_ring_t *ring);
int scp55_audio_ring_push(scp55_audio_ring_t *ring, scp55_audio_frame_t frame);
/* Experimental 8-bit mono capture → 16-bit stereo for DAC diagnosis. */
int scp55_audio_ring_push_u8_mono(scp55_audio_ring_t *ring, uint8_t sample);
/* Outputs silence and returns -1 if empty. */
int scp55_audio_ring_pop(scp55_audio_ring_t *ring, scp55_audio_frame_t *frame);
uint32_t scp55_audio_ring_count(const scp55_audio_ring_t *ring);

#endif
