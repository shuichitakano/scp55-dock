#include "pcmcia_bitbang.h"

#include <stddef.h>

static void lines_idle(pcmcia_bitbang_t *driver)
{
    const pcmcia_pins_t *p = &driver->pins;
    p->set_line(p->context, PCMCIA_OE_N, true);
    p->set_line(p->context, PCMCIA_WE_N, true);
    p->set_line(p->context, PCMCIA_IORD_N, true);
    p->set_line(p->context, PCMCIA_IOWR_N, true);
    p->set_line(p->context, PCMCIA_CE1_N, true);
    p->set_line(p->context, PCMCIA_CE2_N, true);
    p->set_line(p->context, PCMCIA_REG_N, true);
}

int pcmcia_bitbang_init(pcmcia_bitbang_t *driver, const pcmcia_pins_t *pins)
{
    if (!driver || !pins || !pins->set_line || !pins->set_address ||
        !pins->set_data_output || !pins->write_data || !pins->read_data ||
        !pins->set_data_to_card || !pins->set_data_translator ||
        !pins->wait_n || !pins->card_present || !pins->card_power_good ||
        !pins->delay_us || !pins->delay_ms) return -1;
    driver->pins = *pins;
    driver->armed = false;
    pins->set_data_translator(pins->context, false);
    pins->set_data_output(pins->context, false);
    lines_idle(driver);
    pins->set_address(pins->context, 0);
    return 0;
}

int pcmcia_bitbang_arm(pcmcia_bitbang_t *driver)
{
    if (!driver || !driver->pins.card_present(driver->pins.context) ||
        !driver->pins.card_power_good(driver->pins.context)) return -1;
    driver->armed = true;
    return 0;
}

void pcmcia_bitbang_disarm(pcmcia_bitbang_t *driver)
{
    if (!driver) return;
    driver->armed = false;
    driver->pins.set_data_translator(driver->pins.context, false);
    lines_idle(driver);
    driver->pins.set_data_output(driver->pins.context, false);
}

static int cycle(pcmcia_bitbang_t *driver, uint16_t address,
                 bool attribute, bool write, uint8_t *value)
{
    const pcmcia_pins_t *p;
    pcmcia_line_t strobe;
    int result = -1;
    if (!driver || !driver->armed || !value) return -1;
    p = &driver->pins;
    if (!p->card_present(p->context) || !p->card_power_good(p->context)) {
        pcmcia_bitbang_disarm(driver);
        return -2;
    }
    if (attribute && (address & 1u)) return -3;

    /* D translator remains off throughout every direction change. */
    p->set_data_translator(p->context, false);
    p->set_data_output(p->context, write);
    p->set_data_to_card(p->context, write);
    if (write) p->write_data(p->context, *value);
    p->set_address(p->context, address);
    p->set_line(p->context, PCMCIA_REG_N, false);
    p->delay_us(p->context, 1);
    p->set_data_translator(p->context, true);
    p->set_line(p->context, PCMCIA_CE1_N, false);
    p->delay_us(p->context, 1);
    strobe = attribute ? (write ? PCMCIA_WE_N : PCMCIA_OE_N)
                       : (write ? PCMCIA_IOWR_N : PCMCIA_IORD_N);
    p->set_line(p->context, strobe, false);
    p->delay_us(p->context, 1);
    for (unsigned waited = 0; waited < 200; ++waited) {
        if (!p->card_present(p->context) || !p->card_power_good(p->context)) {
            result = -2;
            break;
        }
        if (p->wait_n(p->context)) {
            /* Keep the strobe asserted after WAIT releases. */
            p->delay_us(p->context, 1);
            if (!write) *value = p->read_data(p->context);
            result = 0;
            break;
        }
        p->delay_us(p->context, 1);
    }
    p->set_line(p->context, strobe, true);
    p->delay_us(p->context, 1); /* explicit data/address hold */
    p->set_line(p->context, PCMCIA_CE1_N, true);
    p->set_line(p->context, PCMCIA_REG_N, true);
    p->set_data_translator(p->context, false);
    p->set_data_output(p->context, false);
    if (result == -2) pcmcia_bitbang_disarm(driver);
    return result;
}

static int attr_read(void *context, uint16_t address, uint8_t *value)
{
    return cycle(context, address, true, false, value);
}

static int attr_write(void *context, uint16_t address, uint8_t value)
{
    return cycle(context, address, true, true, &value);
}

static int io_read(void *context, uint16_t address, uint8_t *value)
{
    return cycle(context, address, false, false, value);
}

static int io_write(void *context, uint16_t address, uint8_t value)
{
    return cycle(context, address, false, true, &value);
}

static void delay_ms(void *context, uint32_t milliseconds)
{
    pcmcia_bitbang_t *driver = context;
    driver->pins.delay_ms(driver->pins.context, milliseconds);
}

pcmcia_bus_t pcmcia_bitbang_bus(pcmcia_bitbang_t *driver)
{
    pcmcia_bus_t bus = {0};
    if (!driver) return bus;
    bus.context = driver;
    bus.attr_read = attr_read;
    bus.attr_write = attr_write;
    bus.io_read = io_read;
    bus.io_write = io_write;
    bus.delay_ms = delay_ms;
    return bus;
}
