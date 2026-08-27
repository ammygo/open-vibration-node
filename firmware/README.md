# Firmware

Node firmware: sampling, on-device signal processing, power management and LoRaWAN uplink.

Licence: MIT — see [../LICENSE](../LICENSE).

## Contents

Bench sketches are under `tests/`. Still to come:

- `src/` — application and signal processing sources
- `drivers/` — sensor and peripheral drivers
- `docs/` — payload format specification and decoder

## Processing pipeline

```
sample burst  ──►  window  ──►  FFT + envelope  ──►  extract features  ──►  pack payload
(configurable      (Hann)      (band-limited)       (peaks, RMS, temp)     (< 24 bytes)
 rate and length)
```

The node stays asleep between measurements, wakes on schedule or on an RMS threshold,
computes locally and transmits only extracted features. Raw waveforms are never sent over
the radio — they can optionally be stored on local media for offline analysis.

## Payload

The uplink format will be documented as a versioned specification, with a reference
decoder so that any LoRaWAN network server can interpret the data without proprietary
software. Every payload carries a format version byte so that decoders and firmware can
evolve independently.
