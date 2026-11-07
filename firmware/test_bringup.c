#include "bringup.h"

#include <assert.h>
#include <stdbool.h>
#include <string.h>

typedef struct {
    uint8_t attr[1024], regs[32], index, midi[6];
    unsigned cor_writes, midi_count, held_ms;
    bool ack, fail_cor, fail_codec, fail_ack, fail_note_off;
} mock_t;

static int attr_read(void *ctx, uint16_t address, uint8_t *value)
{
    mock_t *m = ctx;
    if (address & 1u || address / 2 >= sizeof m->attr) return -1;
    *value = m->attr[address / 2];
    return 0;
}

static int attr_write(void *ctx, uint16_t address, uint8_t value)
{
    mock_t *m = ctx;
    assert(address == 0x400);
    if (m->fail_cor) return -1;
    ++m->cor_writes;
    m->attr[address / 2] = value;
    return 0;
}

static int io_read(void *ctx, uint16_t address, uint8_t *value)
{
    mock_t *m = ctx;
    assert(m->cor_writes == 1);
    switch (address) {
    case 0x338: *value = m->fail_codec ? 0x80 : 0; break;
    case 0x339: *value = m->index == 12 ? 0x0a : m->regs[m->index]; break;
    case 0x331: *value = m->ack ? 0 : 0x80; break;
    case 0x330: *value = m->fail_ack ? 0 : 0xfe; m->ack = false; break;
    default: assert(false);
    }
    return 0;
}

static int io_write(void *ctx, uint16_t address, uint8_t value)
{
    mock_t *m = ctx;
    assert(m->cor_writes == 1);
    switch (address) {
    case 0x338: assert(value < 32); m->index = value; break;
    case 0x339: m->regs[m->index] = value; break;
    case 0x331: m->ack = value == 0x3f; break;
    case 0x330:
        if (m->fail_note_off && m->midi_count == 3) return -1;
        assert(m->midi_count < sizeof m->midi);
        m->midi[m->midi_count++] = value;
        break;
    default: assert(false);
    }
    return 0;
}

static void delay_ms(void *ctx, uint32_t ms)
{
    mock_t *m = ctx;
    if (m->midi_count == 3) m->held_ms += ms;
}

static mock_t fresh(void)
{
    static const uint8_t cis[] = {
        0x20, 4, 0x0c, 0xc0, 1, 0,
        0x15, 0x1c, 4, 1,
        'R', 'o', 'l', 'a', 'n', 'd', 0,
        'P', 'C', 'M', 'C', 'I', 'A', ' ', 'S', 'o', 'u', 'n', 'd',
        ' ', 'C', 'a', 'r', 'd', 0, 0xff,
        0x1a, 4, 1, 4, 0, 4, 0xff
    };
    mock_t m = {0};
    memcpy(m.attr, cis, sizeof cis);
    return m;
}

int main(void)
{
    mock_t m = fresh();
    pcmcia_bus_t bus = {.context = &m, .attr_read = attr_read,
        .attr_write = attr_write, .io_read = io_read, .io_write = io_write,
        .delay_ms = delay_ms};
    scp55_config_t config;
    assert(scp55_bringup(&bus, &config) == 0);
    assert(scp55_note_test(&bus, &config) == 0);
    const uint8_t expected[] = {0x90, 60, 64, 0x80, 60, 0};
    assert(m.midi_count == sizeof expected);
    assert(memcmp(m.midi, expected, sizeof expected) == 0);
    assert(m.held_ms == 300);

    m = fresh(); m.attr[2] = 0;
    assert(scp55_bringup(&bus, &config) == -2);
    assert(m.cor_writes == 0 && m.midi_count == 0);
    m = fresh(); m.fail_cor = true;
    assert(scp55_bringup(&bus, &config) == -3);
    m = fresh(); m.fail_codec = true;
    assert(scp55_bringup(&bus, &config) == -4);
    assert(m.midi_count == 0);
    m = fresh(); m.fail_ack = true;
    assert(scp55_bringup(&bus, &config) == -5);
    assert(m.midi_count == 0);
    m = fresh(); m.fail_note_off = true;
    assert(scp55_bringup(&bus, &config) == 0);
    assert(scp55_note_test(&bus, &config) == -3);
    assert(m.midi_count == 3);
    return 0;
}
