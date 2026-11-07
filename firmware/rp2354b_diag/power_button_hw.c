/* BOOTSEL read adapted from Raspberry Pi pico-examples/picoboard/button.
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd. SPDX-License-Identifier: BSD-3-Clause
 * core1 and flash-reading DMA must remain unused during this read.
 */
#include "pico/stdlib.h"
#include "hardware/sync.h"
#include "hardware/clocks.h"
#include "hardware/structs/ioqspi.h"
#include "hardware/structs/sio.h"
#include "power_button.h"
#include "power_button_hw.h"
static scp55_power_button_t button;
static uint64_t next_poll;
static uint32_t settle_cycles;
static bool __no_inline_not_in_flash_func(read_bootsel)(void)
{
    uint32_t flags=save_and_disable_interrupts();
    uint32_t saved=ioqspi_hw->io[1].ctrl;
    hw_write_masked(&ioqspi_hw->io[1].ctrl,
        GPIO_OVERRIDE_LOW<<IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
        IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);
    /* 1 us settling, inlined assembly only while flash CS is Hi-Z. */
    busy_wait_at_least_cycles(settle_cycles);
    bool down=!(sio_hw->gpio_hi_in&SIO_GPIO_HI_IN_QSPI_CSN_BITS);
    ioqspi_hw->io[1].ctrl=saved;
    restore_interrupts(flags);
    return down;
}
void scp55_power_button_hw_init(void)
{
    scp55_power_button_init(&button);
    settle_cycles=(clock_get_hz(clk_sys)+999999u)/1000000u;
    next_poll=0;
}
bool scp55_power_button_service(void)
{
    uint64_t now=time_us_64();
    if(now>=next_poll) {
        next_poll=now+10000;
        scp55_power_button_update(&button,read_bootsel(),now/1000);
    }
    return button.on;
}
void scp55_power_button_standby(void) {scp55_power_button_off(&button);}
