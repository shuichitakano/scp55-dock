#include "midi_router.h"
#include "mpu401.h"

#include <assert.h>

typedef struct {
    uint8_t sent[16];
    unsigned count;
    uint8_t status;
    unsigned delays;
} mock_mpu_t;

static int io_read(void *context, uint16_t address, uint8_t *value)
{
    mock_mpu_t *mock = context;
    if (address != 0x331) return -1;
    *value = mock->status;
    return 0;
}

static int io_write(void *context, uint16_t address, uint8_t value)
{
    mock_mpu_t *mock = context;
    if (address != 0x330 || mock->count >= sizeof mock->sent) return -1;
    mock->sent[mock->count++] = value;
    return 0;
}

static void delay_ms(void *context, uint32_t milliseconds)
{
    ++((mock_mpu_t *)context)->delays;
    (void)milliseconds;
}

int main(void)
{
    mock_mpu_t mock = {0};
    pcmcia_bus_t bus = {.context = &mock, .io_read = io_read,
                        .io_write = io_write, .delay_ms = delay_ms};
    scp55_config_t config = {.io_base = 0x330};
    midi_router_t router;
    midi_router_init(&router, MIDI_SOURCE_DIN);
    assert(midi_router_din_byte(&router, 0x90) == 0);
    assert(midi_router_din_byte(&router, 60) == 0);
    assert(midi_router_din_byte(&router, 64) == 0);
    assert(midi_router_service(&router, &bus, &config, 2) == 2);
    assert(router.count == 1 && mock.sent[0] == 0x90 && mock.sent[1] == 60);

    midi_router_select(&router, MIDI_SOURCE_USB);
    assert(router.count == 0); /* do not interleave the partial DIN message */
    const uint8_t note[] = {0x09, 0x90, 64, 100};
    const uint8_t sysex_end[] = {0x05, 0xf7, 0x00, 0x00};
    const uint8_t wrong_cable[] = {0x19, 0x90, 64, 100};
    assert(midi_router_usb_event(&router, wrong_cable) != 0);
    assert(midi_router_usb_event(&router, note) == 0);
    assert(midi_router_usb_event(&router, sysex_end) == 0);
    assert(midi_router_service(&router, &bus, &config, 16) == 4);
    assert(mock.sent[2] == 0x90 && mock.sent[3] == 64 &&
           mock.sent[4] == 100 && mock.sent[5] == 0xf7);
    assert(router.count == 0 && router.dropped == 0);
    /* SysEx spans packets; real-time and two-byte channel messages retain order. */
    const uint8_t events[][4]={{0x04,0xf0,0x7e,0x7f},{0x07,0x09,0x01,0xf7},
                              {0x0f,0xf8,0,0},{0x0c,0xc0,73,0}};
    const uint8_t expected[]={0xf0,0x7e,0x7f,0x09,0x01,0xf7,0xf8,0xc0,73};
    mock.count=0;
    for (unsigned i=0;i<4;++i) assert(midi_router_usb_event(&router,events[i])==0);
    assert(midi_router_service(&router,&bus,&config,16)==9);
    for (unsigned i=0;i<9;++i) assert(mock.sent[i]==expected[i]);
    /* A packet must never be partially accepted when the queue is full. */
    for (unsigned i=0;i<170;++i) assert(midi_router_usb_event(&router,note)==0);
    assert(router.count==510);
    unsigned head=router.head;
    assert(midi_router_usb_event(&router,note)==-1);
    assert(router.count==510 && router.head==head && router.dropped==3);
    midi_router_init(&router,MIDI_SOURCE_USB);
    unsigned before = mock.count;
    mock.status = 0x40;
    assert(mpu401_try_send(&bus, &config, 0x90) == 1);
    assert(mock.count == before && mock.delays == 0);
    mock.status = 0;
    assert(mpu401_try_send(&bus, &config, 0x90) == 0);
    assert(mock.count == before+1 && mock.delays == 0);
    return 0;
}
