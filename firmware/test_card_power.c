#include "card_power.h"

#include <assert.h>

typedef struct {
    bool present;
    bool compatible;
    bool power_on;
    bool master_clear_high;
    bool translator_on;
    bool data_translator_on;
    bool power_permission_allowed;
    bool bus_permission_allowed;
    bool reset_permission_allowed;
    bool reset_delay_stuck;
    uint32_t elapsed_ms;
    uint32_t power_on_at_ms;
    uint32_t reset_asserted_at_ms;
    pcmcia_reset_state_t reset;
} mock_board_t;

static void set_line(void *context, pcmcia_line_t line, bool high)
{ (void)context; (void)line; (void)high; }
static void set_address(void *context, uint16_t address)
{ (void)context; (void)address; }
static void set_data_output(void *context, bool output)
{ (void)context; (void)output; }
static void write_data(void *context, uint8_t data)
{ (void)context; (void)data; }
static uint8_t read_data(void *context)
{ (void)context; return 0; }
static void set_data_to_card(void *context, bool to_card)
{ (void)context; (void)to_card; }
static void set_data_translator(void *context, bool enabled)
{ ((mock_board_t *)context)->data_translator_on = enabled; }
static bool wait_n(void *context)
{ (void)context; return true; }
static bool card_present(void *context)
{ return ((mock_board_t *)context)->present; }
static bool card_power_good(void *context)
{
    mock_board_t *mock = context;
    return mock->power_on && mock->elapsed_ms - mock->power_on_at_ms >= 5;
}
static void delay_us(void *context, uint32_t us)
{ (void)context; (void)us; }
static void delay_ms(void *context, uint32_t ms)
{ ((mock_board_t *)context)->elapsed_ms += ms; }
static bool card_5v_compatible(void *context)
{ return ((mock_board_t *)context)->compatible; }
static void master_clear_n(void *context, bool high)
{ ((mock_board_t *)context)->master_clear_high = high; }
static void card_power_enable(void *context, bool enabled)
{
    mock_board_t *mock = context;
    if (enabled && !mock->power_on) mock->power_on_at_ms = mock->elapsed_ms;
    mock->power_on = enabled && mock->master_clear_high;
}
static bool card_power_permitted(void *context)
{
    mock_board_t *mock = context;
    return mock->power_on && mock->power_permission_allowed;
}
static void bus_translators_enable(void *context, bool enabled)
{ ((mock_board_t *)context)->translator_on = enabled; }
static bool bus_translators_permitted(void *context)
{
    mock_board_t *mock = context;
    return mock->translator_on && mock->bus_permission_allowed;
}
static void reset_state(void *context, pcmcia_reset_state_t state)
{
    mock_board_t *mock = context;
    mock->reset = state;
    if (state == PCMCIA_RESET_ASSERT_HIGH)
        mock->reset_asserted_at_ms = mock->elapsed_ms;
}
static bool reset_delay_complete(void *context)
{
    mock_board_t *mock = context;
    return !mock->reset_delay_stuck && mock->power_on &&
           mock->elapsed_ms - mock->power_on_at_ms >= 17;
}
static bool reset_translator_permitted(void *context)
{
    mock_board_t *mock = context;
    return mock->reset != PCMCIA_RESET_HIGH_Z &&
           mock->reset_permission_allowed && mock->translator_on && reset_delay_complete(context);
}

int main(void)
{
    mock_board_t mock = {.present = true, .compatible = true,
                         .power_permission_allowed = true,
                         .bus_permission_allowed = true,
                         .reset_permission_allowed = true};
    pcmcia_pins_t bus_pins = {
        .context = &mock, .set_line = set_line, .set_address = set_address,
        .set_data_output = set_data_output, .write_data = write_data,
        .read_data = read_data, .set_data_to_card = set_data_to_card,
        .set_data_translator = set_data_translator, .wait_n = wait_n,
        .card_present = card_present, .card_power_good = card_power_good,
        .delay_us = delay_us, .delay_ms = delay_ms
    };
    pcmcia_power_pins_t power_pins = {
        .context = &mock, .card_present = card_present,
        .card_5v_compatible = card_5v_compatible,
        .card_power_good = card_power_good,
        .master_clear_n = master_clear_n,
        .card_power_enable = card_power_enable,
        .card_power_permitted = card_power_permitted,
        .bus_translators_enable = bus_translators_enable,
        .bus_translators_permitted = bus_translators_permitted,
        .reset_delay_complete = reset_delay_complete,
        .reset_translator_permitted = reset_translator_permitted,
        .reset_state = reset_state, .delay_ms = delay_ms
    };
    pcmcia_bitbang_t bus;
    pcmcia_power_t power;
    assert(pcmcia_bitbang_init(&bus, &bus_pins) == 0);
    assert(pcmcia_power_init(&power, &power_pins, &bus) == 0);
    assert(!mock.power_on && !mock.master_clear_high && !mock.translator_on &&
           mock.reset == PCMCIA_RESET_HIGH_Z);
    assert(pcmcia_power_start(&power) == 0);
    assert(mock.reset_asserted_at_ms >= 17);
    assert(mock.power_on && mock.translator_on && bus.armed);
    assert(mock.reset == PCMCIA_RESET_RELEASE_LOW);
    mock.present = false;
    assert(pcmcia_power_poll(&power) < 0);
    assert(!mock.power_on && !mock.master_clear_high &&
           !mock.translator_on && !bus.armed);
    mock.present = true;
    mock.compatible = false;
    assert(pcmcia_power_start(&power) < 0);
    assert(!mock.power_on);
    mock.compatible = true;
    mock.power_permission_allowed = false;
    assert(pcmcia_power_start(&power) == -9);
    assert(!mock.power_on && !mock.translator_on && !bus.armed);
    mock.power_permission_allowed = true;
    mock.bus_permission_allowed = false;
    assert(pcmcia_power_start(&power) == -10);
    assert(!mock.power_on && !mock.translator_on && !bus.armed);
    mock.bus_permission_allowed = true;
    mock.reset_permission_allowed = false;
    assert(pcmcia_power_start(&power) == -12);
    assert(!mock.power_on && mock.reset == PCMCIA_RESET_HIGH_Z);
    mock.reset_permission_allowed = true;
    mock.reset_delay_stuck = true;
    assert(pcmcia_power_start(&power) == -11);
    assert(!mock.power_on && mock.reset == PCMCIA_RESET_HIGH_Z);
    return 0;
}
