#include "audio_ring.h"

#include <stddef.h>

void scp55_audio_ring_init(scp55_audio_ring_t *ring)
{
    if (!ring) return;
    atomic_store(&ring->head, 0);
    atomic_store(&ring->tail, 0);
    atomic_store(&ring->overrun_frames, 0);
    atomic_store(&ring->underrun_frames, 0);
}

int scp55_audio_ring_push(scp55_audio_ring_t *ring, scp55_audio_frame_t frame)
{
    if (!ring) return -1;
    uint32_t head = atomic_load_explicit(&ring->head, memory_order_relaxed);
    uint32_t tail = atomic_load_explicit(&ring->tail, memory_order_acquire);
    if (head - tail >= SCP55_AUDIO_RING_FRAMES) {
        atomic_fetch_add_explicit(&ring->overrun_frames, 1, memory_order_relaxed);
        return -1;
    }
    ring->frames[head % SCP55_AUDIO_RING_FRAMES] = frame;
    atomic_store_explicit(&ring->head, head + 1, memory_order_release);
    return 0;
}

int scp55_audio_ring_push_u8_mono(scp55_audio_ring_t *ring, uint8_t sample)
{
    int16_t amplitude = (int16_t)(((int)sample - 128) * 256);
    scp55_audio_frame_t frame = {amplitude, amplitude};
    return scp55_audio_ring_push(ring, frame);
}

int scp55_audio_ring_pop(scp55_audio_ring_t *ring, scp55_audio_frame_t *frame)
{
    if (!ring || !frame) return -1;
    uint32_t tail = atomic_load_explicit(&ring->tail, memory_order_relaxed);
    uint32_t head = atomic_load_explicit(&ring->head, memory_order_acquire);
    if (head == tail) {
        frame->left = frame->right = 0;
        atomic_fetch_add_explicit(&ring->underrun_frames, 1, memory_order_relaxed);
        return -1;
    }
    *frame = ring->frames[tail % SCP55_AUDIO_RING_FRAMES];
    atomic_store_explicit(&ring->tail, tail + 1, memory_order_release);
    return 0;
}

uint32_t scp55_audio_ring_count(const scp55_audio_ring_t *ring)
{
    if (!ring) return 0;
    uint32_t head = atomic_load_explicit(&ring->head, memory_order_acquire);
    uint32_t tail = atomic_load_explicit(&ring->tail, memory_order_acquire);
    return head - tail;
}
