#ifndef SCP55_AUDIO_RESAMPLE_H
#define SCP55_AUDIO_RESAMPLE_H
#include "audio_ring.h"
#include <stdbool.h>
typedef struct { int32_t dc_q8; int32_t previous; bool primed; uint32_t clipped; } scp55_resampler_t;
typedef struct { scp55_resampler_t left, right; unsigned factor, dc_divisor, history_pos; int32_t history_left[16], history_right[16]; } scp55_stereo_resampler_t;
/* 11025: 4x linear; 22050: 2x half-band FIR; 44100: direct.
 * Stereo modes use unity gain and independent DC removal; saturation only
 * at the signed 16-bit output limits. No half-scale limiter or 8x boost. */
int scp55_stereo_resampler_init_rate(scp55_stereo_resampler_t *state, unsigned rate);
void scp55_stereo_resampler_init(scp55_stereo_resampler_t *state);
int scp55_stereo_resampler_push(scp55_stereo_resampler_t *state, scp55_audio_ring_t *ring, int16_t left, int16_t right);
void scp55_resampler_init(scp55_resampler_t *state);
/* 11025 Hz signed mono -> 44100 Hz stereo, 4x linear interpolation.
 * 8x bench gain, DC removal, half-scale limiter. Capacity is checked before enqueueing all four frames. */
int scp55_resampler_push(scp55_resampler_t *state, scp55_audio_ring_t *ring, int16_t sample);
#endif
