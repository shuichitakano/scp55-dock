#ifndef SCP55_POWER_BUTTON_H
#define SCP55_POWER_BUTTON_H
#include <stdbool.h>
#include <stdint.h>
typedef struct {
    bool on, raw, stable, consumed;
    uint64_t changed_ms, pressed_ms;
} scp55_power_button_t;
void scp55_power_button_init(scp55_power_button_t *s);
void scp55_power_button_off(scp55_power_button_t *s);
bool scp55_power_button_update(scp55_power_button_t *s, bool pressed, uint64_t now_ms);
#endif
