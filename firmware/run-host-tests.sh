#!/bin/sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
compiler=${CC:-cc}
flags='-std=c11 -Wall -Wextra -Werror -pedantic'

cd "$project_dir"
$compiler $flags -Ifirmware -fsyntax-only firmware/*.c
$compiler $flags -Ifirmware firmware/power_button.c firmware/test_power_button.c -o "$test_dir/button"
"$test_dir/button"
$compiler $flags -Ifirmware firmware/scp55.c firmware/test_core.c -o "$test_dir/core"
"$test_dir/core"
$compiler $flags -Ifirmware firmware/bringup.c firmware/scp55.c firmware/cs4231.c firmware/mpu401.c firmware/test_bringup.c -o "$test_dir/bringup"
"$test_dir/bringup"
$compiler $flags -Ifirmware firmware/audio_capture.c firmware/test_capture.c -o "$test_dir/capture"
"$test_dir/capture"
$compiler $flags -Ifirmware firmware/pcmcia_bitbang.c firmware/test_bitbang.c -o "$test_dir/bus"
"$test_dir/bus"
$compiler $flags -Ifirmware firmware/midi_router.c firmware/mpu401.c firmware/test_midi_router.c -o "$test_dir/midi"
"$test_dir/midi"
$compiler $flags -Ifirmware firmware/midi_merge.c firmware/test_midi_merge.c -o "$test_dir/merge"
"$test_dir/merge"
$compiler $flags -Ifirmware firmware/audio_ring.c firmware/test_audio_ring.c -o "$test_dir/ring"
"$test_dir/ring"
$compiler $flags -Ifirmware firmware/audio_resample.c firmware/audio_ring.c firmware/test_audio_resample.c -o "$test_dir/resample"
"$test_dir/resample"
$compiler $flags -Ifirmware firmware/audio_resample.c firmware/audio_ring.c firmware/test_audio_quality.c -lm -o "$test_dir/quality"
"$test_dir/quality"
$compiler $flags -Ifirmware firmware/card_power.c firmware/pcmcia_bitbang.c firmware/test_card_power.c -o "$test_dir/power"
"$test_dir/power"
$compiler $flags -Ifirmware firmware/diagnostic.c firmware/test_diagnostic.c -o "$test_dir/diagnostic"
"$test_dir/diagnostic"
PYTHONDONTWRITEBYTECODE=1 python3 firmware/test_i2s_pio.py
PYTHONDONTWRITEBYTECODE=1 python3 hardware/test_interlock.py
PYTHONDONTWRITEBYTECODE=1 python3 hardware/verify_diag_pinmap.py
$compiler $flags -Ifirmware firmware/voltage_guard.c firmware/test_voltage_guard.c -o "$test_dir/voltage"
"$test_dir/voltage"
printf 'All host tests passed.\n'
