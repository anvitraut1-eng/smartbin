# Smart Bin

A small home-network project: two ESP32 bins that report their fill level and
log the timestamp whenever they are emptied, viewed in an installable PWA on
your phone.

- **Hardware**: ESP32 DevKit + HC-SR04 ultrasonic + SW-420 vibration sensor
  (one of each per bin)
- **Firmware**: Arduino framework, built with PlatformIO
- **PWA**: vanilla JS, no build step, installable to the home screen
- **Network**: home WiFi only — no cloud, no account
- **Discovery**: mDNS, e.g. `http://smartbin-kitchen.local`

## Repository layout

```
smartbin/
├── firmware/         ESP32 firmware (PlatformIO)
│   ├── platformio.ini
│   └── src/          main.cpp + modules
├── pwa/              Progressive Web App (static)
│   ├── index.html
│   ├── manifest.webmanifest
│   ├── service-worker.js
│   ├── app.js
│   ├── styles.css
│   └── icons/
└── docs/             wiring.md, api.md
```

## Quick start

### 1. Hardware
Wire each ESP32 per `docs/wiring.md` (voltage divider on the echo pin is
mandatory). Power each ESP32 from a USB wall wart.

### 2. Flash the firmware
```bash
cd firmware
pio run -t upload
pio device monitor   # 115 200 baud
```

On first boot the ESP32 starts an AP `SmartBin-Setup-XXXX`. From your phone:

1. Join that WiFi
2. Browser opens the captive portal (or navigate to `192.168.4.1`)
3. Pick your home WiFi, enter password, set a **host suffix** (`kitchen` or
   `office`), Save

The ESP32 reboots and joins your home WiFi. From any laptop on the same
network:

```bash
curl http://smartbin-kitchen.local/api/state
```

### 3. Calibrate each bin

In the PWA: tap `⋯` → Calibrate → "I'm empty now" (with an empty bin) → "I'm
full now" (with a full bin). You can also enter the distances manually.

Or via curl:

```bash
curl -X POST http://smartbin-kitchen.local/api/calibrate/empty
curl -X POST http://smartbin-kitchen.local/api/calibrate/full
```

### 4. Deploy the PWA

Push `pwa/` to a `gh-pages` branch on GitHub, or just upload the files to any
static host (Netlify, Vercel, a Raspberry Pi, etc.). The PWA must be served
over HTTPS for "Add to Home Screen" installability.

Then open `https://<your-host>/smartbin/` on your phone → Add Bin → enter
`smartbin-kitchen.local` → Test → Save. The bin card appears with live fill
%. Tap the browser menu → "Add to Home Screen" to install.

## Endpoints

See `docs/api.md` for the full list. Quick reference:

| Method | Path                         | Notes                          |
|--------|------------------------------|--------------------------------|
| GET    | `/api/state`                 | Live fill %, distance, etc.    |
| GET    | `/api/calibration`           | Empty / full cm + bin name     |
| POST   | `/api/calibrate`             | Body: `{empty_cm?, full_cm?, bin_name?}` |
| POST   | `/api/calibrate/empty`       | One-tap "I'm empty now"        |
| POST   | `/api/calibrate/full`        | One-tap "I'm full now"         |
| GET    | `/api/history?limit=50`      | Last N emptied events          |
| GET    | `/api/info`                  | Bin id, name, fw, mDNS         |
| POST   | `/api/reboot`                | Reboot the ESP32               |

All responses carry `Access-Control-Allow-Origin: *`.

## Reset / re-provisioning

Hold the BOOT button (GPIO 0) for >3 s while powering on the ESP32 to wipe
saved WiFi credentials and re-enter the captive portal. Use this if you move
bins to a different network or change the WiFi password.

## Vibration tuning

The SW-420 module has an on-board sensitivity potentiometer. To set it:

1. Power the ESP32 and open the serial monitor.
2. Tap the sensor once — you should see a brief HIGH pulse, no event logged.
3. Continuously shake the bin for 2–3 s — you should see
   `VIB duty XX% — EVENT LOGGED`.
4. If shakes don't fire, turn the pot counter-clockwise (more sensitive).
5. If single taps fire, turn the pot clockwise (less sensitive).

The software thresholds (window 4 s, duty 35%, cooldown 60 s) live in
`firmware/src/config.h`.
