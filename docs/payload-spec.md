# Uplink payload specification — DRAFT v0

> **Status: draft for discussion.** The binary format is not transmitted yet; three of its
> elements are already exercised by the interim bench protocol (see
> [Bench protocol status](#bench-protocol-status-2-september-2026)). The format will be
> versioned from the first real transmission, and this document will track it. Review and
> criticism are welcome — open an issue.

## Goals

- Fit comfortably inside LoRaWAN duty-cycle and airtime limits: **24 bytes maximum**, so a
  single uplink carries a complete measurement summary even at slow spreading factors.
- Carry **results, not waveforms**: the node runs FFT and envelope analysis locally and
  transmits a spectral summary. Raw data never has to leave the device.
- Be **self-describing enough to evolve**: an explicit version field, so decoders can
  support mixed fleets.
- Be trivially implementable by third parties: fixed offsets, no compression, integer
  fields with documented scale factors.

## Bench protocol status (2 September 2026)

The binary layout below remains the LoRaWAN target. The bench setup meanwhile runs an
interim point-to-point text protocol (`VIB3 id=… n=… i=… a=… v=… c=… t=… e=…`) so the
collector can be developed against real packets. Three items of this specification are
already exercised there and will carry over unchanged:

- **Sensor-health flag** (the inverse of status bit 0, which is set when the sensor is
  healthy): field `e=` is 1 whenever the accelerometer fails
  identification or does not fill the FIFO. The collector then raises a "sensor fault"
  alarm and suppresses diagnosis instead of reporting a healthy, silent machine.
- **Reboot detection** (what status bits 3 and 5 will carry): for now the collector infers
  a reboot from a sequence number that goes backwards; the node runs a 90 s task watchdog.
- **Airtime budget:** at SF7 / BW 125 kHz / CR 4/7 an 85-byte text packet is 226 ms on
  air. In the 869.4–869.65 MHz sub-band (10 % duty cycle) that caps the cadence at
  ≥ 2.3 s; the node enforces a 3 s minimum (7.5 %). The 24-byte binary format is 86 ms on
  air — one of the reasons it exists. Time on air: `T_sym = 2^SF / BW`,
  `N_payload = 8 + max(ceil((8·PL − 4·SF + 28 + 16) / (4·SF)) · (CR + 4), 0)`,
  `T = (12.25 + N_payload) · T_sym` for an 8-symbol preamble with explicit header and CRC
  (see [measurements, Errata 3](measurements.md#errata-3-bench-cadence-exceeded-the-duty-cycle-limit)).
- **LoRaWAN target:** EU868 uplink channels sit in 1 % sub-bands, and a LoRaWAN frame adds
  13 bytes of MAC header and MIC to the payload. A 24-byte payload (37-byte frame) at SF7
  with CR 4/5 is 82 ms on air, which allows an uplink roughly every 8 s; at SF12 it is
  2.0 s on air, one uplink every ~3.3 minutes. That budget — not the bench link — is what
  sets the summary cadence design. The 10 % sub-band used by the bench link at
  869.525 MHz is reserved in LoRaWAN for RX2 downlinks.

## Proposed layout (19 of 24 bytes used)

| Offset | Size | Field | Encoding |
|---|---|---|---|
| 0 | 1 | Version + message type | high nibble: protocol version (0); low nibble: type (0 = periodic summary) |
| 1 | 1 | Sequence number | uint8, wraps at 255 |
| 2–3 | 2 | RMS acceleration | uint16, 0.1 mg units (0 – 6553.5 mg) |
| 4–5 | 2 | Peak acceleration | uint16, 0.25 mg units (0 – 16383.75 mg — covers the full ±8 g range) |
| 6–8 | 3 | Spectral peak 1 | frequency uint16 (Hz) + amplitude uint8 (0.5 dB steps re 0.1 mg) |
| 9–11 | 3 | Spectral peak 2 | same encoding |
| 12–14 | 3 | Spectral peak 3 | same encoding |
| 15–16 | 2 | Die temperature | int16, 0.1 °C units |
| 17 | 1 | Battery voltage | uint8, 20 mV steps from 2.0 V (covers 2.00 – 7.10 V) — *requires a battery sense divider, planned for the next board revision; current boards route BATT+ to the LDO only* |
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

- RMS at 0.1 mg per uint16 tops out at 6553.5 mg — above the physical RMS ceiling of a
  ±8 g sensor (8000/√2 ≈ 5657 mg), so the field cannot overflow, while still resolving
  the ~18 mg bench noise floor with margin.
- Peak uses a coarser 0.25 mg step because a peak *can* reach the full ±8 g (8000 mg);
  a 0.1 mg step in a uint16 would clip at 6553.5 mg on hard impacts. The clipping flag
  (bit 2) then indicates true sensor saturation, not encoding overflow.
- Spectral amplitude is referenced to **0.1 mg**, not 1 mg: early bearing defects on a
  quiet machine sit at 0.3–1 mg, and the per-bin noise floor at 13 Hz bins is
  75 µg/√Hz × √13 ≈ 0.27 mg — an unsigned dB value re 1 mg could not represent any of
  that. Re 0.1 mg, one byte at 0.5 dB steps spans 0.1 mg to well beyond sensor range.
- Frequency as plain uint16 Hz covers the DC–6.3 kHz analysis band without a lookup table.

## Acquisition requirement for spectral fields

Spectral peak frequencies are only meaningful if samples are taken at strictly uniform
26.667 kHz intervals. Polled register reads measured on the current firmware reach
14–17 kS/s — sufficient for RMS statistics, **not** for FFT. Acquisition for the spectral
fields must therefore use the sensor's 3 KB FIFO with burst reads, which guarantees
uniform sampling regardless of host timing. The reference firmware will treat this as a
hard requirement.

## Open questions

1. FFT length (2048 vs 4096 points at 26.667 kHz → 13 Hz vs 6.5 Hz bins) — trade between
   frequency resolution, RAM and energy per window.
2. Fixed spectral peaks vs ISO 10816-style band energies in the reserved bytes — peaks
   localise faults, bands trend severity; possibly both.
3. Downlink: configuration channel format (measurement interval, scale, band definitions).
4. Whether application-layer integrity is needed on top of the LoRaWAN MIC, for
   deployments where the network server is not under the operator's control.

## Reference decoder

A reference decoder (Python, single file, no dependencies) will be published alongside the
first implementing firmware, with test vectors.
