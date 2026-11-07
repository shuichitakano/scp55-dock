#ifndef SCP55_RP2354B_DIAG_BOARD_H
#define SCP55_RP2354B_DIAG_BOARD_H

#include "card_power.h"
#include "diagnostic.h"
#include "scp55.h"

void scp55_board_safe_init(void);
scp55_diag_sample_t scp55_board_sample(void);

#ifdef SCP55_ENABLE_CIS_PROBE
/* Run only after the documented no-card electrical checks. Always powers down. */
int scp55_board_read_cis(scp55_config_t *config);
#endif

#ifdef SCP55_ENABLE_NOTE_TEST
/* Initialize the identified card and play MIDI note 60 for 300 ms.
 * Always powers down, including on initialization or note-off failure. */
int scp55_board_note_test(scp55_config_t *config);
#endif

#ifdef SCP55_ENABLE_AUDIO_PROBE
int scp55_board_audio_probe(scp55_config_t *config, int source);
#endif

#endif
