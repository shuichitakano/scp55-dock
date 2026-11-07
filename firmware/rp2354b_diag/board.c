#include "board.h"
#include "voltage_guard.h"
#include <stdio.h>
#ifdef SCP55_ENABLE_AUDIO_PROBE
#include "audio_probe.h"
#include "audio_live.h"
#endif

#ifndef SCP55_ALLOW_UNCOMPENSATED_PROBE
#define SCP55_ALLOW_UNCOMPENSATED_PROBE 0
#endif
#ifdef SCP55_ENABLE_NOTE_TEST
#include "bringup.h"
#endif

#ifndef SCP55_ADC_REFERENCE_MV
#define SCP55_ADC_REFERENCE_MV 0
#endif
#ifndef SCP55_CARD_ERROR_MV
#define SCP55_CARD_ERROR_MV 0
#endif

#include "pcmcia_bitbang.h"

#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/clocks.h"
#include "pico/stdlib.h"

enum {
    MASTER_CLEAR_N = 25,
    RESET = 31,
    WAIT_N = 32,
    CD1_HIGH = 33,
    CD2_HIGH = 34,
    POWER_EN = 35,
    DATA_DIR = 38,
    DATA_ENABLE = 39,
    POWER_ARM_CLK = 40,
    MCU_BUS_REQUEST = 44,
    CARD_VMON = 45,
    BUS_ENABLE = 47
};

static void output(unsigned pin, bool high)
{
    gpio_put(pin, high);
    gpio_set_dir(pin, GPIO_OUT);
}

void scp55_board_safe_init(void)
{
    for (unsigned pin = 0; pin < 48; ++pin) {
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_IN);
        gpio_disable_pulls(pin);
    }
    output(MASTER_CLEAR_N, false);
    output(DATA_ENABLE, false);
    output(MCU_BUS_REQUEST, false);
    output(POWER_ARM_CLK, false);
    adc_init();
    adc_gpio_init(CARD_VMON);
}

static uint16_t card_millivolts(void)
{
    /* GPIO45 = ADC5; R4/R5 = 10k/10k. Zero reference gives a nominal
     * display only; calibrated limits are required before powered probing. */
    adc_select_input(5);
    return scp55_voltage_convert(adc_read(), SCP55_ADC_REFERENCE_MV);
}

scp55_diag_sample_t scp55_board_sample(void)
{
    return (scp55_diag_sample_t){
        .cd1_high = gpio_get(CD1_HIGH),
        .cd2_high = gpio_get(CD2_HIGH),
        .power_en = gpio_get(POWER_EN),
        .bus_enable = gpio_get(BUS_ENABLE),
        .card_mv = card_millivolts(),
    };
}

#ifdef SCP55_ENABLE_CIS_PROBE

static bool card_present(void *unused)
{
    (void)unused;
    return !gpio_get(CD1_HIGH) && !gpio_get(CD2_HIGH);
}

static bool card_5v_compatible(void *unused)
{
    (void)unused;
    /* Dedicated 5 V SCP-55 hardware; no general PC Card voltage detection. */
    return true;
}

static uint64_t voltage_good_since_us;
static uint16_t last_power_check_mv;
/* Audio checks physical removal/latches on each bus cycle, ADC at most 0.5 ms old.
 * Initialization retains fresh ADC conversion on every check. */
static bool streaming_power_cache;
static uint32_t streaming_delay_cycles;
static uint64_t power_sample_us;
static bool power_sample_good;

static bool card_power_good(void *unused)
{
    (void)unused;
    uint64_t now=time_us_64();
    if (streaming_power_cache) {
        if (!gpio_get(POWER_EN) || !gpio_get(BUS_ENABLE)) return false;
        if (power_sample_us && now-power_sample_us<500) return power_sample_good;
    }
    uint16_t mv = card_millivolts();
    power_sample_us=now;
    last_power_check_mv = mv;
    bool good = scp55_voltage_probe_good(mv, SCP55_ADC_REFERENCE_MV,
        SCP55_CARD_ERROR_MV, SCP55_ALLOW_UNCOMPENSATED_PROBE);
    power_sample_good=good;
    if (!good) voltage_good_since_us = 0;
    else if (!voltage_good_since_us) voltage_good_since_us = time_us_64();
    return good;
}

