# Rev.A manufacturing and assembly data

These files accompany `../scp55-reva-73x115-gerbers.zip`: the 2026-09-30
Rev.A design, 73 x 115 mm, four copper layers, design thickness 1.6 mm.
The later schematic readability update does not change this design.

## Files

| File | Purpose |
| --- | --- |
| `bom-jlcpcb.csv` | JLCPCB upload BOM: Comment, Designator, Footprint, LCSC Part #. |
| `cpl-jlcpcb.csv` | JLCPCB placement upload: Designator, Mid X, Mid Y, Layer, Rotation. Coordinates are in mm. |
| `bom.csv` | Full procurement BOM, including manufacturer part numbers, LCSC IDs, historical stock snapshots, supplier links and hand-soldered parts. |
| `positions.csv` | Full KiCad position export, including top and bottom components. Reference only; use `cpl-jlcpcb.csv` for the JLC upload. |

## Assembly scope

The JLC BOM has 38 part groups covering 149 top-side placements. The CPL has
exactly the same 149 designators. J1, J3, J4 and J5 are excluded from both upload
files and must be fitted by hand on the bottom side. The full BOM and position
export each cover 153 components. Test pads and mounting holes are not assembly
placements.

The CPL preserves KiCad rotation angles. Check the assembly provider's preview
against the physical parts and the board before approving production; component
libraries can use different zero-angle conventions. Coordinates share the Gerber
origin. Negative Y coordinates are retained from the export.

## Parts and orientation notes

- **J1:** salvaged PC Card connector; no orderable replacement MPN is specified.
  Verify the physical pin mapping and continuity before first power-up.
- **J3:** Akizuki PH-1x40SG, item 100167; cut to three positions.
- **J4:** KDJ03-5, Akizuki item 109565.
- **J5:** ST-005-G, Akizuki item 102460. The earlier SDS-50J and SJ1-3523N
  connectors do not match the current manufacturing design.
- **U1/U6:** Nexperia 74LVC4245APW,118 / C6091. Retain the specified pinout.
- **U22:** AP7361C-33ER-13 / C3743528. Retain the ER pinout.
- **L1:** align the winding-direction mark with pad 2 (1V1/COUT), on the left
  when viewed from the top. Do not treat its orientation as arbitrary.
- **D2/D3:** cathode K/GND to the left and anode A toward the resistor on the
  right, in top view. Both LEDs run parallel to the board edge. D2/PWR is near
  USB; D3/MIDI is behind the DIN connector. Check physical polarity marks rather
  than assuming the manufacturer's pin numbering matches KiCad: the KT-0805Y
  manufacturer numbering differs from the KiCad LED pad convention.
- **R7/R8:** despite the word "option" in their values, these are fitted 0-ohm
  parts in the current BOM; they are not DNP.
- **R4/R5:** use the specified 0.1% parts. **C28/C29:** C0G, 5%.
- **C30/C31/C32:** 0402, 4.7uF/10V, C23733. **R73:** 0402, 33 ohm, C25105.
  **C35:** 0603, 4.7uF/16V, C19666. Retain the specified regulator components.
- Check IC pin 1, D1 polarity, L1's mark, LED polarity and connector orientation
  in the assembly preview.

Stock counts, Basic/Extended classifications and availability in `bom.csv` are
historical snapshots from the recorded September 2026 checks, not current
availability or reserved inventory. In particular, U23 and J2 relied on JLC stock
at that time while LCSC showed zero stock. Confirm allocation before ordering.

## Verification

The published JLC BOM/CPL designator sets are identical and contain no duplicates.
The full BOM and full position-export designator sets are also identical.
The original manufacturing export check covered all 153 placements and matched
PTH/NPTH hole positions and sizes to the production board within 0.00101 mm.
The Gerber outline was checked as 73 x 115 mm.

Hardware manufacturing data: CERN-OHL-P-2.0. See the repository's `LICENSE.md`.
