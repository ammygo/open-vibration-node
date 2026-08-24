# Documentation

Build instructions, measurement methodology and test results.

Licence: CC-BY-SA-4.0.

## Planned contents

- `build/` — step-by-step assembly, from bare PCB to sealed node
- `commissioning/` — mounting guidance, orientation, sampling configuration
- `methodology/` — how measurements are taken and interpreted: baseline capture,
  temperature correction, threshold selection
- `measurements/` — published datasets from bench and field tests, with the raw data

## Why the methodology matters

A vibration reading on its own means nothing. A useful node needs a baseline for the
specific machine, grouped by temperature, and thresholds derived from that baseline rather
than from generic limits. Documenting this properly is as much of the project as the
hardware — it is the part that decides whether an operator gets early warnings or a stream
of false alarms.
