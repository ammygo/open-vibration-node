# Enclosure

3D-printable sealed enclosure for the vibration sensing node.

Licence: CERN-OHL-W-2.0 (same terms as `../hardware`); documentation CC-BY-SA-4.0.

## Contents

Nothing published yet. This directory will hold:

- `src/` — parametric source models (STEP)
- `stl/` — exported meshes ready to print
- `print-profiles/` — tested slicer profiles per material

## Design notes

The enclosure has to do three things at once: transmit vibration faithfully from the
mounting surface to the sensor, keep moisture and dust out, and not shield the antenna.

Planned approach: a stiff mounting boss coupling the PCB's sensor area directly to the
machine surface, a gasket groove for an EPDM or silicone O-ring cord (printed gaskets leak
through layer lines and are not used for sealing), a membrane-vented port to equalise
pressure without admitting water, and a radio-transparent section over the antenna.

Materials: ASA for outdoor and UV exposure, PC or PC-FR where surface temperatures are
high. Both need a heated chamber to print without warping — printer requirements will be
documented alongside the profiles.

Threaded brass inserts are used for all fasteners; printed threads do not survive repeated
opening or vibration.
