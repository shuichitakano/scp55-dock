#include "audio_capture.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t selected;
    uint8_t regs[32];
    unsigned mce_writes;
    unsigned bad_ports;
    unsigned pio_reads;
    uint8_t status;
    int fail_pio;
    bool stereo_data;
} mock_codec_t;

static int io_write(void *context, uint16_t address, uint8_t value)
{
    mock_codec_t *mock = context;
    if (address == 0x338) {
        mock->selected = value;
        return 0;
    }
    if (address == 0x339) {
        if (mock->selected & 0x40) ++mock->mce_writes;
        mock->regs[mock->selected & 0x1f] = value;
        return 0;
    }
    ++mock->bad_ports;
    return -1;
}

static int io_read(void *context, uint16_t address, uint8_t *value)
{
    mock_codec_t *mock = context;
    if (address == 0x338) *value = mock->selected;
    else if (address == 0x339) *value = mock->regs[mock->selected & 0x1f];
    else if (address == 0x336) *value = mock->status; /* CRDY */
    else if (address == 0x337) {
        if (mock->fail_pio) return -7;
        static const uint8_t frame[]={0x34,0x12,0xfe,0xff};
        *value = mock->stereo_data ? frame[mock->pio_reads%4] : 0x5a;
        ++mock->pio_reads;
    } else {
        ++mock->bad_ports;
        return -1;
    }
    return 0;
}

static void delay_ms(void *context, uint32_t milliseconds)
{
    (void)context;
    (void)milliseconds;
}

int main(void)
{
    mock_codec_t mock = {.status = 0x20};
    pcmcia_bus_t bus = {.context = &mock, .io_read = io_read,
                        .io_write = io_write, .delay_ms = delay_ms};
    scp55_config_t config = {.cor_address = 0x400, .io_base = 0x330,
                             .config_index = 1};
    scp55_capture_stats_t stats = {0};
    uint8_t sample = 0;
    assert(scp55_capture_start(&bus, &config, SCP55_CAPTURE_OUTPUT_LOOPBACK) == 0);
    assert(mock.regs[0] == 0xc0 && mock.regs[1] == 0xc0);
    assert(mock.regs[8] == 0x00 && mock.regs[9] == 0x82);
    assert(mock.mce_writes == 2 && mock.bad_ports == 0);
    assert(scp55_capture_read(&bus, &config, &sample, &stats) == 0);
    assert(sample == 0x5a && mock.pio_reads == 1 && stats.status_reads == 1);
    mock.status = 0;
    assert(scp55_capture_try_read(&bus, &config, &sample, &stats) == 1);
    assert(mock.pio_reads == 1);
    mock.status = 0x30;
    assert(scp55_capture_try_read(&bus, &config, &sample, &stats) == 0);
    assert(stats.sample_errors == 1);
    mock.fail_pio = 1;
    assert(scp55_capture_try_read(&bus, &config, &sample, &stats) == -2);
    assert(scp55_capture_stop(&bus, &config) == 0);
    assert(mock.regs[9] == 0x80);
    assert(!scp55_capture_start_format(&bus, &config, SCP55_CAPTURE_OUTPUT_LOOPBACK, 0x42));
    assert(mock.regs[8] == 0x42 && mock.regs[9] == 0x82);
    assert(!scp55_capture_start_format(&bus, &config, SCP55_CAPTURE_OUTPUT_LOOPBACK, 0x52));
    assert(mock.regs[8] == 0x52);
    assert(scp55_capture_start_format(&bus, &config, SCP55_CAPTURE_OUTPUT_LOOPBACK, 0xff) < 0);
    assert(!scp55_capture_start_format(&bus,&config,SCP55_CAPTURE_OUTPUT_LOOPBACK,0x56));
    assert(mock.regs[8]==0x56);
    assert(!scp55_capture_start_format(&bus,&config,SCP55_CAPTURE_OUTPUT_LOOPBACK,0x5a));
    assert(mock.regs[8]==0x5a);
    mock.fail_pio=0;mock.pio_reads=0;mock.status=0x60;mock.stereo_data=true;
    int16_t left=0,right=0;
    assert(!scp55_capture_try_stereo(&bus,&config,&left,&right,&stats));
    assert(left==0x1234 && right==-2 && mock.pio_reads==4);
    mock.status=0;
    assert(scp55_capture_try_stereo(&bus,&config,&left,&right,&stats)==1);
    assert(mock.pio_reads==4);
    mock.status=0xe0;
    assert(scp55_capture_try_stereo(&bus,&config,&left,&right,&stats)==-4);
    assert(mock.pio_reads==4);
    mock.status=0x70;
    assert(scp55_capture_try_stereo(&bus,&config,&left,&right,&stats)==-3);
    assert(mock.pio_reads==4);

    return 0;
}
