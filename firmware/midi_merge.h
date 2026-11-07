#ifndef SCP55_MIDI_MERGE_H
#define SCP55_MIDI_MERGE_H
#include <stdint.h>
#include <stdbool.h>
#define MIDI_MERGE_CAPACITY 1024u
typedef struct {
    uint8_t bytes[MIDI_MERGE_CAPACITY];
    unsigned head, tail, count;
    uint8_t running, message[3], used, length;
} midi_merge_input_t;
typedef struct {
    midi_merge_input_t input[2]; /* 0=DIN, 1=USB */
    uint8_t pending[3], pending_pos, pending_count, output_running;
    unsigned turn;
    int sysex_owner;
    uint64_t sysex_activity_us;
    uint32_t sysex_aborts;
    bool end_sysex;
} midi_merge_t;
void midi_merge_init(midi_merge_t *m);
unsigned midi_merge_space(const midi_merge_t *m, unsigned source);
/* Atomic enqueue: caller retains packet on insufficient space. */
int midi_merge_push(midi_merge_t *m, unsigned source, const uint8_t *bytes, unsigned count);
/* At most one output byte; status is restored when sources change. */
bool midi_merge_next(midi_merge_t *m, uint64_t now_us, uint8_t *byte);
void midi_merge_reset_input(midi_merge_t *m, unsigned source);
#endif
