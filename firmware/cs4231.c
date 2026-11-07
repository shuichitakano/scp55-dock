#include "cs4231.h"

static int ready(const pcmcia_bus_t *bus, uint16_t base)
{
    uint8_t status;
    if (!bus->delay_ms) return -1;
    for (unsigned i = 0; i < 200; ++i) {
        if (bus->io_read(bus->context, base + 8u, &status)) return -1;
        if (!(status & 0x80)) return 0;
        bus->delay_ms(bus->context, 1);
    }
    return -1;
}

static int write_reg(const pcmcia_bus_t *bus, uint16_t base,
                     uint8_t index, uint8_t value, uint32_t pause_ms)
{
    if (ready(bus, base) || bus->io_write(bus->context, base + 8u, index)) return -1;
    if (pause_ms) bus->delay_ms(bus->context, pause_ms);
    if (bus->io_write(bus->context, base + 9u, value)) return -1;
    if (pause_ms) bus->delay_ms(bus->context, pause_ms);
    return 0;
}

static int read_reg(const pcmcia_bus_t *bus, uint16_t base,
                    uint8_t index, uint8_t *value)
{
    if (ready(bus, base) || bus->io_write(bus->context, base + 8u, index)) return -1;
    return bus->io_read(bus->context, base + 9u, value);
}

int cs4231_unmute(const pcmcia_bus_t *bus, const scp55_config_t *config)
{
    static const uint8_t regs[][2] = {
        {6, 0x00}, {7, 0x00}, {2, 0x08}, {3, 0x08},
        {4, 0x08}, {5, 0x08}
    };
    uint8_t aux, id;
    if (!bus || !config || !bus->io_read || !bus->io_write || !bus->delay_ms) return -1;
    for (unsigned attempt = 0; attempt < 2; ++attempt) {
        uint32_t pause = attempt ? 1u : 0u;
        int failed = 0;
        for (unsigned i = 0; i < sizeof regs / sizeof regs[0]; ++i)
            if (write_reg(bus, config->io_base, regs[i][0], regs[i][1], pause)) {
                failed = 1;
                break;
            }
        if (!failed && !read_reg(bus, config->io_base, 2, &aux) &&
            !read_reg(bus, config->io_base, 12, &id) &&
            (aux & 0x9f) == 0x08 && (id & 0x0f) == 0x0a) return 0;
    }
    return -2;
}
