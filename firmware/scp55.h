#ifndef SCP55_H
#define SCP55_H

#include "pcmcia_bus.h"

typedef struct {
    uint16_t cor_address;
    uint16_t io_base;
    uint8_t config_index;
} scp55_config_t;

/* Identity is required before configuration. Returns 0 on success. */
int scp55_identify(const pcmcia_bus_t *bus, scp55_config_t *config);
/* Wait for four stable CIS bytes; READY alone is insufficient on cold boot. */
int scp55_wait_cis_stable(const pcmcia_bus_t *bus);
int scp55_set_cor(const pcmcia_bus_t *bus, const scp55_config_t *config);

#endif
