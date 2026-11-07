#ifndef SCP55_MIDI_ROUTER_H
#define SCP55_MIDI_ROUTER_H

#include "scp55.h"

#include <stdint.h>

typedef enum {
    MIDI_SOURCE_DIN,
    MIDI_SOURCE_USB
} midi_source_t;

typedef struct {
    uint8_t bytes[512];
    unsigned head;
    unsigned tail;
    unsigned count;
    uint32_t dropped;
    uint32_t send_errors;
    midi_source_t source;
} midi_router_t;

/* Only one raw MIDI stream is selected at a time. Mixing streams byte by
 * byte would corrupt running status and SysEx. Switching clears the queue;
 * the caller should silence active notes before changing sources. */
void midi_router_init(midi_router_t *router, midi_source_t source);
void midi_router_select(midi_router_t *router, midi_source_t source);
int midi_router_din_byte(midi_router_t *router, uint8_t byte);
/* USB-MIDI 1.0 4-byte event packet, cable 0. Returns 0 if accepted. */
int midi_router_usb_event(midi_router_t *router, const uint8_t packet[4]);
/* Send up to limit queued bytes, keeping failed bytes for a later attempt. */
int midi_router_service(midi_router_t *router, const pcmcia_bus_t *bus,
                        const scp55_config_t *config, unsigned limit);

#endif
