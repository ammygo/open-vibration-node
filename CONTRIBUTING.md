# Contributing

This is a small project and early. The most valuable contributions right now are review
and reproduction, not code:

- **Review the [payload specification draft](docs/payload-spec.md)** — encoding choices,
  missing fields, third-party decoder concerns.
- **Try to rebuild something** — the [mounting pad](enclosure/mounting-pad/), the
  [bench sketches](firmware/tests/), eventually a full board from the
  [production files](hardware/). Reports of what was unclear are as useful as success.
- **Measurement methodology** — if you have access to a calibrated shaker or reference
  accelerometers and see a flaw in [how things are measured](docs/measurements.md), say so.

## Ground rules

- Open an issue first for anything non-trivial, so effort is not wasted.
- Contributions land under the repository licences: MIT (firmware/software),
  CERN-OHL-W-2.0 (hardware), CC-BY-SA-4.0 (documentation).
- Claims must be verifiable: measurements come with conditions and configuration, and a
  result that looks better than physics allows is treated as a bug report, not a result.
- If generative AI assisted your contribution, say so in the pull request — the same
  standard this project applies to itself ([TRANSPARENCY.md](TRANSPARENCY.md)).
