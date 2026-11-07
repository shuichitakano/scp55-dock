#include "voltage_guard.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    assert(!scp55_voltage_good(5000, 0, 100));
    assert(!scp55_voltage_good(5000, 3300, 0));
    assert(!scp55_voltage_good(5000, 3000, 100));
    assert(!scp55_voltage_good(5000, 3300, 201));
    assert(!scp55_voltage_good(4849, 3300, 100));
    assert(scp55_voltage_good(4850, 3300, 100));
    assert(scp55_voltage_good(5150, 3300, 100));
    assert(!scp55_voltage_good(5151, 3300, 100));
    assert(!scp55_voltage_good(4650, 3300, 100));
    assert(!scp55_voltage_good(5350, 3300, 100));
    for (unsigned mv=0; mv<6600; ++mv)
        for (unsigned err=25; err<=200; ++err)
            if (scp55_voltage_good(mv, 3300, err)) {
                assert(mv-err >= 4750);
                assert(mv+err <= 5250);
            }
    assert(scp55_voltage_convert(2048, 3300) == 3300);
    assert(scp55_voltage_convert(2048, 3270) == 3270);
    assert(scp55_voltage_convert(4096, 3300) == UINT16_MAX);
    assert(!scp55_data_dir_level(true));
    assert(!scp55_voltage_probe_ready(3320, 0, false));
    assert(scp55_voltage_probe_ready(3320, 0, true));
    assert(!scp55_voltage_probe_ready(0, 0, true));
    assert(!scp55_voltage_probe_ready(3320, 201, true));
    assert(!scp55_voltage_probe_ready(3320, 1, true));
    for (unsigned mv = 0; mv < 7000; ++mv) {
        assert(scp55_voltage_probe_good(mv, 3320, 0, true) ==
               (mv >= 4750 && mv <= 5250));
        assert(!scp55_voltage_probe_good(mv, 3320, 0, false));
        assert(!scp55_voltage_probe_good(mv, 0, 0, true));
        assert(scp55_voltage_probe_good(mv, 3320, 100, true) ==
               scp55_voltage_good(mv, 3320, 100));
    }
    assert(scp55_data_dir_level(false));
    puts("Voltage uncertainty bounds and Nexperia DIR polarity passed");
}
