#ifndef SCP55_VOLTAGE_GUARD_H
#define SCP55_VOLTAGE_GUARD_H
#include <stdbool.h>
#include <stdint.h>
/* measured reference and verified total card-voltage error, including ADC,
 * divider, reference drift and meter error; zero means not calibrated. */
bool scp55_voltage_calibrated(uint16_t reference_mv, uint16_t error_mv);
uint16_t scp55_voltage_convert(uint16_t raw, uint16_t reference_mv);
bool scp55_voltage_good(uint16_t mv, uint16_t reference_mv, uint16_t error_mv);
/* Explicit bench mode: measured reference, no claimed error bound. Does not
 * change the calibrated policy. Only error_mv == 0 enables this alternative. */
bool scp55_voltage_probe_ready(uint16_t reference_mv, uint16_t error_mv,
                               bool allow_uncompensated);
bool scp55_voltage_probe_good(uint16_t mv, uint16_t reference_mv,
                              uint16_t error_mv, bool allow_uncompensated);
/* Nexperia 74LVC4245A: A=card, B=MCU; DIR high reads from card. */
static inline bool scp55_data_dir_level(bool to_card) { return !to_card; }
#endif
