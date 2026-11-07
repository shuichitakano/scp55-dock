#include "mpu401.h"

int mpu401_try_send(const pcmcia_bus_t *bus, const scp55_config_t *config, uint8_t byte)
{
    uint8_t status;
    if (!bus || !config || !bus->io_read || !bus->io_write) return -1;
    if (bus->io_read(bus->context, config->io_base + 1u, &status)) return -2;
    if (status & 0x40) return 1;
    return bus->io_write(bus->context, config->io_base, byte) ? -2 : 0;
}

static int wait_status(const pcmcia_bus_t *bus, uint16_t base, uint8_t mask,
                       uint8_t expected)
{
    uint8_t status;
    for (unsigned i = 0; i < 500; ++i) {
        if (bus->io_read(bus->context, base + 1u, &status)) return -1;
        if ((status & mask) == expected) return 0;
        bus->delay_ms(bus->context, 1);
    }
    return -1;
}

int mpu401_uart_init(const pcmcia_bus_t *bus, const scp55_config_t *config)
{
    uint8_t byte, status;
    if (!bus || !config || !bus->io_read || !bus->io_write || !bus->delay_ms) return -1;
    if (!wait_status(bus, config->io_base, 0x40, 0))
        (void)bus->io_write(bus->context, config->io_base + 1u, 0xff);
    bus->delay_ms(bus->context, 40);
    for (unsigned i = 0; i < 16; ++i) {
        if (bus->io_read(bus->context, config->io_base + 1u, &status)) return -2;
        if (status & 0x80) break;
        if (bus->io_read(bus->context, config->io_base, &byte)) return -2;
    }
    if (wait_status(bus, config->io_base, 0x40, 0) ||
        bus->io_write(bus->context, config->io_base + 1u, 0x3f) ||
        wait_status(bus, config->io_base, 0x80, 0) ||
        bus->io_read(bus->context, config->io_base, &byte)) return -3;
    return byte == 0xfe ? 0 : -4;
}

int mpu401_send(const pcmcia_bus_t *bus, const scp55_config_t *config, uint8_t byte)
{
    if (!bus || !config || !bus->io_read || !bus->io_write || !bus->delay_ms) return -1;
    if (wait_status(bus, config->io_base, 0x40, 0)) return -2;
    return bus->io_write(bus->context, config->io_base, byte);
}
