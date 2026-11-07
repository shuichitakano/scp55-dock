#include "bringup.h"

#include "cs4231.h"
#include "mpu401.h"

int scp55_bringup(const pcmcia_bus_t *bus, scp55_config_t *config)
{
    if (!bus || !config) return -1;
    if (scp55_wait_cis_stable(bus)) return -6;
    if (scp55_identify(bus, config)) return -2;
    if (scp55_set_cor(bus, config)) return -3;
    if (cs4231_unmute(bus, config)) return -4;
    if (mpu401_uart_init(bus, config)) return -5;
    return 0;
}

int scp55_note_test(const pcmcia_bus_t *bus, const scp55_config_t *config)
{
    static const uint8_t on[] = {0x90, 60, 64};
    static const uint8_t off[] = {0x80, 60, 0};
    if (!bus || !config || !bus->delay_ms) return -1;
    for (unsigned i = 0; i < sizeof on; ++i)
        if (mpu401_send(bus, config, on[i])) return -2;
    bus->delay_ms(bus->context, 300);
    for (unsigned i = 0; i < sizeof off; ++i)
        if (mpu401_send(bus, config, off[i])) return -3;
    return 0;
}
