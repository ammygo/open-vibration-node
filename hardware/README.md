# Hardware

KiCad design files for the vibration sensing node.

Licence: CERN-OHL-W-2.0 — see [LICENSE.txt](LICENSE.txt).

## Contents

Nothing published yet. This directory will hold:

- `node/` — KiCad project: schematics, PCB layout, netlist
- `bom/` — bill of materials with distributor part numbers
- `production/` — Gerber files, drill files, pick-and-place data
- `datasheets/` — datasheets for the main components used

## Design notes

The node carries a low-noise triaxial MEMS accelerometer, an nRF52840-class MCU with an
SX1262 LoRa transceiver, and a power stage supporting either a primary lithium cell or a
solar plus LiFePO4 arrangement.

Two constraints shape the layout: the accelerometer needs a mechanically stiff, direct
coupling path to the mounting surface, and the radio needs a clean ground plane and
keep-out area around the antenna. These pull in opposite directions on a small board, and
the layout choices made to resolve that will be documented here as the design settles.
