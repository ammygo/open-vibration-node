# Open Vibration Node

An open hardware and open firmware vibration sensing node for industrial condition
monitoring — analysing vibration **on the device** and reporting results over LoRaWAN,
with no mandatory cloud service.

> **Status: early development.** Hardware and firmware are being designed and bench-tested.
> Design files will be published in this repository as they mature. Nothing here is
> production-ready yet.

## Why this project exists

Predictive maintenance works: measuring vibration on motors, bearings, pumps and fans
reveals developing faults weeks or months before they cause a breakdown. The techniques
are well documented in engineering literature, and the sensors cost a few euros.

Access to them is not open, though. Commercial condition-monitoring systems are closed
end to end — proprietary sensors, proprietary gateways, and a vendor cloud that must stay
reachable and paid for, or the equipment stops being monitored. Prices start in the
hundreds of euros per measurement point, which puts continuous monitoring out of reach for
small manufacturers, municipal utilities, water pumping stations and building services —
exactly the operators who can least afford an unplanned failure.

There is no open, documented, reproducible node that a competent technician can build,
audit, deploy and keep running independently. This project aims to be that node.

## Design goals

- **Analysis on the device.** FFT and envelope processing run on the node's MCU. Only
  results leave the device — a compact payload of dominant frequencies, amplitudes, RMS
  and temperature (target: under 24 bytes), not raw waveforms. This is what makes
  battery-powered operation over a low-bandwidth radio possible at all.
- **Local-first.** A node works with a self-hosted LoRaWAN network server. No account, no
  vendor cloud, no remote kill switch. Cloud services are optional, never required.
- **Auditable and reproducible.** Complete schematics, PCB layout, bill of materials,
  firmware source and enclosure models under free licences, with build documentation
  detailed enough to reproduce a working node.
- **Long field life.** Multi-year operation on a primary lithium cell, or solar plus
  LiFePO4 where sampling is continuous. Sealed enclosure for industrial and outdoor use.
- **Secure by design.** Per-device keys, signed firmware updates and a published software
  bill of materials, so that nodes remain maintainable under the EU Cyber Resilience Act
  rather than becoming unsupported hardware.

## Planned architecture

```
Accelerometer  ──►  MCU (FFT / envelope / RMS)  ──►  LoRaWAN uplink  ──►  Self-hosted
(low-noise MEMS)    duty-cycled, event-driven        (~20 byte result)      network server
```

| Block | Direction being taken |
|---|---|
| MCU + radio | nRF52840-class MCU with SX1262 LoRa transceiver |
| Sensor | Low-noise triaxial MEMS accelerometer (ADXL355 class), 10 Hz–5 kHz band of interest |
| Signal chain | Windowed FFT plus envelope (demodulated high-band) analysis for early bearing faults |
| Reporting | Dominant spectral peaks, band RMS, temperature; scheduled plus threshold-triggered |
| Power | Primary lithium cell, or solar with LiFePO4 for continuous sampling |
| Enclosure | 3D-printable, sealed, mountable on machine housings; ASA / PC for heat and UV |

Temperature is recorded with every measurement: modal frequencies shift with it, and
comparing spectra taken at different temperatures without correction produces false alarms.

## Repository layout

| Path | Contents |
|---|---|
| `hardware/` | KiCad schematics, PCB layout, bill of materials |
| `firmware/` | Node firmware, signal processing, LoRaWAN stack integration |
| `enclosure/` | 3D-printable enclosure — source models and exported meshes |
| `docs/` | Build instructions, measurement methodology, test results |

## Licences

- Firmware and software: [MIT](LICENSE)
- Hardware design files: CERN-OHL-W-2.0 (see `hardware/LICENSE.txt`)
- Documentation: CC-BY-SA-4.0

## Roadmap

1. Bench prototype — sensor and radio validated on a development board
2. First custom PCB revision, characterised against a reference vibration source
3. Printed enclosure with sealing and machine-mount options, tested outdoors
4. Field deployment on real rotating equipment, with published measurement data
5. Documented reproducible build — someone else assembles a node from this repository

## About

Built by Laurynas Misiūnas, an electronics engineer in Klaipėda district, Lithuania, with
six years of industrial experience deploying edge AI vision systems and embedded
controllers on production lines.

Contributions, review and questions are welcome — open an issue.
