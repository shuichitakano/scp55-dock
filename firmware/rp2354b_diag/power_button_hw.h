#ifndef SCP55_POWER_BUTTON_HW_H
#define SCP55_POWER_BUTTON_HW_H
#include <stdbool.h>
void scp55_power_button_hw_init(void);
bool scp55_power_button_service(void);
void scp55_power_button_standby(void);
#endif
