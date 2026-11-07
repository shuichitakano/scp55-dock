#include "scp55.h"

#include <stddef.h>

#define CIS_MAX_BYTES 1024u

static int cis_read(const pcmcia_bus_t *bus, unsigned index, uint8_t *value)
{
    if (index >= CIS_MAX_BYTES) return -1;
    return bus->attr_read(bus->context, (uint16_t)(index * 2u), value);
}

/* VERS_1 stores two version bytes, then NUL-terminated vendor and product. */
static int vers1_matches(const pcmcia_bus_t *bus, unsigned data, unsigned length)
{
    static const char vendor[] = "Roland";
    static const char product[] = "PCMCIA Sound Card";
    const char *expected[] = {vendor, product};
    const unsigned sizes[] = {sizeof vendor, sizeof product};
    unsigned pos = 2;
    if (length < 2) return 0;
    for (unsigned field = 0; field < 2; ++field) {
        for (unsigned i = 0; i < sizes[field]; ++i) {
            uint8_t value;
            if (pos >= length) return 0;
            if (cis_read(bus, data + pos, &value)) return -1;
            if (value != (uint8_t)expected[field][i]) return 0;
            ++pos;
        }
    }
    return 1;
}

int scp55_wait_cis_stable(const pcmcia_bus_t *bus)
{
    uint8_t previous[4], current[4];
    if (!bus || !bus->attr_read || !bus->delay_ms) return -1;
    for (unsigned i = 0; i < 4; ++i)
        if (cis_read(bus, i, &previous[i])) return -2;
    for (unsigned attempt = 0; attempt < 250; ++attempt) {
        bus->delay_ms(bus->context, 20);
        for (unsigned i = 0; i < 4; ++i)
            if (cis_read(bus, i, &current[i])) return -2;
        if (current[0] != 0xff &&
            current[0] == previous[0] && current[1] == previous[1] &&
            current[2] == previous[2] && current[3] == previous[3]) return 0;
        for (unsigned i = 0; i < 4; ++i) previous[i] = current[i];
    }
    return -3;
}

int scp55_identify(const pcmcia_bus_t *bus, scp55_config_t *config)
{
    unsigned offset = 0;
    int found_manfid = 0, found_vers1 = 0, found_config = 0;
    uint16_t cor = 0;

    if (!bus || !config || !bus->attr_read || !bus->attr_write) return -1;
    *config = (scp55_config_t){0};
    for (unsigned tuple = 0; tuple < 128 && offset + 2 < CIS_MAX_BYTES; ++tuple) {
        uint8_t code, length;
        if (cis_read(bus, offset, &code)) return -2;
        if (code == 0xff) break;
        if (code == 0x00) { ++offset; continue; }
        if (cis_read(bus, offset + 1, &length)) return -2;
        if (length == 0xff || offset + 2u + length > CIS_MAX_BYTES) return -3;
        if (code == 0x20 && length >= 4) {
            uint8_t b[4];
            for (unsigned i = 0; i < 4; ++i)
                if (cis_read(bus, offset + 2u + i, &b[i])) return -2;
            found_manfid = (b[0] == 0x0c && b[1] == 0xc0 &&
                            b[2] == 0x01 && b[3] == 0x00);
        }
        if (code == 0x15) {
            int match = vers1_matches(bus, offset + 2u, length);
            if (match < 0) return -2;
            if (match) found_vers1 = 1;
        }
        if (code == 0x1a && length >= 4) {
            uint8_t size, low, high = 0;
            if (cis_read(bus, offset + 2u, &size)) return -2;
            unsigned address_bytes = (size & 3u) + 1u;
            if (length < 2u + address_bytes) return -3;
            if (cis_read(bus, offset + 4u, &low)) return -2;
            if (address_bytes > 1u && cis_read(bus, offset + 5u, &high)) return -2;
            /* This host exposes A0-A15 only. Reject a nonzero upper RADR. */
            for (unsigned i = 2; i < address_bytes; ++i) {
                uint8_t upper;
                if (cis_read(bus, offset + 4u + i, &upper)) return -2;
                if (upper) return -3;
            }
            cor = (uint16_t)low | ((uint16_t)high << 8);
            found_config = 1;
        }
        offset += 2u + length;
    }
    if (!found_manfid || !found_vers1 || !found_config || (cor & 1u)) return -4;
    config->cor_address = cor;
    config->io_base = 0x330;
    config->config_index = 0x01;
    return 0;
}

int scp55_set_cor(const pcmcia_bus_t *bus, const scp55_config_t *config)
{
    uint8_t readback;
    if (!bus || !config || !bus->attr_read || !bus->attr_write ||
        config->cor_address != 0x400 || config->io_base != 0x330 ||
        config->config_index != 0x01) return -1;
    if (bus->attr_write(bus->context, config->cor_address, config->config_index)) return -2;
    if (bus->delay_ms) bus->delay_ms(bus->context, 5);
    if (bus->attr_read(bus->context, config->cor_address, &readback)) return -3;
    return ((readback & 0x3f) == config->config_index) ? 0 : -4;
}
