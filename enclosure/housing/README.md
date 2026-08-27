# Sensor housing

Enclosure for the two-board sensor stack, revision 3.

> **Work in progress.** The model is published as it stands, at the stage where the board
> stack and mounting interface are settled but sealing details are still being worked
> through. It is here so the mechanical direction can be reviewed and reused, not because
> it is finished.

## Files

| File | Purpose |
|---|---|
| `src/sensor_housing_v3.f3d` | Fusion 360 source — full editable model |
| `export/sensor_housing_v3.step` | STEP export — opens in any CAD package, no Fusion licence needed |

The STEP export exists deliberately: a design published only in a proprietary format is
not open in any practical sense. Anyone should be able to open, measure and modify this
without a commercial licence.

## Design constraints it has to satisfy

- **Rigid path to the sensor.** The housing must not sit between the mounting pad and the
  sensor board; compliance anywhere in that path attenuates exactly the high-frequency
  content the node exists to measure.
- **Radio transparency.** The RAK3112 antenna faces away from the mounted surface, and no
  metal or carbon-filled material may sit over it.
- **Sealing without printed gaskets.** A groove for EPDM or silicone O-ring cord; printed
  gaskets leak through their layer lines.
- **Pressure equalisation.** A membrane vent, so the enclosure does not pump moisture in
  as it heats and cools outdoors.
- **Serviceable fasteners.** Threaded brass inserts — printed threads do not survive
  repeated opening or sustained vibration.

Licence: CERN-OHL-W-2.0.