static void master_clear_n(void *unused, bool high)
{
    (void)unused;
    gpio_put(MASTER_CLEAR_N, high);
}

static void clock_request(unsigned pin, bool enabled)
{
    gpio_put(pin, false);
    if (enabled) {
        sleep_us(2);
        gpio_put(pin, true);
        sleep_us(2);
        gpio_put(pin, false);
    }
}

static void card_power_enable(void *unused, bool enabled)
{
    (void)unused;
    voltage_good_since_us = 0;
    clock_request(POWER_ARM_CLK, enabled);
}

static bool card_power_permitted(void *unused)
{
    (void)unused;
    return gpio_get(POWER_EN);
}

static void bus_translators_enable(void *unused, bool enabled)
{
    (void)unused;
    gpio_put(MCU_BUS_REQUEST, enabled);
}

static bool bus_translators_permitted(void *unused)
{
    (void)unused;
    return gpio_get(BUS_ENABLE);
}

static bool reset_delay_complete(void *unused)
{
    (void)unused;
    return card_power_good(NULL) && voltage_good_since_us &&
           time_us_64() - voltage_good_since_us >= 20000u;
}

static void reset_state(void *unused, pcmcia_reset_state_t state)
{
    (void)unused;
    if (state == PCMCIA_RESET_HIGH_Z) {
        gpio_set_dir(RESET, GPIO_IN);
    } else {
        output(RESET, state == PCMCIA_RESET_ASSERT_HIGH);
    }
}

static void set_line(void *unused, pcmcia_line_t line, bool high)
{
    (void)unused;
    static const unsigned pins[] = {24, 0, 26, 27, 28, 29, 30};
    if (line == PCMCIA_CE2_N) return; /* CE2# is fixed inactive in hardware. */
    if ((unsigned)line < sizeof pins / sizeof pins[0]) gpio_put(pins[line], high);
}

static void set_address(void *unused, uint16_t address)
{
    (void)unused;
    gpio_put_masked(0x00ffff00u, (uint32_t)address<<8);
}

static void set_data_output(void *unused, bool enabled)
{
    (void)unused;
    if (enabled) gpio_set_dir_out_masked(0xffu);
    else gpio_set_dir_in_masked(0xffu);
}

static void write_data(void *unused, uint8_t value)
{
    (void)unused;
    gpio_put_masked(0xffu,value);
}

static uint8_t read_data(void *unused)
{
    (void)unused;
    return (uint8_t)gpio_get_all();
}

static void set_data_to_card(void *unused, bool enabled)
{
    (void)unused;
    gpio_put(DATA_DIR, scp55_data_dir_level(enabled));
}

static void set_data_translator(void *unused, bool enabled)
{
    (void)unused;
    gpio_put(DATA_ENABLE, enabled);
}

static bool wait_n(void *unused)
{
    (void)unused;
    return gpio_get(WAIT_N);
}

static void delay_us(void *unused, uint32_t us)
{
    (void)unused;
    if (streaming_power_cache && us==1) {
        /* Preserve >=1 us setup/hold without microsecond timer quantization. */
        busy_wait_at_least_cycles(streaming_delay_cycles);
    } else sleep_us(us);
}

static void delay_ms(void *unused, uint32_t ms)
{
    (void)unused;
    sleep_ms(ms);
}

