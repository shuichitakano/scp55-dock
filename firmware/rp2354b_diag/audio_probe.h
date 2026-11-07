#ifndef SCP55_AUDIO_PROBE_H
#define SCP55_AUDIO_PROBE_H
#include "scp55.h"
void scp55_audio_probe_clear(void);
int scp55_audio_probe(const pcmcia_bus_t *bus, const scp55_config_t *config, int source);
void scp55_audio_probe_dump(void);
int scp55_audio_probe_play(void);
#endif
