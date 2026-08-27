# Uplink payload specification — DRAFT v0

> **Status: draft for discussion.** Nothing here is implemented yet. The format will be
> versioned from the first real transmission, and this document will track it. Review and
> criticism are welcome — open an issue.

## Goals

- Fit comfortably inside LoRa duty-cycle and airtime limits: **24 bytes maximum**, so a
  single uplink carries a complete measurement summary even at slow spreading factors.
- Carry **results, not waveforms**: the node runs FFT and envelope analysis locally and
  transmits a spectral summary. Raw data never has to leave the device.
- Be **self-describing enough to evolve**: an explicit version field, so decoders can
  support mixed fleets.
- Be trivially implementable by third parties: fixed offsets, no compression, integer
  fields with documented scale factors.

## Proposed layout (19 of 24 bytes used)

| Offset | Size | Field | Encoding |
|---|---|---|---|
| 0 | 1 | Version + message type | high nibble: protocol version (0); low nibble: type (0 = periodic summary) |
| 1 | 1 | Sequence number | uint8, wraps at 255 |
| 2–3 | 2 | RMS acceleration | uint16, 0.1 mg units (0 – 6553.5 mg) |
| 4–5 | 2 | Peak acceleration | uint16, 0.1 mg units |
| 6–8 | 3 | Spectral peak 1 | frequency uint16 (Hz) + amplitude uint8 (0.5 dB steps re 1 mg) |
| 9–11 | 3 | Spectral peak 2 | same encoding |
| 12–14 | 3 | Spectral peak 3 | same encoding |
| 15–16 | 2 | Die temperature | int16, 0.1 °C units |
| 17 | 1 | Battery voltage | uint8, 20 mV steps from 2.0 V (covers 2.00 – 7.10 V) |
| 18 | 1 | Status flags | bitfield, see below |
| 19–23 | 5 | Reserved | zero-filled; candidates: ISO-style band energies, event counters |

### Status flags

| Bit | Meaning |
|---|---|
| 0 | Sensor identified (`WHO_AM_I` = 0x7B this cycle) |
| 1 | Configuration read-back matched what was written |
| 2 | Clipping detected in the analysis window |
| 3 | Watchdog reset occurred since previous uplink |
| 4 | Battery below warning threshold |
| 5 | First uplink after boot |
| 6–7 | Reserved |

Bits 0 and 1 exist because of a lesson this project already paid for: the sensor once ran
at 105 Hz while the code believed 26.667 kHz ([errata](measurements.md#errata-sensor-was-running-at-the-wrong-rate)).
A node should assert its own configuration in every message it sends.

## Scale-factor rationale

- 0.1 mg resolution over a uint16 covers the sensor's full ±8 g range headroom for RMS
  while resolving the ~18 mg bench noise floor with margin.
- Frequency as plain uint16 Hz covers the DC–6.3 kHz analysis band without a lookup table.
- Amplitude in 0.5 dB steps spans ~127 dB — more than the sensor's dynamic range — in one
  byte.

## Open questions

1. FFT length (2048 vs 4096 points at 26.667 kHz → 13 Hz vs 6.5 Hz bins) — trade between
   frequency resolution, RAM and energy per window.
2. Fixed spectral peaks vs ISO 10816-style band energies in the reserved bytes — peaks
   localise faults, bands trend severity; possibly both.
3. Downlink: configuration channel format (measurement interval, scale, band definitions).
4. Whether application-layer integrity is needed on top of LoRaWAN's, for LoRa-without-
   LoRaWAN deployments.

## Reference decoder

A reference decoder (Python, single file, no dependencies) will be published alongside the
first implementing firmware, with test vectors.