#ifdef SCP55_ENABLE_AUDIO_PROBE
static int (*normal_io_read)(void *, uint16_t, uint8_t *);
static int (*normal_io_write)(void *, uint16_t, uint8_t);
static uint32_t audio_delay_cycles;
static inline bool audio_bus_latches_ok(void)
{
    uint32_t high=sio_hw->gpio_hi_in;
    /* CD1/CD2 low, power latch and bus latch high. */
    return (high & ((1u<<1)|(1u<<2)|(1u<<3)|(1u<<15))) == ((1u<<3)|(1u<<15));
}
static inline void audio_bus_delay(void)
{
    busy_wait_at_least_cycles(audio_delay_cycles);
}
static int audio_io_cycle(void *context,uint16_t address,uint8_t *value,bool write)
{
    pcmcia_bitbang_t *driver=context;
    if(!driver->armed || !value || !audio_bus_latches_ok()) return -2;
    /* PC Card Standard 8.0 tables 4-24/25: IORD/IOWR >=165 ns,
     * address setup >=70 ns, write data setup/hold >=60/30 ns.
     * Each phase here is >=250 ns (plus GPIO/translator overhead).
     * Poll WAIT only after the strobe setup; allow another phase on release. */
    gpio_put(DATA_ENABLE,false);
    if(write) gpio_set_dir_out_masked(0xffu);
    else gpio_set_dir_in_masked(0xffu);
    gpio_put(DATA_DIR,scp55_data_dir_level(write));
    if(write) gpio_put_masked(0xffu,*value);
    gpio_put_masked(0x04ffff00u,(uint32_t)address<<8); /* address + REG# low */
    audio_bus_delay();
    gpio_put(DATA_ENABLE,true);
    gpio_put(24,false); /* CE1# */
    audio_bus_delay();
    unsigned strobe=write?30:29; /* IOWR# / IORD# */
    gpio_put(strobe,false);
    audio_bus_delay();
    int result=-1;
    for(unsigned waited=0;waited<200;waited++) {
        if(!audio_bus_latches_ok() || !card_power_good(NULL)) {result=-2;break;}
        if(gpio_get(WAIT_N)) {
            audio_bus_delay();
            if(!write) *value=(uint8_t)gpio_get_all();
            result=0;break;
        }
        audio_bus_delay();
    }
    gpio_put(strobe,true);
    audio_bus_delay();
    gpio_set_mask((1u<<24)|(1u<<26));
    gpio_put(DATA_ENABLE,false);
    gpio_set_dir_in_masked(0xffu);
    if(result==-2) pcmcia_bitbang_disarm(driver);
    return result;
}
static int audio_io_read(void *context,uint16_t address,uint8_t *value)
{
    if(!streaming_power_cache || (address!=0x331 && address!=0x336 && address!=0x337))
        return normal_io_read(context,address,value);
    return audio_io_cycle(context,address,value,false);
}
static int audio_io_write(void *context,uint16_t address,uint8_t value)
{
    if(!streaming_power_cache || address!=0x330)
        return normal_io_write(context,address,value);
    return audio_io_cycle(context,address,&value,true);
}
#endif

static void prepare_bus_gpio(void)
{
    /* Interlocks still cleared here. The MCU-facing side can be driven. */
    for (unsigned pin = 8; pin <= 23; ++pin) output(pin, false);
    const unsigned idle_high[] = {24, 26, 27, 28, 29, 30};
    for (unsigned i = 0; i < sizeof idle_high / sizeof idle_high[0]; ++i)
        output(idle_high[i], true);
    output(DATA_DIR, scp55_data_dir_level(false));
}

