# Hardware

Two 28 mm round boards, joined by seven wires with the battery between them.

Licence: CERN-OHL-W-2.0 — see [LICENSE.txt](LICENSE.txt).

| Path | Board | Contents |
|---|---|---|
| `sensor-board/` | IIS3DWB accelerometer, bottom layer | KiCad board, gerbers, BOM, pick-and-place |
| `radio-board/` | RAK3112 module (ESP32-S3 + SX1262), LDO | KiCad board, gerbers, BOM, CPL, pick-and-place |

The `production/` files under each board are the sets actually sent for fabrication and
assembly — not idealised exports. Boards from these files have been manufactured and the
sensor board verified on the bench ([measurements](../docs/measurements.md)).

## Inter-board wiring

`GND`, `3V3`, `SPI_CS`, `SPI_SCK`, `SPI_MISO`, `SPI_MOSI`, `INT1`.

Pad order is identical on both boards, so the wires run straight across without crossing.

## Layout constraints worth knowing

Two requirements pull against each other on a 28 mm board, and the layout is the
compromise between them:

- The accelerometer needs a **short, stiff mechanical path** to the mounting surface,
  which is why it sits on the bottom layer facing the measured surface rather than on top.
- The radio needs a **clean ground plane and antenna keep-out**, which is why the module
  sits on the top layer with the antenna facing away from the mounted metal.

Splitting these onto two boards resolves the conflict; the cost is the seven-wire harness
and the assembly step that goes with it.

## Before you reorder these boards

Run [`../tools/check_handedness.py`](../tools/README.md) against the sensor board. A
mirrored bottom-layer footprint passes design rule checking and fabricator review and
still cannot be assembled — that mistake cost three revisions here.

## Bill of materials

The BOM files carry LCSC part numbers where the boards were assembled with them, so the
component selection can be reproduced directly. Prices, suppliers and order references are
deliberately not included.
