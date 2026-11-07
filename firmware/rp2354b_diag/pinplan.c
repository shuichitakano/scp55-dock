#include "pinplan.h"

#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/uart.h"
#include "scp55_i2s.pio.h"
#include "pico/time.h"

#if !PICO_PIO_USE_GPIO_BASE
#error RP2354B I2S on GPIO42/43/46 requires high GPIO PIO support
#endif

enum { MIDI_ACTIVITY = 36, MIDI_RX = 41, I2S_BCLK = 42, I2S_LRCLK = 43, I2S_DOUT = 46 };
static PIO configured_pio;
static uint configured_sm, configured_offset;
static pio_sm_config configured_config;
static uint64_t midi_led_off_us;
_Static_assert(I2S_LRCLK == I2S_BCLK + 1, "PIO sideset clock pins must be adjacent");
_Static_assert(I2S_DOUT >= 16 && I2S_DOUT <= 47, "PIO high GPIO range");

bool scp55_pinplan_prepare(void)
{
    scp55_midi_led_init();
    PIO pio;
    uint sm, offset;
    if (!pio_claim_free_sm_and_add_program_for_gpio_range(
            &scp55_i2s_program, &pio, &sm, &offset,
            I2S_BCLK, I2S_DOUT - I2S_BCLK + 1, true)) return false;
    if (pio_get_gpio_base(pio) != 16) {
        pio_remove_program_and_unclaim_sm(&scp55_i2s_program, pio, sm, offset);
        return false;
    }

    uart_init(uart1, 31250); /* MIDI DIN: one RX pin; UART TX is unused. */
    uart_set_format(uart1, 8, 1, UART_PARITY_NONE);
    uart_set_hw_flow(uart1, false, false);
    configured_pio = pio;
    scp55_pinplan_restore_pins();
    pio_sm_config config = scp55_i2s_program_get_default_config(offset);
    sm_config_set_out_pins(&config, I2S_DOUT, 1);
    sm_config_set_sideset_pins(&config, I2S_BCLK);
    sm_config_set_out_shift(&config, false, true, 32);
    /* The PIO program takes exactly 64 cycles per stereo frame. */
    sm_config_set_clkdiv(&config, (float)clock_get_hz(clk_sys) / (44100.0f * 64.0f));
    configured_sm = sm;
    configured_offset = offset;
    configured_config = config;
    pio_sm_init(pio, sm, offset, &config);
    if (pio_sm_set_consecutive_pindirs(pio, sm, I2S_BCLK, 2, true) != PICO_OK ||
        pio_sm_set_consecutive_pindirs(pio, sm, I2S_DOUT, 1, true) != PICO_OK) {
        pio_remove_program_and_unclaim_sm(&scp55_i2s_program, pio, sm, offset);
        return false;
    }
    pio_sm_exec(pio, sm, pio_encode_jmp(offset + scp55_i2s_offset_entry_point));

    /* DMA can feed the TX FIFO; no additional GPIO is required. */
    dma_channel_config dma = dma_channel_get_default_config(0);
    channel_config_set_transfer_data_size(&dma, DMA_SIZE_32);
    channel_config_set_read_increment(&dma, true);
    channel_config_set_write_increment(&dma, false);
    channel_config_set_dreq(&dma, pio_get_dreq(pio, sm, true));
    return true;
}

void scp55_pinplan_restore_pins(void)
{
    scp55_midi_led_init();
    if (!configured_pio) return;
    gpio_set_function(MIDI_RX, GPIO_FUNC_UART);
    pio_gpio_init(configured_pio, I2S_BCLK);
    pio_gpio_init(configured_pio, I2S_LRCLK);
    pio_gpio_init(configured_pio, I2S_DOUT);
}

/* Foreground hooks shared by DIN reception and a future USB-MIDI receiver.
 * Retriggering holds the LED on for 100 ms after the latest received byte.
 * No sleeps: MIDI handling continues while the indicator is lit. */
void scp55_midi_led_init(void)
{
    gpio_init(MIDI_ACTIVITY);
    gpio_put(MIDI_ACTIVITY, false);
    gpio_set_drive_strength(MIDI_ACTIVITY, GPIO_DRIVE_STRENGTH_2MA);
    gpio_set_dir(MIDI_ACTIVITY, GPIO_OUT);
    midi_led_off_us = 0;
}

void scp55_midi_activity_notify(void)
{
    midi_led_off_us = time_us_64() + 100000u;
    gpio_put(MIDI_ACTIVITY, true);
}

