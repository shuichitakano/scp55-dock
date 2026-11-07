#ifndef SCP55_PCMCIA_BITBANG_H
#define SCP55_PCMCIA_BITBANG_H

#include "pcmcia_bus.h"

#include <stdbool.h>
#include <stdint.h>

/* These are logic levels at the MCU side of the board's translators.
 * Hardware pulls and power/card-detect gating remain mandatory. */
typedef enum {
    PCMCIA_CE1_N,
    PCMCIA_CE2_N,
    PCMCIA_REG_N,
    PCMCIA_OE_N,
    PCMCIA_WE_N,
    PCMCIA_IORD_N,
    PCMCIA_IOWR_N
} pcmcia_line_t;

typedef struct {
    void *context;
    void (*set_line)(void *context, pcmcia_line_t line, bool high);
    void (*set_address)(void *context, uint16_t address);
    void (*set_data_output)(void *context, bool output);
    void (*write_data)(void *context, uint8_t data);
    uint8_t (*read_data)(void *context);
    void (*set_data_to_card)(void *context, bool to_card);
    void (*set_data_translator)(void *context, bool enabled);
    bool (*wait_n)(void *context);
    bool (*card_present)(void *context);
    bool (*card_power_good)(void *context);
    void (*delay_us)(void *context, uint32_t microseconds);
    void (*delay_ms)(void *context, uint32_t milliseconds);
} pcmcia_pins_t;

typedef struct {
    pcmcia_pins_t pins;
    bool armed;
} pcmcia_bitbang_t;

/* Sets inactive strobes and disables the DATA translator. Does not power card. */
int pcmcia_bitbang_init(pcmcia_bitbang_t *driver, const pcmcia_pins_t *pins);

/* Board power/reset state machine calls arm only after interlocks pass. */
int pcmcia_bitbang_arm(pcmcia_bitbang_t *driver);
void pcmcia_bitbang_disarm(pcmcia_bitbang_t *driver);
pcmcia_bus_t pcmcia_bitbang_bus(pcmcia_bitbang_t *driver);

#endif
