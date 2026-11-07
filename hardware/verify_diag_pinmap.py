#!/usr/bin/env python3
"""Catch drift between the board diagnostic and the schematic GPIO table."""

import csv
from pathlib import Path
import re

root = Path(__file__).resolve().parent
mapping = {row["signal"]: int(row["gpio"])
           for row in csv.DictReader((root / "gpio-reva.csv").open())}
assert sorted(mapping.values()) == list(range(48)), "48 GPIOs must be assigned exactly once"
source = (root.parent / "firmware/rp2354b_diag/board.c").read_text()
for signal in ("MASTER_CLEAR_N", "CD1_HIGH", "CD2_HIGH", "DATA_ENABLE", "POWER_ARM_CLK", "POWER_EN",
               "CARD_VMON", "MCU_BUS_REQUEST", "BUS_ENABLE"):
    match = re.search(rf"\b{signal}\s*=\s*(\d+)\b", source)
    assert match, f"diagnostic pin missing: {signal}"
    assert int(match.group(1)) == mapping[signal], signal
for bit in range(8):
    assert mapping[f"MCU_D{bit}"] == bit
for bit in range(16):
    assert mapping[f"MCU_A{bit}"] == 8 + bit
control = re.search(r"static const unsigned pins\[\] = \{([^}]+)\}", source)
assert control
actual = tuple(int(value) for value in control.group(1).split(","))
expected = tuple(mapping[name] if name != "CE2" else 0 for name in (
    "MCU_CE1_N", "CE2", "MCU_REG_N", "MCU_OE_N", "MCU_WE_N",
    "MCU_IORD_N", "MCU_IOWR_N"))
assert actual == expected, "control line mapping"
assert mapping["MCU_RESET"] == 31
assert mapping["MCU_WAIT_N"] == 32
assert mapping["MCU_DATA_DIR"] == 38
pinplan = (root.parent / "firmware/rp2354b_diag/pinplan.c").read_text()
for pin_name, signal in (("MIDI_ACTIVITY", "MIDI_ACTIVITY"),
                         ("MIDI_RX", "MIDI_DIN_RX"),
                         ("I2S_BCLK", "I2S_BCLK"),
                         ("I2S_LRCLK", "I2S_LRCLK"),
                         ("I2S_DOUT", "I2S_DOUT")):
    match = re.search(rf"\b{pin_name}\s*=\s*(\d+)\b", pinplan)
    assert match and int(match.group(1)) == mapping[signal], signal
print("RP2354B diagnostic pins match Rev.A GPIO table")

assert 'gpio_put(DATA_DIR, scp55_data_dir_level(enabled))' in source
assert 'output(DATA_DIR, scp55_data_dir_level(false))' in source
