#ifndef SCP55_MPU401_H
#define SCP55_MPU401_H

#include "scp55.h"

int mpu401_uart_init(const pcmcia_bus_t *bus, const scp55_config_t *config);
int mpu401_send(const pcmcia_bus_t *bus, const scp55_config_t *config, uint8_t byte);
/* 0=sent, 1=busy (retry later), negative=error; never waits. */
int mpu401_try_send(const pcmcia_bus_t *bus, const scp55_config_t *config, uint8_t byte);

#endif
