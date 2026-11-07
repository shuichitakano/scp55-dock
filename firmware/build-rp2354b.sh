#!/bin/sh
# Build without SDK downloads, then convert with a locally installed picotool 2.x.
set -eu
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
profile=${1:-passive}
bench=OFF
audio=OFF
usb_midi=OFF
case "$profile" in
    passive) cis=OFF; note=OFF ;;
    cis) cis=ON; note=OFF ;;
    note) cis=ON; note=ON ;;
    cis-bench) cis=ON; note=OFF; bench=ON ;;
    note-bench) cis=ON; note=ON; bench=ON ;;
    usb-midi-bench) cis=ON; note=ON; bench=ON; audio=ON; usb_midi=ON ;;
    live-bench) cis=ON; note=ON; bench=ON; audio=ON ;;
    audio-bench) cis=ON; note=ON; bench=ON; audio=ON ;;
    *) printf 'Usage: sh firmware/build-rp2354b.sh [passive|cis|note|cis-bench|note-bench|audio-bench|live-bench|usb-midi-bench]\n' >&2; exit 2 ;;
esac
: "${PICO_SDK_PATH:?Set PICO_SDK_PATH to the local Pico SDK checkout}"
: "${PICOTOOL:?Set PICOTOOL to a local picotool 2.x executable}"
"$PICOTOOL" version
build_dir="$project_dir/build/firmware/$profile"
output_dir="$project_dir/output/firmware/$profile"
reference=${SCP55_ADC_REFERENCE_MV:-0}
error=${SCP55_CARD_ERROR_MV:-0}
# Passive artifacts always use nominal display only.
if [ "$profile" = passive ]; then reference=0; error=0; fi
cmake -S "$project_dir/firmware/rp2354b_diag" -B "$build_dir" \
    -DPICO_NO_PICOTOOL=1 \
    -DSCP55_CAPTURE_RATE="${SCP55_CAPTURE_RATE:-44100}" \
    -DSCP55_ENABLE_USB_MIDI="$usb_midi" \
    -DSCP55_ALLOW_UNCOMPENSATED_PROBE="$bench" \
    -DSCP55_ENABLE_AUDIO_PROBE="$audio" \
    -DSCP55_ENABLE_CIS_PROBE="$cis" -DSCP55_ENABLE_NOTE_TEST="$note" \
    -DSCP55_ADC_REFERENCE_MV="$reference" -DSCP55_CARD_ERROR_MV="$error"
cmake --build "$build_dir" --parallel 6
mkdir -p "$output_dir"
artifact="scp55-$profile"
cp "$build_dir/scp55_rp2354b_diag.elf" "$output_dir/$artifact.elf"
"$PICOTOOL" uf2 convert "$output_dir/$artifact.elf" "$output_dir/$artifact.uf2" --platform rp2350
"$PICOTOOL" info -a "$output_dir/$artifact.uf2"
printf 'Built %s (ADC reference=%s mV, error=%s mV)\n' "$output_dir/$artifact.uf2" "$reference" "$error"
