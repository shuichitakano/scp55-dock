#ifndef SCP55_AUDIO_CAPTURE_H
#define SCP55_AUDIO_CAPTURE_H

#include "scp55.h"

typedef enum {
    SCP55_CAPTURE_LINE = 0,
    SCP55_CAPTURE_AUX1 = 1,
    SCP55_CAPTURE_OUTPUT_LOOPBACK = 3
} scp55_capture_source_t;

typedef struct {
    uint32_t status_reads;
    uint32_t sample_errors;
    uint32_t timeout_count;
} scp55_capture_stats_t;

/* Experimental, opt-in 5.512 kHz / mono / unsigned 8-bit PIO capture.
 * These register settings are supported by the CS4231A datasheet, but their
 * capture behavior through the SCP-55 PC Card bridge remains unverified.
 * Start only after scp55_bringup(). An error must be followed by stop or
 * by a card power cycle before another attempt. */
int scp55_capture_start(const pcmcia_bus_t *bus, const scp55_config_t *config,
                        scp55_capture_source_t source);
/* MODE 1: 0x00=u8/5512.5, 0x40=s16le/5512.5, 0x42=s16le mono/11025, 0x52=s16le stereo/11025, 0x56=stereo/22050, 0x5a=stereo/44100.
 * SCP-55 uses its 16.9344 MHz crystal on XI1 (source bit remains zero). */
int scp55_capture_start_format(const pcmcia_bus_t *bus, const scp55_config_t *config,
                              scp55_capture_source_t source, uint8_t format);
int scp55_capture_read(const pcmcia_bus_t *bus, const scp55_config_t *config,
                       uint8_t *sample, scp55_capture_stats_t *stats);
/* Nonblocking: 0=sample, 1=not ready, negative=bus/argument error. */
int scp55_capture_try_read(const pcmcia_bus_t *bus, const scp55_config_t *config,
                          uint8_t *sample, scp55_capture_stats_t *stats);
/* MODE 1 signed 16-bit stereo (0x52/0x56/0x5a). Poll once, then read a whole L-low/L-high/R-low/R-high
 * frame as permitted by CS4231A PIO interface (datasheet pp.15,30).
 * 0=frame, 1=not ready; negative=bus error, SER or frame alignment failure. */
int scp55_capture_try_stereo(const pcmcia_bus_t *bus, const scp55_config_t *config,
    int16_t *left, int16_t *right, scp55_capture_stats_t *stats);
int scp55_capture_stop(const pcmcia_bus_t *bus, const scp55_config_t *config);

#endif
