#include "diagnostic.h"
#include "board.h"
#include "pinplan.h"
#include "power_button_hw.h"
#ifdef SCP55_ENABLE_AUDIO_PROBE
#include "audio_probe.h"
#endif

#ifdef SCP55_ENABLE_USB_MIDI
#include "tusb.h"
#endif
#include <stdio.h>

#include "hardware/uart.h"
#include "pico/stdlib.h"
#include "pico/stdio_usb.h"

#if !defined(PICO_RP2350A) || PICO_RP2350A != 0
#error This diagnostic requires the 48-GPIO RP2354B board definition
#endif

int main(void)
{
    scp55_board_safe_init();
    scp55_power_button_hw_init();
    bool pinplan_ready = scp55_pinplan_prepare();
    stdio_init_all();
    bool greeted = false;
#ifdef SCP55_ENABLE_USB_MIDI
    bool usb_started = false;
    bool power_wanted = true;
#endif
    uint32_t midi_bytes = 0;
    uint8_t midi_last = 0;
    uint64_t last_report_us = 0;
    while (true) {
#ifdef SCP55_ENABLE_USB_MIDI
        bool wanted=scp55_power_button_service();
        if(wanted && !power_wanted) {
            uint8_t discard[4];
            while(tud_midi_packet_read(discard)) {}
            usb_started=false;
        }
        power_wanted=wanted;
        if(!power_wanted) {
            uint8_t discard[4];
            while(tud_midi_packet_read(discard)) {}
        }
#endif
        if (pinplan_ready) {
            while (uart_is_readable(uart1)) {
                midi_last = (uint8_t)uart_getc(uart1);
                ++midi_bytes;
                scp55_midi_activity_notify();
            }
        }
        scp55_midi_activity_service();
        uint64_t now_us = time_us_64();
        if (now_us - last_report_us < 500000u) {
            sleep_ms(1);
            continue;
        }
        last_report_us = now_us;
        scp55_diag_sample_t a = scp55_board_sample();
        sleep_ms(2);
        scp55_diag_sample_t b = scp55_board_sample();
        uint32_t faults = scp55_diag_evaluate(&b);
        if (!scp55_diag_same(&a, &b)) faults |= SCP55_DIAG_UNSTABLE;
#ifdef SCP55_ENABLE_USB_MIDI
        if (!scp55_diag_card_present(&b)) usb_started=false;
        if (power_wanted && !usb_started && pinplan_ready && faults==0 && scp55_diag_card_present(&b)) {
            usb_started=true;
            scp55_config_t config;
            (void)scp55_board_audio_probe(&config,-5);
            scp55_pinplan_restore_pins();
            scp55_power_button_standby();
            power_wanted=false;
            continue;
        }
#endif
        if (stdio_usb_connected()) {
            if (!greeted) {
                puts("SCP55 RP2354B diagnostic; card power off at startup");
                printf("UART1/I2S PIO pin plan: %s\r\n", pinplan_ready ? "ready" : "FAILED");
                puts("CD: 1=open, 0=ground; flags: 01=CD mismatch, 04=power active, 08=bus active, 20=unstable; fixed 5V SCP-55 only");
#ifdef SCP55_ENABLE_CIS_PROBE
#if SCP55_ALLOW_UNCOMPENSATED_PROBE
                printf("Bench probe: reference=%u mV; uncompensated 4750..5250 mV window; send 'c' for CIS\r\n",
                       (unsigned)SCP55_ADC_REFERENCE_MV);
#else
                puts("CIS probe: calibration required; send 'c' after bench voltage checks");
#endif
#endif
#ifdef SCP55_ENABLE_NOTE_TEST
                puts("Note test: send 'n' to initialize SCP-55 and play note 60 for 300 ms; power off on exit");
#endif
#ifdef SCP55_ENABLE_AUDIO_PROBE
                puts("Live audio: r=12s demo, s=60s demo, m=DIN MIDI (10 min max), x=stop; stops on USB disconnect");
                puts("Audio capture: a=output loopback, u=AUX1, l=line; bounded capture then power off; p=replay LINE OUT");
#endif
                greeted = true;
            }
            printf("CD=%u%u CARD_mV=%u PWR=%u BUS_EN=%u present=%u flags=%02lx MIDI_bytes=%lu last=%02x\r\n",
                   b.cd1_high, b.cd2_high, b.card_mv,
                   b.power_en, b.bus_enable,
                   scp55_diag_card_present(&b), (unsigned long)faults,
                   (unsigned long)midi_bytes, midi_last);
#ifdef SCP55_ENABLE_CIS_PROBE
            int command = getchar_timeout_us(0);
#ifdef SCP55_ENABLE_USB_MIDI
            if (command == 'v') {
                scp55_power_button_hw_init();
                power_wanted=true;
                uint8_t discard[4];
                while (tud_midi_packet_read(discard)) {}
                usb_started=false;
                continue;
            }
#endif
#ifdef SCP55_ENABLE_AUDIO_PROBE
            if (command == 'p') {
                printf("Replay result=%d\r\n", scp55_audio_probe_play());
            }
#endif
            bool probe_requested = command == 'c';
#ifdef SCP55_ENABLE_AUDIO_PROBE
            int capture_source = command == 'a' ? 3 : command == 'u' ? 1 : command == 'l' ? 0 : -1;
            bool live_requested = command == 'r' || command == 's' || command == 'm';
            if (live_requested) capture_source = command == 'm' ? -3 : command == 's' ? -4 : -2;
            probe_requested = probe_requested || capture_source >= 0 || live_requested;
#endif
#ifdef SCP55_ENABLE_NOTE_TEST
            probe_requested = probe_requested || command == 'n';
#endif
            if (probe_requested && (faults != 0 || !scp55_diag_card_present(&b)))
                puts("Probe refused: card absent or diagnostic flags set; no power applied");
            if (probe_requested && faults == 0 && scp55_diag_card_present(&b)) {
                scp55_config_t config;
                printf("%s test starting\r\n", command == 'n' ? "Note" : command == 'c' ? "CIS" : "Audio");
                int result;
#ifdef SCP55_ENABLE_AUDIO_PROBE
                if (capture_source >= 0 || live_requested) result = scp55_board_audio_probe(&config, capture_source);
                else
#endif
#ifdef SCP55_ENABLE_NOTE_TEST
                if (command == 'n') result = scp55_board_note_test(&config);
                else
#endif
                result = scp55_board_read_cis(&config);
                if (result == 0)
                    printf("%s OK: COR=%04x IO=%04x index=%02x; power now off\r\n",
                           command == 'n' ? "Note" : command == 'c' ? "CIS" : "Audio", config.cor_address, config.io_base, config.config_index);
                else
                    if (result == -16) puts("Probe locked: ADC reference/error calibration not configured; no power applied");
                    else printf("Probe failed: %d; power now off\r\n", result);
                if (pinplan_ready) scp55_pinplan_restore_pins();
#ifdef SCP55_ENABLE_AUDIO_PROBE
                if (!result && capture_source >= 0 && pinplan_ready)
                    (void)scp55_audio_probe_play();
#endif
                /* Discard commands queued during the blocking test, so a
                 * pasted/repeated command cannot trigger another power cycle. */
                while (getchar_timeout_us(0) != PICO_ERROR_TIMEOUT) {}
            }
#endif
        } else {
            greeted = false;
        }
    }
}
