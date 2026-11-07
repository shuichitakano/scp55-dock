#include "audio_probe.h"
#include "pinplan.h"
#include "audio_capture.h"
#include "mpu401.h"
#include "pico/time.h"
#include <stdio.h>
#include <string.h>

static uint8_t samples[4][2048];
static unsigned counts[4];
static uint32_t elapsed[4];
static scp55_capture_stats_t stats[4];

void scp55_audio_probe_clear(void)
{
    memset(counts, 0, sizeof counts);
    memset(elapsed, 0, sizeof elapsed);
    memset(stats, 0, sizeof stats);
}

static int midi(const pcmcia_bus_t *bus, const scp55_config_t *cfg,
                uint8_t status, uint8_t note, uint8_t velocity)
{
    return mpu401_send(bus, cfg, status) || mpu401_send(bus, cfg, note) ||
           mpu401_send(bus, cfg, velocity);
}

static void registers(const pcmcia_bus_t *bus, const scp55_config_t *cfg)
{
    const uint8_t indices[] = {0,1,8,9,11,12,28};
    for (unsigned i = 0; i < sizeof indices; ++i) {
        uint8_t value = 0;
        int rc = bus->io_write(bus->context, cfg->io_base+8, indices[i]);
        if (!rc) rc = bus->io_read(bus->context, cfg->io_base+9, &value);
        printf("Codec I%u=%02x rc=%d\r\n", indices[i], value, rc);
    }
}

int scp55_audio_probe(const pcmcia_bus_t *bus, const scp55_config_t *cfg, int source)
{
    scp55_audio_probe_clear();
    int result = scp55_capture_start(bus, cfg, (scp55_capture_source_t)source);
    printf("Capture source=%d start=%d\r\n", source, result);
    registers(bus, cfg);
    if (result) return -40 + result;
    /* Flute program; compare silence, A4, A5, then release. */
    if (mpu401_send(bus, cfg, 0xc0) || mpu401_send(bus, cfg, 73)) return -50;
    for (unsigned phase = 0; phase < 4; ++phase) {
        if (phase == 1 && midi(bus, cfg, 0x90, 69, 80)) return -50;
        if (phase == 2 && (midi(bus, cfg, 0x80, 69, 0) ||
                          midi(bus, cfg, 0x90, 81, 80))) return -50;
        if (phase == 3 && midi(bus, cfg, 0x80, 81, 0)) return -50;
        uint64_t start = time_us_64();
        unsigned target = phase == 0 || phase == 3 ? 1024 : 2048;
        while (counts[phase] < target) {
            if (time_us_64() - start > 2000000u) {
                ++stats[phase].timeout_count;
                result = -51;
                break;
            }
            int rc = scp55_capture_try_read(bus, cfg, &samples[phase][counts[phase]], &stats[phase]);
            if (rc < 0) { result = -52; break; }
            if (rc == 0) ++counts[phase];
        }
        elapsed[phase] = (uint32_t)(time_us_64() - start);
        if (result) break;
    }
    (void)midi(bus, cfg, 0xb0, 120, 0);
    if (scp55_capture_stop(bus, cfg) && !result) result = -53;
    return result;
}

void scp55_audio_probe_dump(void)
{
    /* Only called after board power has been stopped. */
    for (unsigned p = 0; p < 4; ++p) {
        printf("AUDIO phase=%u count=%u elapsed_us=%lu polls=%lu errors=%lu timeouts=%lu\r\n",
               p, counts[p], (unsigned long)elapsed[p], (unsigned long)stats[p].status_reads,
               (unsigned long)stats[p].sample_errors, (unsigned long)stats[p].timeout_count);
        for (unsigned i = 0; i < counts[p]; i += 64) {
            printf("PCM %u %u ", p, i);
            for (unsigned j = i; j < i+64 && j < counts[p]; ++j) printf("%02x", samples[p][j]);
            puts("");
        }
    }
}

int scp55_audio_probe_play(void)
{
    static uint8_t recording[8192];
    unsigned count = 0;
    if (counts[0] != 1024 || counts[1] != 2048 || counts[2] != 2048 || counts[3] != 1024)
        return -1;
    for (unsigned p = 0; p < 4; ++p) {
        memcpy(recording + count, samples[p], counts[p]);
        count += counts[p];
    }
    puts("LINE OUT: playing captured SCP-55 audio, 44.1 kHz stereo; card power off");
    int result = scp55_i2s_play_u8(recording, count);
    printf("LINE OUT DMA result=%d; analog output not measured\r\n", result);
    return result;
}
