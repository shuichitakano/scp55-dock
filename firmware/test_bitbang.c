#include "pcmcia_bitbang.h"

#include <assert.h>
#include <stdint.h>

typedef struct {
    bool lines[7];
    bool translator;
    bool output;
    bool present;
    bool power_good;
    bool stall_wait;
    unsigned wait_polls;
    unsigned direction_changes_with_translator_on;
    uint16_t address;
    uint8_t data;
    uint32_t elapsed_us;
} mock_pins_t;

static void set_line(void *context, pcmcia_line_t line, bool high)
{
    mock_pins_t *mock = context;
    mock->lines[line] = high;
}

static void set_address(void *context, uint16_t address)
{
    mock_pins_t *mock = context;
    mock->address = address;
}

static void set_data_output(void *context, bool output)
{
    mock_pins_t *mock = context;
    if (mock->translator) ++mock->direction_changes_with_translator_on;
    mock->output = output;
}

static void write_data(void *context, uint8_t data)
{
    mock_pins_t *mock = context;
    mock->data = data;
}

static uint8_t read_data(void *context)
{
    mock_pins_t *mock = context;
    return mock->data;
}

static void set_data_to_card(void *context, bool to_card)
{
    mock_pins_t *mock = context;
    if (mock->translator) ++mock->direction_changes_with_translator_on;
    (void)to_card;
}

static void set_data_translator(void *context, bool enabled)
{
    mock_pins_t *mock = context;
    if (enabled) assert(mock->present && mock->power_good);
    mock->translator = enabled;
}

static bool wait_n(void *context)
{
    mock_pins_t *mock = context;
    ++mock->wait_polls;
    return !mock->stall_wait && mock->wait_polls >= 3;
}

static bool card_present(void *context)
{
    mock_pins_t *mock = context;
    return mock->present;
}

static bool card_power_good(void *context)
{
    mock_pins_t *mock = context;
    return mock->power_good;
}

static void delay_us(void *context, uint32_t microseconds)
{
    mock_pins_t *mock = context;
    mock->elapsed_us += microseconds;
}

static void delay_ms(void *context, uint32_t milliseconds)
{
    mock_pins_t *mock = context;
    mock->elapsed_us += milliseconds * 1000u;
}

static void assert_idle(const mock_pins_t *mock)
{
    for (unsigned i = 0; i < 7; ++i) assert(mock->lines[i]);
    assert(!mock->translator && !mock->output);
}

int main(void)
{
    mock_pins_t mock = {.present = true, .power_good = true, .data = 0x5a};
    pcmcia_pins_t pins = {
        .context = &mock, .set_line = set_line, .set_address = set_address,
        .set_data_output = set_data_output, .write_data = write_data,
        .read_data = read_data, .set_data_to_card = set_data_to_card,
        .set_data_translator = set_data_translator, .wait_n = wait_n,
        .card_present = card_present, .card_power_good = card_power_good,
        .delay_us = delay_us, .delay_ms = delay_ms
    };
    pcmcia_bitbang_t driver;
    assert(pcmcia_bitbang_init(&driver, &pins) == 0);
    assert_idle(&mock);
    assert(pcmcia_bitbang_arm(&driver) == 0);
    pcmcia_bus_t bus = pcmcia_bitbang_bus(&driver);
    uint8_t value = 0;
    assert(bus.attr_read(bus.context, 0x400, &value) == 0);
    assert(value == 0x5a && mock.address == 0x400 && mock.wait_polls == 3);
    assert_idle(&mock);
    assert(bus.attr_read(bus.context, 0x401, &value) == -3);
    assert_idle(&mock);
    mock.wait_polls = 0;
    assert(bus.io_write(bus.context, 0x330, 0x90) == 0);
    assert(mock.address == 0x330 && mock.data == 0x90);
    assert_idle(&mock);
    mock.stall_wait = true;
    assert(bus.io_read(bus.context, 0x331, &value) < 0);
    assert_idle(&mock);
    assert(mock.direction_changes_with_translator_on == 0);
    pcmcia_bitbang_disarm(&driver);
    mock.present = false;
    assert(pcmcia_bitbang_arm(&driver) != 0);
    return 0;
}
