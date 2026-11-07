#include "card_power.h"

#include <stddef.h>

void pcmcia_power_stop(pcmcia_power_t *power)
{
    if (!power) return;
    power->running = false;
    power->pins.bus_translators_enable(power->pins.context, false);
    power->pins.master_clear_n(power->pins.context, false);
    pcmcia_bitbang_disarm(power->bus_driver);
    power->pins.reset_state(power->pins.context, PCMCIA_RESET_HIGH_Z);
    power->pins.card_power_enable(power->pins.context, false);
}

int pcmcia_power_init(pcmcia_power_t *power, const pcmcia_power_pins_t *pins,
                      pcmcia_bitbang_t *bus_driver)
{
    if (!power || !pins || !bus_driver || !pins->card_present ||
        !pins->card_5v_compatible || !pins->card_power_good ||
        !pins->master_clear_n || !pins->card_power_enable ||
        !pins->card_power_permitted ||
        !pins->bus_translators_enable || !pins->bus_translators_permitted ||
        !pins->reset_delay_complete || !pins->reset_state ||
        !pins->reset_translator_permitted || !pins->delay_ms) return -1;
    power->pins = *pins;
    power->bus_driver = bus_driver;
    power->running = false;
    pcmcia_power_stop(power);
    return 0;
}

int pcmcia_power_start(pcmcia_power_t *power)
{
    if (!power || power->running) return -1;
    const pcmcia_power_pins_t *p = &power->pins;
    pcmcia_power_stop(power);
    /* Do not restart into a charged socket after a prior fault/removal. */
    for (unsigned i = 0; p->card_power_good(p->context); ++i) {
        if (i >= 500) return -8;
        p->delay_ms(p->context, 1);
    }
    if (!p->card_present(p->context) || !p->card_5v_compatible(p->context))
        return -2;
    p->master_clear_n(p->context, true);
    /* Clear release must precede the new clock edge. */
    p->delay_ms(p->context, 1);
    p->card_power_enable(p->context, true);
    if (!p->card_power_permitted(p->context)) {
        pcmcia_power_stop(power);
        return -9;
    }
    for (unsigned i = 0; i < 500; ++i) {
        if (!p->card_present(p->context) || !p->card_5v_compatible(p->context)) {
            pcmcia_power_stop(power);
            return -3;
        }
        if (p->card_power_good(p->context)) break;
        p->delay_ms(p->context, 1);
        if (i == 499) {
            pcmcia_power_stop(power);
            return -4;
        }
    }
    /* Board callback confirms a continuous supply settling interval. */
    bool reset_delay_ready = false;
    for (unsigned i = 0; i < 100; ++i) {
        if (!p->card_present(p->context) || !p->card_power_good(p->context) ||
            !p->card_power_permitted(p->context)) {
            pcmcia_power_stop(power);
            return -5;
        }
        if (p->reset_delay_complete(p->context)) {
            reset_delay_ready = true;
            break;
        }
        p->delay_ms(p->context, 1);
    }
    if (!reset_delay_ready) {
        pcmcia_power_stop(power);
        return -11;
    }
    if (!p->card_present(p->context) || !p->card_power_good(p->context)) {
        pcmcia_power_stop(power);
        return -5;
    }
    /* RESET shares U4 with the control bus. Controls are already idle,
     * DATA_ENABLE remains low, and RESET is preloaded before enabling U4. */
    p->reset_state(p->context, PCMCIA_RESET_ASSERT_HIGH);
    p->bus_translators_enable(p->context, true);
    if (!p->bus_translators_permitted(p->context)) {
        pcmcia_power_stop(power);
        return -10;
    }
    if (!p->reset_translator_permitted(p->context)) {
        pcmcia_power_stop(power);
        return -12;
    }
    p->delay_ms(p->context, 20);
    p->reset_state(p->context, PCMCIA_RESET_RELEASE_LOW);
    if (!p->reset_translator_permitted(p->context)) {
        pcmcia_power_stop(power);
        return -12;
    }
    p->delay_ms(p->context, 20);
    if (!p->card_present(p->context) || !p->card_power_good(p->context)) {
        pcmcia_power_stop(power);
        return -6;
    }
    if (pcmcia_bitbang_arm(power->bus_driver)) {
        pcmcia_power_stop(power);
        return -7;
    }
    power->running = true;
    return 0;
}

int pcmcia_power_poll(pcmcia_power_t *power)
{
    if (!power || !power->running) return -1;
    const pcmcia_power_pins_t *p = &power->pins;
    if (!p->card_present(p->context) || !p->card_5v_compatible(p->context) ||
        !p->card_power_good(p->context) ||
        !p->card_power_permitted(p->context) ||
        !p->reset_delay_complete(p->context) ||
        !p->reset_translator_permitted(p->context) ||
        !p->bus_translators_permitted(p->context)) {
        pcmcia_power_stop(power);
        return -1;
    }
    return 0;
}
