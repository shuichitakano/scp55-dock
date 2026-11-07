#include "audio_live.h"
#include "audio_capture.h"
#include "audio_resample.h"
#include "mpu401.h"
#include "pinplan.h"
#include "power_button_hw.h"
#include "hardware/uart.h"
#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#ifdef SCP55_ENABLE_USB_MIDI
#include "tusb.h"
#include "midi_router.h"
#include "midi_merge.h"
static midi_merge_t merge;
static midi_router_t usb_router;
#endif
#include <stdio.h>
#include <string.h>

#ifndef SCP55_CAPTURE_RATE
#define SCP55_CAPTURE_RATE 44100
#endif
#if SCP55_CAPTURE_RATE == 11025
#define LIVE_FORMAT 0x52
#elif SCP55_CAPTURE_RATE == 22050
#define LIVE_FORMAT 0x56
#elif SCP55_CAPTURE_RATE == 44100
#define LIVE_FORMAT 0x5a
#else
#error Unsupported SCP55_CAPTURE_RATE
#endif
static scp55_audio_ring_t ring;
static scp55_stereo_resampler_t resampler;
static scp55_capture_stats_t capture;
typedef struct {
    uint32_t samples, bytes, midi_sent, uart_errors, elapsed_ms, fill_last;
    int16_t minimum, maximum;
    uint64_t abs_left, abs_right;
    uint32_t different_frames, din_received, usb_received;
    int result;
    const char *reason;
} live_report_t;
static live_report_t report;
static uint8_t midi_queue[128];
static unsigned midi_head, midi_tail;

static int enqueue(uint8_t b)
{
    if (midi_head-midi_tail==sizeof midi_queue) return -1;
    midi_queue[midi_head++ % sizeof midi_queue]=b;
    return 0;
}
static int note(bool on, uint8_t pitch)
{
    return enqueue(on?0x90:0x80) || enqueue(pitch) || enqueue(on?80:0);
}

