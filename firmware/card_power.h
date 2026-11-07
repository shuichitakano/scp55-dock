#ifndef SCP55_CARD_POWER_H
#define SCP55_CARD_POWER_H

#include "pcmcia_bitbang.h"

typedef enum {
    PCMCIA_RESET_HIGH_Z,
    PCMCIA_RESET_ASSERT_HIGH,
    PCMCIA_RESET_RELEASE_LOW
} pcmcia_reset_state_t;

typedef struct {
    void *context;
    bool (*card_present)(void *context);
    bool (*card_5v_compatible)(void *context);
    bool (*card_power_good)(void *context);
    /* Shared asynchronous clear: low removes both stored power/bus grants. */
    void (*master_clear_n)(void *context, bool high);
    void (*card_power_enable)(void *context, bool enabled);
    /* Read the physical interlock Q/EN result, not the MCU request pin. */
    bool (*card_power_permitted)(void *context);
    void (*bus_translators_enable)(void *context, bool enabled);
    /* Read the physical bus latch result, not the MCU request pin. */
    bool (*bus_translators_permitted)(void *context);
    /* Hardware timer released only after card VCC has been valid long enough. */
    bool (*reset_delay_complete)(void *context);
    void (*reset_state)(void *context, pcmcia_reset_state_t state);
    /* Read the independent RESET translator permission latch. */
    bool (*reset_translator_permitted)(void *context);
    void (*delay_ms)(void *context, uint32_t milliseconds);
} pcmcia_power_pins_t;

typedef struct {
    pcmcia_power_pins_t pins;
    pcmcia_bitbang_t *bus_driver;
    bool running;
} pcmcia_power_t;

/* Physical hardware must also gate translators/eFuse on card detect and
 * power-good, even if firmware stops executing. */
int pcmcia_power_init(pcmcia_power_t *power, const pcmcia_power_pins_t *pins,
                      pcmcia_bitbang_t *bus_driver);
int pcmcia_power_start(pcmcia_power_t *power);
void pcmcia_power_stop(pcmcia_power_t *power);
/* Call frequently from main loop; returns -1 if removal/power fault stopped it. */
int pcmcia_power_poll(pcmcia_power_t *power);

#endif
