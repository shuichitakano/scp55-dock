#ifndef SCP55_AUDIO_LIVE_H
#define SCP55_AUDIO_LIVE_H
#include "scp55.h"
#include "card_power.h"
int scp55_audio_live_run(const pcmcia_bus_t *bus, const scp55_config_t *config,
                         pcmcia_power_t *power, bool demo, unsigned seconds);
void scp55_audio_live_dump(void);
#endif
