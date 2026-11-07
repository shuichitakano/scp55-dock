#include "audio_ring.h"

#include <assert.h>

int main(void)
{
    scp55_audio_ring_t ring;
    scp55_audio_frame_t frame;
    scp55_audio_ring_init(&ring);
    assert(scp55_audio_ring_push_u8_mono(&ring, 0) == 0);
    assert(scp55_audio_ring_push_u8_mono(&ring, 128) == 0);
    assert(scp55_audio_ring_count(&ring) == 2);
    assert(scp55_audio_ring_pop(&ring, &frame) == 0);
    assert(frame.left == -32768 && frame.right == -32768);
    assert(scp55_audio_ring_pop(&ring, &frame) == 0);
    assert(frame.left == 0 && frame.right == 0);
    assert(scp55_audio_ring_pop(&ring, &frame) < 0);
    assert(frame.left == 0 && atomic_load(&ring.underrun_frames) == 1);
    for (unsigned i = 0; i < SCP55_AUDIO_RING_FRAMES; ++i)
        assert(scp55_audio_ring_push_u8_mono(&ring, 255) == 0);
    assert(scp55_audio_ring_push_u8_mono(&ring, 255) < 0);
    assert(atomic_load(&ring.overrun_frames) == 1);
    assert(scp55_audio_ring_count(&ring) == SCP55_AUDIO_RING_FRAMES);
    return 0;
}
