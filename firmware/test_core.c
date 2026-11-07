#include "scp55.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    uint8_t attr[2048];
    unsigned writes;
    uint16_t last_write;
    unsigned unsettled_reads;
} mock_t;

static int attr_read(void *context, uint16_t address, uint8_t *value)
{
    mock_t *mock = context;
    if ((address & 1u) || address / 2u >= sizeof mock->attr) return -1;
    if (mock->unsettled_reads) {
        --mock->unsettled_reads;
        *value = 0xff;
        return 0;
    }
    *value = mock->attr[address / 2u];
    return 0;
}

static int attr_write(void *context, uint16_t address, uint8_t value)
{
    mock_t *mock = context;
    if (address != 0x400) return -1;
    mock->last_write = address;
    ++mock->writes;
    mock->attr[address / 2u] = value;
    return 0;
}

static void delay_ms(void *context, uint32_t milliseconds)
{
    (void)context;
    (void)milliseconds;
}

int main(void)
{
    static const uint8_t cis[] = {
        0x20, 0x04, 0x0c, 0xc0, 0x01, 0x00,
        0x15, 0x1c, 0x04, 0x01,
        'R', 'o', 'l', 'a', 'n', 'd', 0,
        'P', 'C', 'M', 'C', 'I', 'A', ' ', 'S', 'o', 'u', 'n', 'd',
        ' ', 'C', 'a', 'r', 'd', 0, 0xff,
        0x1a, 0x04, 0x01, 0x04, 0x00, 0x04,
        0xff
    };
    mock_t mock = {0};
    scp55_config_t config = {0};
    pcmcia_bus_t bus = {.context = &mock, .attr_read = attr_read,
                        .attr_write = attr_write, .delay_ms = delay_ms};
    memcpy(mock.attr, cis, sizeof cis);
    mock.unsettled_reads = 8; /* cold boot: two all-FF snapshots */
    assert(scp55_wait_cis_stable(&bus) == 0);
    assert(scp55_identify(&bus, &config) == 0);
    assert(config.cor_address == 0x400 && config.io_base == 0x330);
    assert(scp55_set_cor(&bus, &config) == 0);
    assert(mock.writes == 1 && mock.last_write == 0x400);

    mock.writes = 0;
    mock.attr[2] = 0x00; /* wrong manufacturer: no configuration permitted */
    assert(scp55_identify(&bus, &config) != 0);
    assert(scp55_set_cor(&bus, &config) != 0);
    assert(mock.writes == 0);
    mock.attr[2] = 0x0c;
    mock.attr[10] = 'X'; /* wrong product text with correct MANFID */
    assert(scp55_identify(&bus, &config) != 0);
    assert(scp55_set_cor(&bus, &config) != 0);
    assert(mock.writes == 0);
    return 0;
}
