# Bench measurements

First characterisation of the second-generation sensor board, 26 August 2026.
Configuration: IIS3DWB at 26.667 kHz ODR, ±8 g range, 256-sample windows, SPI Mode 0.

These are **characterisation** results — they describe how the hardware behaves, not how
faults are diagnosed. Raw serial logs and a shaker comparison against a reference
accelerometer will be added as that work is done.

## Results

| Measurement | Result |
|---|---|
| Sensor identification (`WHO_AM_I`) | `0x7B`, stable across 7/7 read cycles |
| SPI mode | Mode 0 only; Mode 3 returns `0x00` — the part does not respond in Mode 3 |
| Noise floor at rest | ~1.0 mg RMS (256-sample window) |
| Gravity, face down | Z ≈ −957 mg |
| Gravity, inverted | Z ≈ +1018 mg |
| Impact response (tap test) | RMS rises from ~1 mg to 77 / 155 / 412 / 900 mg depending on strength |
| Settling after impact | Returns to noise floor in under 1 s |
| On-die temperature | Stable, 26.5–26.9 °C |

## What these numbers mean

The sign of the gravity reading flips correctly when the board is inverted and the
magnitude sits close to 1 g in both orientations, which confirms axis orientation and
scale factor. The ~1 mg RMS noise floor with a 256-sample window is consistent with the
datasheet noise density over this bandwidth, and it sets the practical detection limit:
vibration below roughly 1 mg RMS is not distinguishable from the sensor's own noise.

The tap test spans nearly three orders of magnitude without saturating and settles back
within a second, showing the acquisition chain has usable dynamic range and no ringing
that would smear later spectral analysis.

## Not yet measured

- Frequency response against a shaker with a calibrated reference accelerometer
- Behaviour across the full −40…+105 °C range
- Long-term drift
- Power consumption in duty-cycled operation

## Reproducing

The sketches under [`../firmware/tests/`](../firmware/tests/) produce these readings:
`sensor_id` verifies the SPI link and part identity, `acquisition` streams mean, RMS and
temperature over the serial port.
