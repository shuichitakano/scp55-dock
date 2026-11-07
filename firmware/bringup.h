#ifndef SCP55_BRINGUP_H
#define SCP55_BRINGUP_H

#include "scp55.h"

/* Physical bus initialization and power safety checks precede this call. */
int scp55_bringup(const pcmcia_bus_t *bus, scp55_config_t *config);
int scp55_note_test(const pcmcia_bus_t *bus, const scp55_config_t *config);

#endif
