# Bench node firmware (interim, pre-LoRaWAN)

Point-to-point LoRa firmware used for bench characterisation and the collector development.
It already implements the parts of the node that do not depend on the network layer — FIFO
acquisition, ISO 10816 velocity RMS, health reporting, watchdog, raw capture over WiFi — and
transmits a plain-text summary over raw LoRa. The LoRaWAN node firmware (milestone 2 of the
proposal) will reuse this acquisition and processing core and replace the radio layer with a
LoRaWAN stack and the [binary payload](../../docs/payload-spec.md).

Two sketches:

| Folder | Board | Role |
|---|---|---|
| `node/` | RAK3112 (ESP32-S3 + SX1262) with the IIS3DWB sensor board | Measures, transmits, listens for downlinks, captures raw data on demand |
| `receiver/` | Seeed XIAO ESP32-S3 + Wio-SX1262 kit | Prints received packets to USB serial; forwards queued downlinks after the next uplink |

Together they form the bench link described in [measurements](../../docs/measurements.md);
the receiver is a stand-in for a LoRaWAN gateway, nothing more.

## What the node does each cycle

1. Reads a gap-free window of 8192 samples per axis (307 ms at 26.667 kHz) from the sensor's
   hardware FIFO in 64-word bursts. Polling the data-ready flag was found to drop ~40 % of
   samples ([Errata 2](../../docs/measurements.md#errata-2-polled-acquisition-was-dropping-samples)),
   so nothing time-domain uses it.
2. Per axis: broadband acceleration RMS and peak (mg), and velocity RMS in the 10–1000 Hz
   band (mm/s) — mean removal → Butterworth high-pass 8 Hz → trapezoidal integration →
   high-pass 8 Hz → low-pass 1000 Hz → RMS after a 2730-sample settling period. The chain is
   validated in [`tools/vrms_filter_check.py`](../../tools/vrms_filter_check.py).
3. Transmits one text packet (below), then listens 1.8 s for a downlink.
4. Sleeps the remainder of the cadence while still serving USB commands.

Self-monitoring: a `WHO_AM_I` mismatch or a FIFO that does not fill sets the health flag
`e=1` and triggers a sensor re-initialisation; a 90 s task watchdog restarts the module if
anything blocks; the radio is re-initialised after 5 consecutive transmit errors and the
module restarts after 20.

## Packet format (text, interim)

```
VIB3 id=<node> n=<seq> i=<cadence s> a=<aX>,<aY>,<aZ> v=<vX>,<vY>,<vZ> c=<crest> t=<°C> e=<health>
```

| Field | Meaning | Unit |
|---|---|---|
| `id` | last two bytes of the ESP32 MAC address | hex |
| `n` | sequence number since boot (a step backwards = the node rebooted) | — |
| `i` | current cadence | s |
| `a` | broadband acceleration RMS per axis | mg |
| `v` | velocity RMS per axis, 10–1000 Hz | mm/s |
| `c` | crest factor: max peak / total RMS | — |
| `t` | sensor die temperature | °C |
| `e` | health flag: 0 = OK, 1 = sensor not responding — readings are not trustworthy | — |

An 85-byte packet at SF7 / BW 125 kHz / CR 4/7 is 226 ms on air; the node refuses cadences
below 3 s to stay under the 10 % duty cycle of the 869.4–869.65 MHz sub-band
([Errata 3](../../docs/measurements.md#errata-3-bench-cadence-exceeded-the-duty-cycle-limit)).

**Downlinks** (sent by the receiver after the node's own uplink): `CFG id=<node> int=<s>`
sets the cadence (3…86400 s, stored in flash); `CAP id=<node>` requests a raw capture.

**Raw capture:** 32768 samples per axis (1.23 s) are recorded into PSRAM, then the node joins
WiFi and POSTs them to the collector as one `application/octet-stream` body — int16
little-endian X block, then Y, then Z — to `/api/raw?node=<id>&n=32768&sr=26667`. The
collector address is taken from the `URL` command if set, otherwise from mDNS
(`vib-collector.local`), otherwise from the placeholder in the sketch. WiFi is switched off
again afterwards; this path is on demand only.

**USB serial commands** (115200 baud):

| Command | Effect |
|---|---|
| `STAT` | node id, cadence, WiFi, collector URL, sensor state, uptime, reset reason |
| `WIFI <ssid> <password>` | stores WiFi credentials in flash (never sent over the air) |
| `INT <s>` | sets the cadence, 3…86400 s |
| `CAP` | triggers a raw capture now |
| `URL http://host:port/api/raw` / `URL -` | fixes the collector address / returns to automatic |

## Building

Arduino core `esp32` 2.0.17 and RadioLib 7.7.x. PSRAM must be enabled for the node — the raw
capture buffers live there.

```
arduino-cli compile --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc,PSRAM=opi node
arduino-cli upload  --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc,PSRAM=opi -p <port> node
arduino-cli compile --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc receiver
```

Expected serial output on boot: `WHO_AM_I: 0x7B OK`, `radio.begin: 0`, `WDT 90 s enabled`,
then one line per cycle ending in `(acq 306 ms, proc 354 ms)` — the acquisition time equals
8192 / 26 667 Hz, which is the check that no samples were dropped.

Pins — node (RAK3112 internal SX1262): NSS 7, SCK 5, MISO 3, MOSI 6, RST 8, DIO1 47, BUSY 48,
antenna switch GPIO 4 high. Sensor board on HSPI: SCK 13, MISO 10, MOSI 11, CS 12. Receiver
(XIAO + Wio-SX1262): NSS 41, DIO1 39, RST 42, BUSY 40, SPI 7/8/9, TCXO 1.8 V.

Radio, both ends: 869.525 MHz, BW 125 kHz, SF7, CR 4/7, sync word 0x12, 10 dBm.

## Limits, on purpose

No encryption, no addressing beyond the node id, no acknowledgements, one receiver — this is
a bench link. Those are exactly the things LoRaWAN provides, which is why the network layer
is the next milestone rather than an extension of this sketch.

## Provenance

Written with the assistance of Anthropic Claude from the author's specification and tested by
the author on the hardware above; see [TRANSPARENCY.md](../../TRANSPARENCY.md). Commits
carry the `Co-Authored-By` trailer.
