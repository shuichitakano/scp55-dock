#include "midi_router.h"

#include "mpu401.h"

#include <stddef.h>

#define CAPACITY (sizeof(((midi_router_t *)0)->bytes))

static int enqueue(midi_router_t *router, const uint8_t *data, unsigned length)
{
    if (length > CAPACITY - router->count) {
        router->dropped += length;
        return -1;
    }
    for (unsigned i = 0; i < length; ++i) {
        router->bytes[router->head] = data[i];
        router->head = (router->head + 1u) % CAPACITY;
    }
    router->count += length;
    return 0;
}

void midi_router_init(midi_router_t *router, midi_source_t source)
{
    if (!router) return;
    router->head = router->tail = router->count = 0;
    router->dropped = router->send_errors = 0;
    router->source = source;
}

void midi_router_select(midi_router_t *router, midi_source_t source)
{
    if (!router || router->source == source) return;
    router->head = router->tail = router->count = 0;
    router->source = source;
}

int midi_router_din_byte(midi_router_t *router, uint8_t byte)
{
    if (!router || router->source != MIDI_SOURCE_DIN) return -1;
    return enqueue(router, &byte, 1);
}

int midi_router_usb_event(midi_router_t *router, const uint8_t packet[4])
{
    static const uint8_t cin_length[16] = {
        0, 0, 2, 3, 3, 1, 2, 3, 3, 3, 3, 3, 2, 2, 3, 1
    };
    if (!router || !packet || router->source != MIDI_SOURCE_USB ||
        (packet[0] >> 4) != 0) return -1;
    unsigned length = cin_length[packet[0] & 0x0f];
    if (!length) return -2;
    return enqueue(router, packet + 1, length);
}

int midi_router_service(midi_router_t *router, const pcmcia_bus_t *bus,
                        const scp55_config_t *config, unsigned limit)
{
    if (!router || !bus || !config) return -1;
    unsigned sent = 0;
    while (router->count && sent < limit) {
        uint8_t byte = router->bytes[router->tail];
        if (mpu401_send(bus, config, byte)) {
            ++router->send_errors;
            return -2;
        }
        router->tail = (router->tail + 1u) % CAPACITY;
        --router->count;
        ++sent;
    }
    return (int)sent;
}
