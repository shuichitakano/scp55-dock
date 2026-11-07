#include "diagnostic.h"

bool scp55_diag_card_present(const scp55_diag_sample_t *sample)
{
    return sample && !sample->cd1_high && !sample->cd2_high;
}

bool scp55_diag_same(const scp55_diag_sample_t *a,
                     const scp55_diag_sample_t *b)
{
    return a && b &&
           a->cd1_high == b->cd1_high && a->cd2_high == b->cd2_high &&
           a->power_en == b->power_en &&
           a->bus_enable == b->bus_enable;
}

uint32_t scp55_diag_evaluate(const scp55_diag_sample_t *sample)
{
    if (!sample) return SCP55_DIAG_UNSTABLE;
    uint32_t faults = 0;
    if (sample->cd1_high != sample->cd2_high)
        faults |= SCP55_DIAG_CD_MISMATCH;
    /* MASTER_CLEAR_N is driven low throughout this diagnostic. */
    if (sample->power_en) faults |= SCP55_DIAG_POWER_WHILE_INHIBITED;
    if (sample->bus_enable) faults |= SCP55_DIAG_BUS_WHILE_INHIBITED;
    return faults;
}
