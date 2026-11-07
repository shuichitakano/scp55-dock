# Firmware

Firmware for the RP2354B-based SCP-55 Dock. The playback build supports USB and
DIN MIDI simultaneously, 44.1 kHz / 16-bit stereo audio, unity gain and BOOT
card-power control. Sources are in `rp2354b_diag/` and the C files in this directory.

## Build

Tested with Pico SDK 2.3.0, the Arm GNU toolchain, CMake and picotool 2.x.
Prepare the Pico SDK separately, including its required submodules such as
TinyUSB. Run the following commands from the repository root:

```sh
export PICO_SDK_PATH=/path/to/pico-sdk
export PICOTOOL=/path/to/picotool
sh firmware/build-rp2354b.sh passive
# Set your measured ADC reference in mV; 3320 is the supplied UF2's bench value.
SCP55_ADC_REFERENCE_MV=3320 sh firmware/build-rp2354b.sh usb-midi-bench
```

Build artifacts are written to `output/firmware/<profile>/`.
The `usb-midi-bench` profile omits measurement-error compensation.
Card power is not enabled if the ADC reference is left unset.

## Tests

```sh
sh firmware/run-host-tests.sh
```

The host suite checks power states, card identification, bus transactions, MIDI
merging, audio processing, button behavior and GPIO assignments. These tests
are separate from hardware validation.
