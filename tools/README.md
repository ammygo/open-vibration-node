# Tools

## `check_handedness.py` — catching mirrored footprints on the bottom layer

A mirrored footprint is one of the few PCB errors that survives every normal check.
Design rule checking passes, the fabricator's review passes, the boards arrive looking
perfect — and the chip cannot be assembled correctly, because no rotation will ever bring
its pins onto the right pads. In this project that mistake killed three board revisions
before it was understood.

This script catches it before the order is placed.

### The rule, in plain language

A chip lying lands-up on a table looks exactly like the datasheet **bottom view**. Lifting
it straight up onto the underside of a board changes nothing about that arrangement.

Therefore: for a bottom-mounted part, the pad positions in world coordinates must match
the datasheet bottom view **up to a single common rotation** — 0°, 90°, 180° or 270°.
If no rotation matches, the footprint is mirrored, and no amount of rotating in the
assembly file will fix it.

Understanding this rule matters more than running the script. The script only automates
the comparison.

### What it does

Reads pad coordinates straight from the KiCad `.kicad_pcb` file, converts them into world
coordinates, and compares them against the datasheet's bottom-view land positions at each
of the four rotations, reporting the worst-case pad error for each.

### Example output

Current sensor board — correct footprint:

```
================================================================
HANDEDNESS CHECK: bottom-v5
   rotation   0 deg: max pad error = 2.9369 mm
   rotation  90 deg: max pad error = 2.0767 mm
   rotation 180 deg: max pad error = 0.0000 mm
   rotation 270 deg: max pad error = 2.0767 mm
   >>> PASS: pads = datasheet BOTTOM VIEW rotated 180 deg
   >>> chip is PLACEABLE; CPL rotation must be 180 deg
```

One rotation lands at zero error — the footprint is correct, and the assembly file must
use that same rotation. On a mirrored footprint all four rotations stay far from zero and
the script reports `FAIL: FOOTPRINT IS MIRRORED`; that is the outcome that would have
saved three board revisions here.

### Usage

```
python check_handedness.py path/to/board.kicad_pcb
```

No dependencies beyond the Python standard library.

### Adapting it to another part

Two things are specific to this project and need editing for a different design:

- `DS_BOTTOM` — the land coordinates read from the datasheet's package drawing. Replace
  with the values for your part.
- The board-origin offsets in the world-coordinate conversion (`- 150.0`, `- 100.0`) —
  set these to your board's origin.

The footprint filter currently selects `LGA-14`; change it to match your package name.
