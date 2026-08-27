# Bench tests

Minimal Arduino sketches used to bring up and verify the sensor board. They are
deliberately simple — no filtering, no thresholds, no interpretation — so that anyone
rebuilding this hardware can confirm the basics work before adding anything on top.

| Sketch | Purpose |
|---|---|
| `sensor_id/` | Reads `WHO_AM_I` in SPI Mode 0 and Mode 3, and checks whether MISO floats. Use this first when a board does not respond. |
| `acquisition/` | Configures the IIS3DWB (26.667 kHz ODR, ±8 g, BDU), reads 256-sample windows and prints mean, RMS and die temperature over serial. |
| `bench_demo/` | Full self-check sequence: identity, configuration read-back, gravity vector and noise floor, printed as a readable report. |

Wiring as used on the current boards: `SCK` 13, `MISO` 10, `MOSI` 11, `CS` 12.
Expected `WHO_AM_I` value is `0x7B`. The part responds in SPI Mode 0 only.

`CTRL1_XL` must be written as `0xAC` for 26.667 kHz at ±8 g. The value `0x2C` is an
undocumented combination that the part accepts while running at roughly 105 Hz — it cost
us a published measurement before it was caught, so these sketches now read the register
back and report what the part is actually doing. See the
[errata](../../docs/measurements.md#errata-sensor-was-running-at-the-wrong-rate).

Results from these sketches: [`../../docs/measurements.md`](../../docs/measurements.md).
