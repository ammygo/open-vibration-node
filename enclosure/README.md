# Enclosure and mechanical parts

Licence: CERN-OHL-W-2.0 (same terms as `../hardware`); documentation CC-BY-SA-4.0.

| Path | Contents |
|---|---|
| `mounting-pad/` | Threaded pad coupling the sensor board to the machine surface — published |
| `housing/` | Sealed enclosure — not yet published |

## Design notes

The enclosure has to do three things at once: transmit vibration faithfully from the
mounting surface to the sensor, keep moisture and dust out, and not shield the antenna.

Planned approach: a stiff mounting path from the pad through the board to the sensor, a
gasket groove for an EPDM or silicone O-ring cord (printed gaskets leak through layer
lines and are not used for sealing), a membrane-vented port to equalise pressure without
admitting water, and a radio-transparent section over the antenna.

Materials: ASA for outdoor and UV exposure, PC or PC-FR where surface temperatures are
high. Both need a heated chamber to print without warping — printer requirements will be
documented alongside the profiles.

Threaded brass inserts are used for all fasteners; printed threads do not survive repeated
opening or vibration.
