# Bench measurements

Characterisation of the second-generation sensor board. Configuration: IIS3DWB at
26.667 kHz ODR, ±8 g range, 256-sample windows, SPI Mode 0.

These are **characterisation** results — they describe how the hardware behaves, not how
faults are diagnosed. A shaker comparison against a reference accelerometer is still to be
done.

> **Correction, 28 August 2026.** The noise floor originally published here (~1 mg RMS)
> was measured with a misconfigured sensor and has been replaced. See
> [Errata](#errata-sensor-was-running-at-the-wrong-rate) below — the story of how the
> error surfaced is worth more than the number itself.

## Results

| Measurement | Result |
|---|---|
| Sensor identification (`WHO_AM_I`) | `0x7B`, stable across 7/7 read cycles |
| SPI mode | Mode 0 only; Mode 3 returns `0x00` — the part does not respond in Mode 3 |
| Noise floor at rest, full bandwidth | **~18 mg RMS** (256-sample window, bench surface) |
| Gravity, face down | Z ≈ −957 mg |
| Gravity, inverted | Z ≈ +1018 mg |
| Impact response (tap test) | RMS rises from the noise floor to 77 / 155 / 412 / 900 mg depending on strength |
| Settling after impact | Returns to noise floor in under 1 s |
| On-die temperature | Stable, 26.5–26.9 °C |

Gravity, orientation, identification and settling behaviour are unaffected by the
correction below. The tap-test amplitudes were recorded during initial bring-up and
demonstrate dynamic range and settling rather than absolute amplitude at full bandwidth.

![Serial output of the self-check demo](images/bench-demo-output.jpg)

*The `bench_demo` sketch reporting identity, configuration read-back, gravity vector and
noise floor, followed by live samples — the spikes are taps on the bench surface.*

## What these numbers mean

The sign of the gravity reading flips correctly when the board is inverted and the
magnitude sits close to 1 g in both orientations, which confirms axis orientation and
scale factor.

The ~18 mg RMS noise floor is measured across the full 6.3 kHz bandwidth on an ordinary
bench. For reference, the datasheet noise density of 75 µg/√Hz over that bandwidth gives a
theoretical floor of about 5.9 mg for the sensor alone; the difference is the environment —
building vibration reaching the bench, and microphonics in the flying-lead harness between
the two boards. Reducing it is a mechanical and wiring problem, not a sensor problem, and
it is one of the things the enclosure work has to address.

The practical consequence: at full bandwidth on this setup, vibration below roughly 18 mg
RMS is not distinguishable from the background. Narrowing the analysis band — which is what
the FFT does — recovers sensitivity within the bands of interest.

The tap test spans nearly three orders of magnitude without saturating and settles back
within a second, showing the acquisition chain has usable dynamic range and no ringing
that would smear later spectral analysis.

## Velocity RMS in the ISO 10816 band (added 2 September 2026)

The node now reports vibration velocity RMS the way ISO 10816 / ISO 20816 define it:
band-limited to 10–1000 Hz and computed on a gap-free 307 ms window (8192 samples at
26.667 kHz) read from the sensor's hardware FIFO.

Processing chain per axis: remove mean → 2nd-order Butterworth high-pass 8 Hz →
trapezoidal integration to velocity → 2nd-order high-pass 8 Hz (removes integration
drift) → 2nd-order low-pass 1000 Hz → RMS over the last 5462 samples (the first 2730 are
discarded for filter settling). Two cascaded 8 Hz sections give a combined −3 dB point at
10 Hz. The filters run in double precision: at fc/fs ≈ 3·10⁻⁴ a single-precision biquad
loses its poles to rounding.

Validation of the chain against synthetic signals (100 mg amplitude; exact velocity RMS =
a / (2πf·√2)), reproducible with [`tools/vrms_filter_check.py`](../tools/vrms_filter_check.py):

| Signal | Expected | From the chain | Error |
|---|---|---|---|
| 10 Hz (band edge, −3 dB) | 7.80 mm/s | 7.90 mm/s | +1.2 % |
| 24.7 Hz (1480 rpm shaft rate) | 4.47 mm/s | 4.40 mm/s | −1.6 % |
| 50 Hz | 2.21 mm/s | 2.19 mm/s | −0.6 % |
| 100 Hz | 1.10 mm/s | 1.10 mm/s | +0.1 % |
| 500 Hz | 0.221 mm/s | 0.214 mm/s | −3.1 % ¹ |
| 1000 Hz (band edge, −3 dB) | 0.078 mm/s | 0.078 mm/s | −0.4 % |
| 2 Hz, out of band (55 mm/s unfiltered) | — | 0.49 mm/s | rejected 110× |
| 1000 mg DC offset + 24.7 Hz at 50 mg | 2.23 mm/s | 2.20 mm/s | −1.6 % |

¹ Expected, not an error: at half its corner frequency the second-order 1000 Hz low-pass
already attenuates by |H| = 1/√(1 + 0.5⁴) = 0.970.

At rest on the bench the node reports 0.03–0.09 mm/s per axis: the 18 mg wideband noise
floor collapses once the band is limited. The 8192-sample acquisition takes 306 ms
(8192 / 26 667 Hz = 307 ms) — that timing is the permanent check that no samples were
dropped; see the second erratum.

## Errata: sensor was running at the wrong rate

The first published noise floor was ~1 mg RMS. That figure was real in the sense that the
instrument reported it — but it was impossible. With 75 µg/√Hz across 6.3 kHz, no
configuration of this part can produce 1 mg RMS; the arithmetic alone puts the floor near
5.9 mg.

What had happened: the configuration register `CTRL1_XL` was being written as `0x2C`, an
undocumented bit combination. The part accepted it and ran at roughly 105 Hz instead of
26.667 kHz. A 105 Hz bandwidth genuinely does produce a ~1 mg floor — the measurement was
correct for a sensor doing something other than what the code claimed.

The error surfaced from timing, not from reading the datasheet again: sample blocks were
taking far longer to fill than 26.667 kHz would allow. A diagnostic sketch that measured
actual sample rate against register contents confirmed it — `0x2C` gave 105 Hz, `0xAC`
gave full rate, and reading the register back returned what was written.

The correct value is `0xAC`: `XL_EN[2:0] = 101` in bits 7:5, the only permitted setting for
26.667 kHz, with ±8 g in bits 3:2. The bit layout is documented in ST application note
AN5444 rather than the main datasheet register map, which is how the wrong value survived
review in the first place.

Two things worth taking from this. First, always verify the configuration you believe you
wrote — read the register back and check the resulting sample rate, which the sketches
here now do. Second, a measurement that looks better than physics allows is a bug report,
not a result.

## Errata 2: polled acquisition was dropping samples

Until version 6 of the node firmware the per-packet metrics were computed from samples
read one at a time by polling the data-ready flag over SPI. That loop sustains only about
15 kS/s, so roughly 40 % of the 26.667 kHz samples were silently skipped — while the
integration step still assumed 37.5 µs between samples. The FFT path was never affected
(it used the FIFO from the start), but the trended velocity RMS was biased.

It surfaced during a systems audit, not from a wrong-looking number: the time to fill a
block did not match sample count ÷ rate. Everything time-domain now comes from the FIFO,
and the firmware prints the block acquisition time on every cycle so the check cannot be
forgotten. The lesson generalises the first erratum: verify the rate you *achieved*, not
only the rate you configured.

## Errata 3: bench cadence exceeded the duty-cycle limit

For two days of bench testing the node transmitted every 2 s. An 85-byte packet at
SF7 / BW 125 kHz / CR 4/7 is 226 ms on air, so that cadence occupied 11 % of the channel —
above the 10 % permitted in the 869.4–869.65 MHz sub-band. Nothing flagged it: the radio
does not refuse to transmit, and the receiver was a bench unit a metre away at 10 dBm.

It was caught in the systems audit by computing time-on-air from the LoRa symbol formula
instead of estimating it. The node, its downlink configuration path and the collector now
all enforce a 3 s minimum (7.5 %), and the [payload specification](payload-spec.md#bench-protocol-status-2-september-2026)
records the airtime budget so the limit is designed in rather than checked afterwards. The
24-byte binary format planned for LoRaWAN is 86 ms on air — 4.3 % even at 2 s.

## Not yet measured

- Frequency response against a shaker with a calibrated reference accelerometer
- Behaviour across the full −40…+105 °C range
- Long-term drift
- Power consumption in duty-cycled operation

## Reproducing

The sketches under [`../firmware/tests/`](../firmware/tests/) produce these readings:
`sensor_id` verifies the SPI link and part identity, `acquisition` streams mean, RMS and
temperature, and `bench_demo` runs the full self-check sequence including the register
read-back described above. The velocity-RMS filter chain and its validation table are
reproduced by `tools/vrms_filter_check.py` (plain Python, no hardware needed).
