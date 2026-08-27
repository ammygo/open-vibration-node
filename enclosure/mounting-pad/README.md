# Mounting pad

A threaded pad that screws into a machine housing and carries the sensor board.

> **Version 1 — known limitation.** This revision has a flat top face and **no pocket for
> the accelerometer**. The IIS3DWB package sits about 0.9 mm below the underside of the
> sensor board, so it will contact a flat pad. A recess of Ø8–10 mm and about 2 mm depth in
> the centre of the top face is required. Existing pads can be drilled by hand; the next
> revision will include the pocket in the source model.

## Dimensions

| Feature | Size |
|---|---|
| Threaded stud | M6 × 1.0, 9 mm long |
| Hex section | 13 mm across flats, 6 mm tall |
| Top platform | Ø30 mm, 3 mm thick |

## Why a pad at all

Vibration measurement is only as good as the mechanical path between the machine surface
and the sensing element. Magnets and adhesive tape introduce compliance that rolls off the
higher frequencies — precisely the 1–6 kHz region where early bearing faults appear. A
threaded pad screwed into the housing gives a stiff, repeatable coupling, and the flat
platform lets the sensor board be bonded across its full area rather than resting on
standoffs.

The hex section exists so the pad can be tightened with an ordinary spanner without
touching the platform surface.

## Files

| File | Purpose |
|---|---|
| `src/mounting_pad_v1.scad` | Parametric source (OpenSCAD) — edit dimensions here |
| `export/mounting_pad_v1.step` | STEP export for CAD work |
| `export/mounting_pad_v1.stl` | Mesh for 3D printing |

The modelled thread is a printable approximation, not a metrological thread form. For
metal pads, cut a real M6 thread; for printed prototypes it holds well enough in a tapped
housing.

Licence: CERN-OHL-W-2.0, as for all hardware in this repository.