static int run_probe(scp55_config_t *config, bool play_note, int capture_source)
{
    (void)capture_source;
    if (!config) return -1;
    if (!scp55_voltage_probe_ready(SCP55_ADC_REFERENCE_MV, SCP55_CARD_ERROR_MV,
                                   SCP55_ALLOW_UNCOMPENSATED_PROBE))
        return -16; /* No power arm pulse before calibration. */
    scp55_diag_sample_t initial = scp55_board_sample();
    if (!scp55_diag_card_present(&initial) ||
        scp55_diag_evaluate(&initial)) return -2;

    prepare_bus_gpio();
    pcmcia_pins_t bus_pins = {
        .set_line = set_line,
        .set_address = set_address,
        .set_data_output = set_data_output,
        .write_data = write_data,
        .read_data = read_data,
        .set_data_to_card = set_data_to_card,
        .set_data_translator = set_data_translator,
        .wait_n = wait_n,
        .card_present = card_present,
        .card_power_good = card_power_good,
        .delay_us = delay_us,
        .delay_ms = delay_ms,
    };
    pcmcia_bitbang_t driver;
    if (pcmcia_bitbang_init(&driver, &bus_pins)) {
        scp55_board_safe_init();
        return -3;
    }
    pcmcia_power_pins_t power_pins = {
        .card_present = card_present,
        .card_5v_compatible = card_5v_compatible,
        .card_power_good = card_power_good,
        .master_clear_n = master_clear_n,
        .card_power_enable = card_power_enable,
        .card_power_permitted = card_power_permitted,
        .bus_translators_enable = bus_translators_enable,
        .bus_translators_permitted = bus_translators_permitted,
        .reset_delay_complete = reset_delay_complete,
        .reset_state = reset_state,
        .reset_translator_permitted = bus_translators_permitted,
        .delay_ms = delay_ms,
    };
    pcmcia_power_t power;
    if (pcmcia_power_init(&power, &power_pins, &driver)) {
        scp55_board_safe_init();
        return -4;
    }
    last_power_check_mv = 0;
    int result = pcmcia_power_start(&power);
    if (!result) {
        pcmcia_bus_t bus = pcmcia_bitbang_bus(&driver);
#ifdef SCP55_ENABLE_AUDIO_PROBE
        normal_io_read=bus.io_read;bus.io_read=audio_io_read;
        normal_io_write=bus.io_write;bus.io_write=audio_io_write;
#endif
#ifdef SCP55_ENABLE_NOTE_TEST
        if (play_note) {
            /* Keep bring-up failures separate from power/calibration errors. */
            result = scp55_bringup(&bus, config);
            if (result) result -= 20;
            if (!result && pcmcia_power_poll(&power)) result = -5;
            if (!result) {
                puts("Init OK: SCP-55 identity, COR readback, codec readback, MPU UART ACK");
#ifdef SCP55_ENABLE_AUDIO_PROBE
                if (capture_source <= -2) {
                    power_sample_us=0;
                    streaming_delay_cycles=(clock_get_hz(clk_sys)+999999u)/1000000u;
                    audio_delay_cycles=(clock_get_hz(clk_sys)+3999999u)/4000000u;
                    streaming_power_cache=true;
                    result = scp55_audio_live_run(&bus, config, &power,
                        capture_source != -3 && capture_source != -5, capture_source == -5 ? 0 : capture_source == -4 ? 60 : capture_source == -3 ? 600 : 12);
                    streaming_power_cache=false;
                } else if (capture_source >= 0) {
                    result = scp55_audio_probe(&bus, config, capture_source);
                } else
#endif
                {
                    result = scp55_note_test(&bus, config);
                    if (!result) puts("MIDI sent: 90 3C 40; 300 ms; 80 3C 00 (audio not measured)");
                    if (result) result -= 30;
                }
            }
        } else
#else
        (void)play_note;
#endif
        {
            result = scp55_wait_cis_stable(&bus);
            if (!result) result = scp55_identify(&bus, config);
        }
        if (!result && pcmcia_power_poll(&power)) result = -5;
    }
    pcmcia_power_stop(&power);
    scp55_board_safe_init();
    printf("Probe last checked CARD_mV=%u; result=%d; power off\r\n",
           last_power_check_mv, result);
#ifdef SCP55_ENABLE_AUDIO_PROBE
    if (capture_source >= 0) scp55_audio_probe_dump();
    if (capture_source <= -2) scp55_audio_live_dump();
#endif
    return result;
}

int scp55_board_read_cis(scp55_config_t *config)
{
    return run_probe(config, false, -1);
}

#ifdef SCP55_ENABLE_NOTE_TEST
int scp55_board_note_test(scp55_config_t *config)
{
    return run_probe(config, true, -1);
}
#endif
#ifdef SCP55_ENABLE_AUDIO_PROBE
int scp55_board_audio_probe(scp55_config_t *config, int source)
{
    scp55_audio_probe_clear();
    return run_probe(config, true, source);
}
#endif
#endif
