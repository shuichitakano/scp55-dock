#include "audio_capture.h"

enum {
    REG_INDEX = 8, REG_DATA = 9, REG_STATUS = 6, REG_PIO = 7,
    I_LEFT_SOURCE = 0, I_RIGHT_SOURCE = 1, I_FORMAT = 8, I_INTERFACE = 9,
    MCE = 0x40, CPIO = 0x80, CEN = 0x02, CRDY = 0x20, SER = 0x10
};

static int codec_ready(const pcmcia_bus_t *bus, uint16_t base)
{
    uint8_t value;
    for (unsigned i = 0; i < 200; ++i) {
        if (bus->io_read(bus->context, base + REG_INDEX, &value)) return -1;
        if (!(value & 0x80)) return 0;
        bus->delay_ms(bus->context, 1);
    }
    return -1;
}

static int codec_put(const pcmcia_bus_t *bus, uint16_t base,
                     uint8_t index, uint8_t value, int mce)
{
    if (codec_ready(bus, base)) return -1;
    if (bus->io_write(bus->context, base + REG_INDEX,
                      (uint8_t)(index | (mce ? MCE : 0)))) return -1;
    if (bus->io_write(bus->context, base + REG_DATA, value)) return -1;
    return 0;
}

static int codec_get(const pcmcia_bus_t *bus, uint16_t base,
                     uint8_t index, uint8_t *value)
{
    if (codec_ready(bus, base)) return -1;
    if (bus->io_write(bus->context, base + REG_INDEX, index)) return -1;
    return bus->io_read(bus->context, base + REG_DATA, value);
}

int scp55_capture_start(const pcmcia_bus_t *bus, const scp55_config_t *config,
                        scp55_capture_source_t source)
{
    return scp55_capture_start_format(bus, config, source, 0x00);
}

int scp55_capture_start_format(const pcmcia_bus_t *bus, const scp55_config_t *config,
                              scp55_capture_source_t source, uint8_t requested_format)
{
    uint8_t interface, format, left, right;
    uint16_t base;
    if (!bus || !config || config->io_base != 0x330 ||
        !bus->io_read || !bus->io_write || !bus->delay_ms ||
        (requested_format != 0x00 && requested_format != 0x40 && requested_format != 0x42 && requested_format != 0x52 && requested_format != 0x56 && requested_format != 0x5a) ||
        (source != SCP55_CAPTURE_LINE && source != SCP55_CAPTURE_AUX1 &&
         source != SCP55_CAPTURE_OUTPUT_LOOPBACK)) return -1;
    base = config->io_base;

    /* Input control I0/I1 bits 7:6: 00 Line, 01 AUX1, 11 Line Out. */
    uint8_t source_value = (uint8_t)(source << 6);
    if (codec_put(bus, base, I_LEFT_SOURCE, source_value, 0) ||
        codec_put(bus, base, I_RIGHT_SOURCE, source_value, 0) ||
        codec_get(bus, base, I_LEFT_SOURCE, &left) ||
        codec_get(bus, base, I_RIGHT_SOURCE, &right) ||
        (left & 0xc0) != source_value || (right & 0xc0) != source_value)
        return -2;

    /* SCP-55's sole crystal is 16.9344 MHz on XI1. Source bit 0 stays 0.
     * I8=0 means unsigned 8-bit mono at 5.512 kHz on this card.
     * I9=0x80 selects PIO capture with capture initially disabled. */
    if (codec_put(bus, base, I_FORMAT, requested_format, 1) ||
        codec_put(bus, base, I_INTERFACE, CPIO, 1) ||
        bus->io_write(bus->context, base + REG_INDEX, I_INTERFACE) ||
        codec_ready(bus, base) ||
        codec_get(bus, base, I_FORMAT, &format) ||
        codec_get(bus, base, I_INTERFACE, &interface) ||
        format != requested_format || (interface & (CPIO | CEN)) != CPIO)
        return -3;
    if (codec_put(bus, base, I_INTERFACE, CPIO | CEN, 0) ||
        codec_get(bus, base, I_INTERFACE, &interface) ||
        (interface & (CPIO | CEN)) != (CPIO | CEN)) return -4;
    return 0;
}

int scp55_capture_try_read(const pcmcia_bus_t *bus, const scp55_config_t *config,
                          uint8_t *sample, scp55_capture_stats_t *stats)
{
    uint8_t status;
    if (!bus || !config || config->io_base != 0x330 || !sample || !stats ||
        !bus->io_read) return -1;
    if (bus->io_read(bus->context, config->io_base + REG_STATUS, &status)) return -2;
    ++stats->status_reads;
    if (status & SER) ++stats->sample_errors;
    if (!(status & CRDY)) return 1;
    return bus->io_read(bus->context, config->io_base + REG_PIO, sample) ? -2 : 0;
}

int scp55_capture_read(const pcmcia_bus_t *bus, const scp55_config_t *config,
                       uint8_t *sample, scp55_capture_stats_t *stats)
{
    uint8_t status;
    if (!bus || !config || config->io_base != 0x330 || !sample || !stats ||
        !bus->io_read || !bus->delay_ms)
        return -1;
    for (unsigned i = 0; i < 50; ++i) {
        if (bus->io_read(bus->context, config->io_base + REG_STATUS, &status))
            return -2;
        ++stats->status_reads;
        if (status & SER) ++stats->sample_errors;
        if (status & CRDY)
            return bus->io_read(bus->context, config->io_base + REG_PIO, sample);
        bus->delay_ms(bus->context, 1);
    }
    ++stats->timeout_count;
    return -3;
}

int scp55_capture_stop(const pcmcia_bus_t *bus, const scp55_config_t *config)
{
    uint8_t interface;
    if (!bus || !config || config->io_base != 0x330 ||
        !bus->io_read || !bus->io_write || !bus->delay_ms)
        return -1;
    if (codec_get(bus, config->io_base, I_INTERFACE, &interface)) return -2;
    return codec_put(bus, config->io_base, I_INTERFACE,
                     (uint8_t)(interface & ~CEN), 0);
}

int scp55_capture_try_stereo(const pcmcia_bus_t *bus, const scp55_config_t *cfg,
    int16_t *left, int16_t *right, scp55_capture_stats_t *stats)
{
    if(!bus || !cfg || cfg->io_base!=0x330 || !bus->io_read || !left || !right || !stats) return -1;
    uint8_t status, bytes[4];
    if(bus->io_read(bus->context,cfg->io_base+REG_STATUS,&status)) return -2;
    ++stats->status_reads;
    if(status&SER) {++stats->sample_errors;return -3;}
    if(!(status&CRDY)) return 1;
    /* Bit 6=left; bit 7=upper. Never silently swap channels after lost data. */
    if((status&0xc0)!=0x40) return -4;
    for(unsigned i=0;i<4;++i)
        if(bus->io_read(bus->context,cfg->io_base+REG_PIO,&bytes[i])) return -2;
    *left=(int16_t)((uint16_t)bytes[0]|((uint16_t)bytes[1]<<8));
    *right=(int16_t)((uint16_t)bytes[2]|((uint16_t)bytes[3]<<8));
    return 0;
}
