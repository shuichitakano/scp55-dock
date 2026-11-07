#ifndef SCP55_RP2354B_BOARD_H
#define SCP55_RP2354B_BOARD_H

/* RP2354B is the 48-GPIO RP2350B die with 2 MB W25Q16JVWI in package.
 * The in-package flash uses the same Winbond command set as Pico 2's
 * stage-2 configuration. No Pico 2 LED, SMPS, or VBUS pins are assumed. */
pico_board_cmake_set(PICO_PLATFORM, rp2350)
#define PICO_RP2350A 0
#define PICO_BOOT_STAGE2_CHOOSE_W25Q080 1
pico_board_cmake_set_default(PICO_FLASH_SIZE_BYTES, (2 * 1024 * 1024))
#ifndef PICO_FLASH_SIZE_BYTES
#define PICO_FLASH_SIZE_BYTES (2 * 1024 * 1024)
#endif

#endif
