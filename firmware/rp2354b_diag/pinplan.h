#ifndef SCP55_RP2354B_PINPLAN_H
#define SCP55_RP2354B_PINPLAN_H

#include <stdbool.h>

/* Reserves UART1 RX and one PIO SM for 16-bit stereo 44.1 kHz I2S.
 * Does not start the SM or DMA. Returns false if the PIO resources are busy. */
bool scp55_pinplan_prepare(void);
void scp55_pinplan_restore_pins(void);

/* MIDI activity indicator: GPIO36, active high; call service in main loop. */
void scp55_midi_led_init(void);
void scp55_midi_activity_notify(void);
void scp55_midi_activity_service(void);

#ifdef SCP55_ENABLE_AUDIO_PROBE
#include <stdint.h>
int scp55_i2s_play_u8(const uint8_t *samples, unsigned count);
#include "audio_ring.h"
typedef struct {
    uint32_t blocks;
    uint32_t stalls;
    uint32_t min_fill;
    uint32_t max_fill;
    int32_t correction_ppm;
} scp55_stream_stats_t;
int scp55_i2s_stream_start(scp55_audio_ring_t *ring);
int scp55_i2s_stream_service(void);
void scp55_i2s_stream_stop(void);
scp55_stream_stats_t scp55_i2s_stream_stats(void);
#endif

#endif
