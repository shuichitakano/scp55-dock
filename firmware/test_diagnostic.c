#include "diagnostic.h"
#include <assert.h>
int main(void)
{
    scp55_diag_sample_t s = {.cd1_high = true, .cd2_high = true};
    assert(!scp55_diag_card_present(&s));
    assert(scp55_diag_evaluate(&s) == 0);
    s.cd1_high = false;
    assert(scp55_diag_evaluate(&s) == SCP55_DIAG_CD_MISMATCH);
    s.cd2_high = false;
    assert(scp55_diag_card_present(&s));
    assert(scp55_diag_evaluate(&s) == 0);
    s.power_en = true;
    assert(scp55_diag_evaluate(&s) == SCP55_DIAG_POWER_WHILE_INHIBITED);
    s.power_en = false; s.bus_enable = true;
    assert(scp55_diag_evaluate(&s) == SCP55_DIAG_BUS_WHILE_INHIBITED);
    assert(scp55_diag_same(&s, &s));
    scp55_diag_sample_t t = s; t.cd1_high = true;
    assert(!scp55_diag_same(&s, &t));
    assert(!scp55_diag_same(0, &t));
    return 0;
}
