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

### What the KiCad sources are and are not

**The gerbers are the authoritative artifact** — they are what was manufactured. The KiCad
files are the design sources behind them, and on both boards they differ from the gerbers
in ways worth knowing before you rebuild:

- **Sensor board** — fully routed, but one CS track and its via sit about 0.1 mm from
  where the manufactured gerbers place them. The final clearance fix was applied in the
  export tool rather than back-annotated to KiCad. Electrically identical; regenerating
  gerbers from KiCad will not produce a byte-identical set.
- **Radio board** — carries **placement and netlist only, with no routing**. The routing
  was done in the export tool and exists solely in the gerbers. Opening this file will
  show an unrouted board; that is the actual state of the source, not a damaged file.

If you intend to modify either design, start from the gerbers for what was built and treat
the KiCad files as the placement reference.

![Both boards, both sides](../docs/images/boards-both-sides.jpg)

*Sensor board (bottom row) and radio board (top row), both sides. The accelerometer sits
on the underside of the sensor board; the RAK3112 module on the top of the radio board.*

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