void scp55_midi_activity_service(void)
{
    if (midi_led_off_us && time_us_64() >= midi_led_off_us) {
        gpio_put(MIDI_ACTIVITY, false);
        midi_led_off_us = 0;
    }
}

#ifdef SCP55_ENABLE_AUDIO_PROBE
/* Finite playback at 44.1 kHz, 8x expansion of the 5512.5 Hz capture.
 * Keep silence before/after the recording for DAC PLL lock and settling. */
static uint32_t playback_frames[8192 * 8 + 8820];
int scp55_i2s_play_u8(const uint8_t *samples, unsigned count)
{
    if (!configured_pio || !samples || !count || count > 8192) return -1;
    unsigned frames = count * 8 + 8820;
    for (unsigned i = 0; i < frames; ++i) playback_frames[i] = 0;
    /* Remove the observed ADC midpoint and boost the quiet bench recording.
     * Bound output to half scale. This is a listening test, not final gain. */
    for (unsigned i = 0; i < count; ++i) {
        int a = ((int)samples[i] - 127) * 4096;
        int b = ((int)samples[i+1 < count ? i+1 : i] - 127) * 4096;
        for (unsigned j = 0; j < 8; ++j) {
            int v = a + (b-a) * (int)j / 8;
            if (v > 16384) v = 16384;
            if (v < -16384) v = -16384;
            uint32_t word = (uint16_t)(int16_t)v;
            playback_frames[4410 + i*8 + j] = (word << 16) | word;
        }
    }
    int dma = dma_claim_unused_channel(false);
    if (dma < 0) return -2;
    PIO pio = configured_pio;
    uint sm = configured_sm;
    scp55_pinplan_restore_pins();
    pio_sm_set_enabled(pio, sm, false);
    pio_sm_init(pio, sm, configured_offset, &configured_config);
    pio_sm_set_consecutive_pindirs(pio, sm, I2S_BCLK, 2, true);
    pio_sm_set_consecutive_pindirs(pio, sm, I2S_DOUT, 1, true);
    pio_sm_exec(pio, sm, pio_encode_jmp(configured_offset + scp55_i2s_offset_entry_point));
    dma_channel_config cfg = dma_channel_get_default_config((uint)dma);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
    channel_config_set_read_increment(&cfg, true);
    channel_config_set_write_increment(&cfg, false);
    channel_config_set_dreq(&cfg, pio_get_dreq(pio, sm, true));
    dma_channel_configure((uint)dma, &cfg, &pio->txf[sm], playback_frames, frames, true);
    pio_sm_set_enabled(pio, sm, true);
    uint64_t deadline = time_us_64() + 4000000u;
    int result = 0;
    while (dma_channel_is_busy((uint)dma) || !pio_sm_is_tx_fifo_empty(pio, sm)) {
        if (time_us_64() > deadline) { result = -3; break; }
        sleep_ms(1);
    }
    sleep_ms(1); /* last word has shifted; tail is already zero */
    dma_channel_abort((uint)dma);
    pio_sm_set_enabled(pio, sm, false);
    pio_sm_clear_fifos(pio, sm);
    dma_channel_unclaim((uint)dma);
    return result;
}
#endif

#ifdef SCP55_ENABLE_AUDIO_PROBE
/* Foreground-serviced DMA ping-pong. Chaining keeps clocks running while
 * the next 256-frame block is prepared; no bus access is performed in IRQs. */
static int stream_dma[2] = {-1,-1};
static uint32_t stream_words[2][256];
static scp55_audio_ring_t *stream_ring;
static scp55_stream_stats_t stream_stats;
static uint64_t stream_last_completion;

static void stream_fill(unsigned index)
{
    for (unsigned i=0;i<256;++i) {
        scp55_audio_frame_t f;
        (void)scp55_audio_ring_pop(stream_ring,&f);
        stream_words[index][i]=((uint32_t)(uint16_t)f.left<<16)|(uint16_t)f.right;
    }
}