int scp55_audio_live_run(const pcmcia_bus_t *bus, const scp55_config_t *cfg,
                         pcmcia_power_t *power, bool demo, unsigned seconds)
{
    bool usb = seconds == 0;
#ifdef SCP55_ENABLE_USB_MIDI
    midi_router_init(&usb_router, MIDI_SOURCE_USB);
    midi_merge_init(&merge);
    bool usb_was_ready=tud_ready();
#endif
    report=(live_report_t){.minimum=INT16_MAX,.maximum=INT16_MIN,.reason="duration"};
    capture=(scp55_capture_stats_t){0};
    scp55_audio_ring_init(&ring); (void)scp55_stereo_resampler_init_rate(&resampler,SCP55_CAPTURE_RATE);
    midi_head=midi_tail=0;
    while (uart_is_readable(uart1)) (void)uart_getc(uart1);
    uart_get_hw(uart1)->rsr=0;
    if (demo) { (void)enqueue(0xc0); (void)enqueue(73); }
    int result=scp55_capture_start_format(bus,cfg,SCP55_CAPTURE_OUTPUT_LOOPBACK,LIVE_FORMAT);
    printf("LIVE START: %u Hz s16le stereo -> 44100 Hz stereo; %s; limit=%u s; x=stop; format_rc=%d\r\n",
           (unsigned)SCP55_CAPTURE_RATE,demo?"demo":usb?"USB + DIN MIDI":"DIN MIDI",seconds,result);
    if (result) { report.result=-60; report.reason="capture setup"; return -60; }
    bool running=false;
    uint8_t current_note=0;
    static const uint8_t melody[]={60,64,67,72,71,67,64,62,60,67,69,72};
    unsigned step=0;
    uint64_t start=time_us_64(), last_sample=start, next_control=start, next_note=start+250000;
    uint64_t midi_wait_start=start, next_midi=start;
    while (usb || time_us_64()-start < (uint64_t)seconds*1000000u) {
        uint64_t now=time_us_64();
        if (running) {
            int dma_rc=scp55_i2s_stream_service();
            if (atomic_load(&ring.underrun_frames)) {result=-72;report.reason="audio buffer errors";break;}
            if (dma_rc) {result=-61;report.reason=dma_rc == -3 ? "DMA refill late" : "DMA no progress";break;}
        }
        int16_t left, right;
        int rc=scp55_capture_try_stereo(bus,cfg,&left,&right,&capture);
        if (capture.sample_errors) {result=-73;report.reason="codec sample errors";break;}
        if (rc<0) {result=-62;report.reason=rc==-4 ? "capture frame alignment" : "capture bus";break;}
        if (rc==0) {
                report.bytes+=4; ++report.samples; last_sample=now;
                if (left<report.minimum) report.minimum=left;
                if (left>report.maximum) report.maximum=left;
                if (right<report.minimum) report.minimum=right;
                if (right>report.maximum) report.maximum=right;
                if (left!=right) ++report.different_frames;
                if (scp55_stereo_resampler_push(&resampler,&ring,left,right)) {result=-63;report.reason="ring full";break;}
                /* Compare audible channel levels after independent DC removal. */
                int32_t l=resampler.left.previous, r=resampler.right.previous;
                report.abs_left += l<0 ? -l : l;
                report.abs_right += r<0 ? -r : r;
                if (!running && scp55_audio_ring_count(&ring)>=768) {
                    if (scp55_i2s_stream_start(&ring)) {result=-64;report.reason="DMA start";break;}
                    running=true;
                }
        }
        if (now-last_sample>50000) {result=-65;report.reason="capture timeout";break;}
        if (demo && now>=next_note) {
            if (current_note && note(false,current_note)) {result=-66;report.reason="MIDI queue";break;}
            current_note=melody[step++ % sizeof melody];
            if (note(true,current_note)) {result=-66;report.reason="MIDI queue";break;}
            next_note=now+400000;
        }
        if (!demo) {
            for (unsigned i=0;i<8 && uart_is_readable(uart1);++i) {
                uint32_t raw=uart_get_hw(uart1)->dr;
                if (raw & 0xf00u) {++report.uart_errors;result=-67;report.reason="UART error";break;}
                uint8_t din=(uint8_t)raw;
                ++report.din_received;
#ifdef SCP55_ENABLE_USB_MIDI
                int queued=usb ? midi_merge_push(&merge,0,&din,1) : enqueue(din);
#else
                int queued=enqueue(din);
#endif
                if (queued) {result=-66;report.reason="DIN queue";break;}
                scp55_midi_activity_notify();
            }
            if (result) break;
        }
#ifdef SCP55_ENABLE_USB_MIDI
        if (usb) {
            uint8_t packet[4];
            /* Backpressure the USB endpoint before consuming a whole event. */
            if (usb_router.count <= sizeof usb_router.bytes - 3 && tud_midi_packet_read(packet)) {
                if (midi_router_usb_event(&usb_router, packet)) {
                    result=-74;report.reason="invalid USB MIDI event";break;
                }
                scp55_midi_activity_notify();
            }
            if (usb_router.count && midi_merge_space(&merge,1)) {
                uint8_t byte=usb_router.bytes[usb_router.tail];
                (void)midi_merge_push(&merge,1,&byte,1);
                usb_router.tail=(usb_router.tail+1)%sizeof usb_router.bytes;
                --usb_router.count;++report.usb_received;
            }
            uint8_t merged;
            if (midi_head-midi_tail<sizeof midi_queue && midi_merge_next(&merge,now,&merged))
                (void)enqueue(merged);

        }
#endif
        /* MIDI wire-rate pacing leaves capture headroom during USB bursts. */
        if (midi_head!=midi_tail && now>=next_midi) {
            rc=mpu401_try_send(bus,cfg,midi_queue[midi_tail % sizeof midi_queue]);
            if (rc<0) {result=-68;report.reason="MIDI bus";break;}
            if (rc==0) {++midi_tail;++report.midi_sent;midi_wait_start=now;next_midi=now+320;}
            else if (now-midi_wait_start>100000) {result=-69;report.reason="MPU busy";break;}
        } else if (midi_head==midi_tail) midi_wait_start=now;
        if (now>=next_control) {
            next_control=now+1000;
#ifdef SCP55_ENABLE_USB_MIDI
            if(!scp55_power_button_service()) {report.reason="BOOT long press";break;}
#endif
            if (pcmcia_power_poll(power)) {result=-70;report.reason="power interlock";break;}
            #ifdef SCP55_ENABLE_USB_MIDI
            bool ready=tud_ready();
            if (usb && usb_was_ready && !ready) {
                midi_router_init(&usb_router,MIDI_SOURCE_USB);
                midi_merge_reset_input(&merge,1);
                /* Close held USB notes on host loss; DIN remains available. */
                for(unsigned channel=0;channel<16;++channel) {
                    uint8_t panic[]={0xb0|channel,64,0,0xb0|channel,123,0,0xb0|channel,120,0};
                    (void)midi_merge_push(&merge,1,panic,sizeof panic);
                }
            }
            usb_was_ready=ready;
            bool connected=usb || stdio_usb_connected();
#else
            bool connected=stdio_usb_connected();
#endif
            if (!connected) {report.reason="USB disconnected";break;}
            if (getchar_timeout_us(0)=='x') {report.reason="stop command";break;}
            scp55_midi_activity_service();
        }
    }
    if (!result && (atomic_load(&ring.underrun_frames) || atomic_load(&ring.overrun_frames))) {
        result=-72;report.reason="audio buffer errors";
    }
    if (!result && capture.sample_errors) {result=-73;report.reason="codec sample errors";}
    report.elapsed_ms=(uint32_t)((time_us_64()-start)/1000);
    report.fill_last=scp55_audio_ring_count(&ring);
    scp55_i2s_stream_stop();
    /* No active audio deadline after DMA stops. Power owner always stops power. */
    if (scp55_capture_stop(bus,cfg) && !result) {result=-71;report.reason="capture stop";}
    report.result=result;
    return result;
}

