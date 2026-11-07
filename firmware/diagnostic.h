#ifndef SCP55_DIAGNOSTIC_H
#define SCP55_DIAGNOSTIC_H

#include <stdbool.h>
#include <stdint.h>

/* All values are the 3V3 MCU-side levels, after the external input buffers. */
typedef struct {
    bool cd1_high;
    bool cd2_high;
    bool power_en;
    bool bus_enable;
    uint16_t card_mv;
} scp55_diag_sample_t;

enum {
    SCP55_DIAG_CD_MISMATCH = 1u << 0,
    SCP55_DIAG_POWER_WHILE_INHIBITED = 1u << 2,
    SCP55_DIAG_BUS_WHILE_INHIBITED = 1u << 3,
    SCP55_DIAG_UNSTABLE = 1u << 5
};

/* This diagnostic never requests a power or bus grant. Any asserted grant is
 * therefore a fault, even if a card is present. */
uint32_t scp55_diag_evaluate(const scp55_diag_sample_t *sample);
bool scp55_diag_card_present(const scp55_diag_sample_t *sample);
bool scp55_diag_same(const scp55_diag_sample_t *a,
                     const scp55_diag_sample_t *b);

#endif