int scp55_i2s_stream_start(scp55_audio_ring_t *ring)
{
    if (!configured_pio || !ring || stream_ring || scp55_audio_ring_count(ring)<768) return -1;
    stream_dma[0]=dma_claim_unused_channel(false);
    if (stream_dma[0]<0) return -2;
    stream_dma[1]=dma_claim_unused_channel(false);
    if (stream_dma[1]<0) { dma_channel_unclaim((uint)stream_dma[0]); stream_dma[0]=-1; return -2; }
    stream_ring=ring;
    stream_stats=(scp55_stream_stats_t){.min_fill=UINT32_MAX};
    scp55_pinplan_restore_pins();
    PIO pio=configured_pio; uint sm=configured_sm;
    pio_sm_set_enabled(pio,sm,false);
    pio_sm_init(pio,sm,configured_offset,&configured_config);
    pio_sm_set_consecutive_pindirs(pio,sm,I2S_BCLK,2,true);
    pio_sm_set_consecutive_pindirs(pio,sm,I2S_DOUT,1,true);
    pio_sm_exec(pio,sm,pio_encode_jmp(configured_offset+scp55_i2s_offset_entry_point));
    for (unsigned i=0;i<2;++i) {
        stream_fill(i);
        dma_channel_config cfg=dma_channel_get_default_config((uint)stream_dma[i]);
        channel_config_set_transfer_data_size(&cfg,DMA_SIZE_32);
        channel_config_set_read_increment(&cfg,true);
        channel_config_set_write_increment(&cfg,false);
        channel_config_set_dreq(&cfg,pio_get_dreq(pio,sm,true));
        channel_config_set_chain_to(&cfg,(uint)stream_dma[1-i]);
        dma_channel_configure((uint)stream_dma[i],&cfg,&pio->txf[sm],stream_words[i],256,false);
    }
    stream_last_completion=time_us_64();
    dma_hw->intr=(1u<<(uint)stream_dma[0])|(1u<<(uint)stream_dma[1]);
    dma_start_channel_mask(1u<<(uint)stream_dma[0]);
    pio_sm_set_enabled(pio,sm,true);
    return 0;
}

int scp55_i2s_stream_service(void)
{
    if (!stream_ring) return -1;
    /* Two sequential BUSY reads can straddle a chain handoff and both read
     * false even while DMA is healthy. Use completion progress for liveness. */
    if (time_us_64()-stream_last_completion > 20000u) {
        ++stream_stats.stalls;
        return -2;
    }
    for (unsigned i=0;i<2;++i) {
        uint channel=(uint)stream_dma[i];
        /* TRANS_COUNT reads the live counter, not the reload value. It stays
         * zero after rearming until the peer triggers this channel. Consume
         * the latched completion bit once instead of refilling it repeatedly. */
        if (dma_hw->intr & (1u<<channel)) {
            dma_hw->intr=1u<<channel;
            stream_last_completion=time_us_64();
            if (dma_channel_is_busy(channel)) {++stream_stats.stalls;return -3;}
            stream_fill(i);
            dma_channel_set_read_addr(channel,stream_words[i],false);
            dma_channel_set_trans_count(channel,256,false);
            ++stream_stats.blocks;
            uint32_t fill=scp55_audio_ring_count(stream_ring);
            if (fill<stream_stats.min_fill) stream_stats.min_fill=fill;
            if (fill>stream_stats.max_fill) stream_stats.max_fill=fill;
            /* Slow occupancy feedback follows the independent card clock.
             * Target is 256 frames after refilling the completed block. */
            int32_t ppm=((int32_t)fill-256)*4;
            if (ppm>2000) ppm=2000;
            if (ppm< -2000) ppm=-2000;
            stream_stats.correction_ppm=ppm;
            float rate=44100.0f*(1.0f+(float)ppm/1000000.0f);
            pio_sm_set_clkdiv(configured_pio,configured_sm,(float)clock_get_hz(clk_sys)/(rate*64.0f));
        }
    }
    return 0;
}

void scp55_i2s_stream_stop(void)
{
    if (!stream_ring) return;
    pio_sm_set_enabled(configured_pio,configured_sm,false);
    /* Disable chaining before abort, so neither channel can re-trigger its peer. */
    for (unsigned i=0;i<2;++i) {
        uint ch=(uint)stream_dma[i];
        dma_channel_config cfg=dma_get_channel_config(ch);
        channel_config_set_chain_to(&cfg,ch);
        dma_channel_set_config(ch,&cfg,false);
    }
    for (unsigned i=0;i<2;++i) {
        dma_channel_abort((uint)stream_dma[i]);
        dma_hw->intr=1u<<(uint)stream_dma[i];
        dma_channel_unclaim((uint)stream_dma[i]);
        stream_dma[i]=-1;
    }
    pio_sm_clear_fifos(configured_pio,configured_sm);
    stream_ring=NULL;
}
scp55_stream_stats_t scp55_i2s_stream_stats(void) { return stream_stats; }
#endif