void scp55_audio_live_dump(void)
{
    scp55_stream_stats_t s=scp55_i2s_stream_stats();
#ifdef SCP55_ENABLE_USB_MIDI
    printf("MIDI INPUT DIN=%lu USB=%lu sysex_aborts=%lu\r\n",
        (unsigned long)report.din_received,(unsigned long)report.usb_received,(unsigned long)merge.sysex_aborts);
#endif
    printf("STEREO filtered_abs_L=%llu abs_R=%llu different_frames=%lu\r\n",
        (unsigned long long)report.abs_left,(unsigned long long)report.abs_right,(unsigned long)report.different_frames);
    printf("LIVE END result=%d reason=%s ms=%lu samples=%lu bytes=%lu min=%d max=%d MIDI=%lu UART_errors=%lu\r\n",
        report.result,report.reason,(unsigned long)report.elapsed_ms,(unsigned long)report.samples,
        (unsigned long)report.bytes,report.minimum,report.maximum,(unsigned long)report.midi_sent,(unsigned long)report.uart_errors);
    printf("LIVE audio: SER=%lu polls=%lu DMA_blocks=%lu stalls=%lu underrun=%lu overrun=%lu fill=%lu/%lu/%lu correction_ppm=%ld clipped=%lu\r\n",
        (unsigned long)capture.sample_errors,(unsigned long)capture.status_reads,(unsigned long)s.blocks,
        (unsigned long)s.stalls,(unsigned long)atomic_load(&ring.underrun_frames),
        (unsigned long)atomic_load(&ring.overrun_frames),(unsigned long)s.min_fill,
        (unsigned long)report.fill_last,(unsigned long)s.max_fill,(long)s.correction_ppm,(unsigned long)(resampler.left.clipped+resampler.right.clipped));
}
