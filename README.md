# SCP-55 Dock

A standalone adapter for the Roland SCP-55 PC Card sound module. It receives
USB MIDI and DIN MIDI and plays audio through the board's LINE OUT, without a
PC Card slot. The board uses an RP2354B microcontroller.

Rev.A: 73 × 115 mm, four layers, 1.6 mm design thickness.

## Downloads

- [Schematic PDF](output/pdf/scp55-reva.pdf)
- [Manufacturing Gerbers, including PTH/NPTH drill files](output/manufacturing/scp55-reva-73x115-gerbers.zip)
- [Playback firmware (UF2)](output/firmware/usb-midi-bench/scp55-usb-midi-bench.uf2)
- [Passive diagnostic firmware (UF2)](output/firmware/passive/scp55-passive.uf2)
- [Firmware source and build instructions](firmware/README.md)
- [SHA-256 checksums](SHA256SUMS)

## Flashing and use

1. Disconnect USB. For initial board checks, also remove the SCP-55 card.
2. Hold the board's BOOT button while connecting USB. Release it when the
   BOOTSEL drive appears, then copy the UF2 file to that drive.
3. On a newly assembled board, start with the passive diagnostic firmware and
   check board voltages and card detection. This firmware does not play audio.
4. With USB disconnected, insert the SCP-55. Flash the playback firmware using
   the same BOOTSEL procedure. The card starts automatically on power-up.
5. Send MIDI to **SCP-55 USB MIDI** over USB, or to the board's DIN MIDI IN.
   Connect LINE OUT to an amplifier or powered speakers and adjust volume there.

USB and DIN inputs are always active; no manual input selection is needed.
Audio capture and output use 44.1 kHz, 16-bit stereo.
Hold BOOT for about two seconds to turn card power off. Release it, then press
briefly to turn card power back on. The microcontroller, USB interface and DAC
remain powered while the card is off.

## Tested configuration and limitations

USB and DIN playback, stereo output and BOOT card-power control have been tested
on hardware. A second board also passed USB playback and left/right separation
checks. There is no master-volume UI.

The supplied playback UF2 uses a 3320 mV ADC reference and a bench configuration
without measurement-error compensation. Measure the reference voltage and
rebuild for another board. This adapter is specifically designed for the SCP-55.
Verify the pin mapping of the salvaged J1 connector when assembling a board.

The schematic and Gerbers are dated 2026-09-30; the playback UF2 was built on
2026-10-19. This repository does not include editable KiCad files, enclosure
files, development logs or SCP-55 ROM data.

This is an independent project, unaffiliated with Roland.

## License

A reuse license for the original project material has not yet been selected.
Third-party copyright and license notices are retained in their source files.
The Raspberry Pi-derived BOOTSEL implementation is subject to the
[BSD-3-Clause terms](licenses/pico-sdk-BSD-3-Clause.txt).
