#include "voltage_guard.h"
bool scp55_voltage_calibrated(uint16_t reference_mv, uint16_t error_mv)
{
    return reference_mv >= 3100 && reference_mv <= 3500 &&
           error_mv >= 25 && error_mv <= 200;
}
uint16_t scp55_voltage_convert(uint16_t raw, uint16_t reference_mv)
{
    if (raw > 4095) return UINT16_MAX;
    uint32_t ref = reference_mv ? reference_mv : 3300u;
    return (uint16_t)(((uint32_t)raw * ref * 2u + 2048u) / 4096u);
}
bool scp55_voltage_good(uint16_t mv, uint16_t reference_mv, uint16_t error_mv)
{
    return scp55_voltage_calibrated(reference_mv, error_mv) &&
           mv >= 4750u + error_mv && mv <= 5250u - error_mv;
}
bool scp55_voltage_probe_ready(uint16_t reference_mv, uint16_t error_mv,
                               bool allow_uncompensated)
{
    return scp55_voltage_calibrated(reference_mv, error_mv) ||
           (allow_uncompensated && error_mv == 0 &&
            reference_mv >= 3100 && reference_mv <= 3500);
}
bool scp55_voltage_probe_good(uint16_t mv, uint16_t reference_mv,
                              uint16_t error_mv, bool allow_uncompensated)
{
    if (error_mv != 0 || !allow_uncompensated)
        return scp55_voltage_good(mv, reference_mv, error_mv);
    return scp55_voltage_probe_ready(reference_mv, error_mv, true) &&
           mv >= 4750u && mv <= 5250u;
}
