cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD scp55_rp2354b CACHE STRING "Rev.A RP2354B board")
set(PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_LIST_DIR}/boards CACHE STRING "Local board headers")
if(NOT DEFINED ENV{PICO_SDK_PATH})
    message(FATAL_ERROR "Set PICO_SDK_PATH to a Pico SDK 2.x checkout")
endif()
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(scp55_rp2354b_diag C CXX ASM)
pico_sdk_init()

add_executable(scp55_rp2354b_diag
    main.c
    power_button_hw.c
    ../power_button.c
    board.c
    pinplan.c
    ../diagnostic.c
    ../voltage_guard.c
)
pico_generate_pio_header(scp55_rp2354b_diag ${CMAKE_CURRENT_LIST_DIR}/scp55_i2s.pio)
option(SCP55_ENABLE_CIS_PROBE "Enable explicit powered CIS read command" OFF)
option(SCP55_ENABLE_NOTE_TEST "Enable explicit powered single-note test (includes CIS probe)" OFF)
option(SCP55_ENABLE_AUDIO_PROBE "Enable bounded experimental audio capture" OFF)
option(SCP55_ENABLE_USB_MIDI "Enable USB MIDI composite device and automatic playback" OFF)
if(SCP55_ENABLE_USB_MIDI)
    set(SCP55_ENABLE_AUDIO_PROBE ON)
    target_sources(scp55_rp2354b_diag PRIVATE usb_descriptors.c ../midi_router.c ../midi_merge.c)
    target_compile_definitions(scp55_rp2354b_diag PRIVATE
        SCP55_ENABLE_USB_MIDI=1 CFG_TUD_MIDI=1 CFG_TUD_MIDI_RX_BUFSIZE=512 CFG_TUD_MIDI_TX_BUFSIZE=64
        PICO_STDIO_USB_USE_DEFAULT_DESCRIPTORS=0)
endif()
if(SCP55_ENABLE_AUDIO_PROBE)
    set(SCP55_ENABLE_NOTE_TEST ON)
    target_compile_definitions(scp55_rp2354b_diag PRIVATE SCP55_ENABLE_AUDIO_PROBE=1)
    target_sources(scp55_rp2354b_diag PRIVATE ../audio_capture.c ../audio_ring.c
        ../audio_resample.c audio_probe.c audio_live.c)
endif()
if(SCP55_ENABLE_NOTE_TEST)
    set(SCP55_ENABLE_CIS_PROBE ON)
    target_compile_definitions(scp55_rp2354b_diag PRIVATE SCP55_ENABLE_NOTE_TEST=1)
    target_sources(scp55_rp2354b_diag PRIVATE
        ../bringup.c ../cs4231.c ../mpu401.c)
endif()
if(SCP55_ENABLE_CIS_PROBE)
    target_compile_definitions(scp55_rp2354b_diag PRIVATE SCP55_ENABLE_CIS_PROBE=1)
    target_sources(scp55_rp2354b_diag PRIVATE
        ../card_power.c
        ../pcmcia_bitbang.c
        ../scp55.c
    )
endif()
target_include_directories(scp55_rp2354b_diag PRIVATE ..)
target_compile_options(scp55_rp2354b_diag PRIVATE -Wall -Wextra -Werror)
target_link_libraries(scp55_rp2354b_diag pico_stdlib pico_stdio_usb hardware_pio hardware_uart hardware_dma hardware_clocks hardware_adc)
pico_enable_stdio_usb(scp55_rp2354b_diag 1)
pico_enable_stdio_uart(scp55_rp2354b_diag 0)
pico_add_extra_outputs(scp55_rp2354b_diag)

# 0 keeps active probing locked; values must come from bench calibration.
set(SCP55_ADC_REFERENCE_MV 0 CACHE STRING "Measured ADC reference, millivolts")
set(SCP55_CARD_ERROR_MV 0 CACHE STRING "Verified worst-case card measurement error, millivolts")
option(SCP55_ALLOW_UNCOMPENSATED_PROBE "Bench only: use measured reference without an error bound" OFF)
target_compile_definitions(scp55_rp2354b_diag PRIVATE
    SCP55_ALLOW_UNCOMPENSATED_PROBE=$<BOOL:${SCP55_ALLOW_UNCOMPENSATED_PROBE}>
    SCP55_ADC_REFERENCE_MV=${SCP55_ADC_REFERENCE_MV}
    SCP55_CARD_ERROR_MV=${SCP55_CARD_ERROR_MV})

set(SCP55_CAPTURE_RATE 44100 CACHE STRING "Live stereo capture rate")
target_compile_definitions(scp55_rp2354b_diag PRIVATE SCP55_CAPTURE_RATE=${SCP55_CAPTURE_RATE})
