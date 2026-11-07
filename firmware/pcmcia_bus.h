#ifndef SCP55_PCMCIA_BUS_H
#define SCP55_PCMCIA_BUS_H

#include <stdint.h>

/* Return 0 on success; a negative value on timeout, removal, or bus fault.
 * attr_addr is the PHYSICAL card address: CIS byte n is at 2*n; COR is 0x400.
 * A driver must never start a cycle unless power, card detect, reset, and
 * translator interlocks have been satisfied. */
typedef struct {
    void *context;
    int (*attr_read)(void *context, uint16_t attr_addr, uint8_t *value);
    int (*attr_write)(void *context, uint16_t attr_addr, uint8_t value);
    int (*io_read)(void *context, uint16_t io_addr, uint8_t *value);
    int (*io_write)(void *context, uint16_t io_addr, uint8_t value);
    void (*delay_ms)(void *context, uint32_t ms);
} pcmcia_bus_t;

#endif
