# Wiring

One ESP32 per bin. Both bins are wired identically; only the **host suffix**
you enter during WiFi provisioning differs (e.g. `kitchen`, `office`).

## Components

| Item                         | Notes                                |
|------------------------------|--------------------------------------|
| ESP32 DevKit (v1 or v4)      | 3.3 V logic, 5 V tolerant V_in       |
| HC-SR04 ultrasonic           | 5 V power, 5 V echo (needs divider)  |
| SW-420 vibration sensor      | 3.3 V, on-board sensitivity pot      |
| 1 kΩ + 2 kΩ resistors        | HC-SR04 Echo voltage divider         |
| USB wall wart (5 V, ≥500 mA) | One per bin                          |

## Pin map

| Function           | ESP32 GPIO | Notes                                  |
|--------------------|------------|----------------------------------------|
| HC-SR04 VCC        | 5 V (Vin)  | Sensor needs 5 V                       |
| HC-SR04 GND        | GND        |                                        |
| HC-SR04 Trig       | GPIO 5     | Output (3.3 V is enough to trigger)    |
| HC-SR04 Echo       | GPIO 18    | Input, **voltage divider required**    |
| SW-420 VCC         | 3.3 V      |                                        |
| SW-420 GND         | GND        |                                        |
| SW-420 DO          | GPIO 19    | Input                                  |
| Status LED         | GPIO 2     | On-board LED                           |
| BOOT button        | GPIO 0     | Hold >3 s at boot to wipe NVS          |
| Calibration button | GPIO 4     | Momentary push button to GND, internal pull-up |

## Calibration button

GPIO 4 has an internal pull-up resistor enabled in firmware. Connect a
momentary push button between **GPIO 4** and **GND**. Pressing the button
cycles through calibration steps:

1. **Press once** → Enter calibration mode (LED blinks fast at 5 Hz)
2. **Press again** → Measures distance and stores as `empty_cm` (LED solid for 2s, then 2 quick blinks)
3. **Press again** → Measures distance and stores as `full_cm` (LED solid for 2s, then 3 quick blinks)
4. **Press again** → Exit calibration mode (LED back to normal)

If a measurement fails (sensor out of range), the LED rapidly blinks 5 times
and the state stays the same so you can retry.

While in calibration mode, make sure the bin is empty before pressing the
"set empty" step, and full before pressing the "set full" step.

## Voltage divider (HC-SR04 Echo)

HC-SR04 outputs 5 V on the Echo pin. ESP32 GPIO is not 5 V tolerant, so add
a 1 kΩ / 2 kΩ divider:

```
       5 V (HC-SR04 VCC)
        │
       ┌┴┐
       │ │ 1 kΩ
       │ │
       └┬┘
        │   ┌──────────┐
        └───┤ GPIO 18  │
            │          │
           ┌┴┐         │
           │ │ 2 kΩ    │
           │ │         │
           └┬┘         │
            │          │
           GND        GND
```

The voltage at GPIO 18 is `5 V × 2 / (1 + 2) ≈ 3.3 V` — safe for the ESP32.

## Mechanical notes

- Mount the HC-SR04 at least 3 cm above the highest possible fill line. HC-SR04
  cannot reliably measure distances under 2 cm.
- The HC-SR04 "sees" a cone of ~15°. Keep the sensor pointed straight down at
  the contents.
- Mount the SW-420 to the underside of the bin lid with double-sided tape.
  Vibration transmits through solid contact much better than through air.
- If the bin lid is plastic, the SW-420 can also be glued to the side of the
  bin near the top.

## Power

Power the ESP32 from a USB wall wart. The HC-SR04 is powered from the ESP32's
5 V (Vin) pin. If the wall wart is undersized, you may see brown-out resets
when the HC-SR04 fires; in that case power the HC-SR04 from a separate 5 V
supply sharing a common ground with the ESP32.
