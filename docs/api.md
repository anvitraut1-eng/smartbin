# API

The ESP32 exposes a small JSON HTTP API on port 80. All responses carry
`Access-Control-Allow-Origin: *` so the PWA (different origin) can call freely.

Base URL (replace `smartbin-kitchen` with your bin's suffix):
`http://smartbin-kitchen.local`

## `GET /api/state`

Live state of the bin.

```json
{
  "fill_pct": 73,
  "distance_cm": 18.4,
  "empty_cm": 30.0,
  "full_cm": 8.0,
  "calibrated": true,
  "sensor_out_of_range": false,
  "vibrating": false,
  "vib_duty_pct": 12,
  "last_emptied": 1731000000,
  "uptime_s": 4321,
  "rssi": -54
}
```

| Field                   | Type    | Meaning                                              |
|-------------------------|---------|------------------------------------------------------|
| `fill_pct`              | int     | 0–100, clamped                                       |
| `distance_cm`           | float   | Median-filtered ultrasonic reading                   |
| `empty_cm`              | float   | Calibrated empty distance (0 if not calibrated)      |
| `full_cm`               | float   | Calibrated full distance                             |
| `calibrated`            | bool    | Both `empty_cm` and `full_cm` are set                |
| `sensor_out_of_range`   | bool    | `distance_cm > empty_cm + 2` (sensor past the rim)   |
| `vibrating`             | bool    | SW-420 pin currently HIGH                            |
| `vib_duty_pct`          | int     | % of last 4 s window that was HIGH (debug/tuning)    |
| `last_emptied`          | int     | Unix seconds of last "emptied" event (0 = never)     |
| `uptime_s`              | int     | Seconds since boot                                   |
| `rssi`                  | int     | WiFi RSSI in dBm                                     |

## `GET /api/calibration`

```json
{ "bin_name": "Kitchen", "host_suffix": "kitchen", "empty_cm": 30.0, "full_cm": 8.0 }
```

## `POST /api/calibrate`

Body (any subset):

```json
{ "bin_name": "Kitchen", "empty_cm": 30.0, "full_cm": 8.0 }
```

Response: `{ "ok": true }`

## `POST /api/calibrate/empty`

Takes a 10-sample median ultrasonic reading and stores it as `empty_cm`.

Response: `{ "ok": true, "empty_cm": 31.2 }` (or `{"ok":false,"error":"read_failed"}`).

## `POST /api/calibrate/full`

Takes a 10-sample median ultrasonic reading and stores it as `full_cm`.

Response: `{ "ok": true, "full_cm": 7.9 }`

## `GET /api/history?limit=50`

```json
{ "events": [ { "t": 1731000000, "type": "emptied" }, ... ] }
```

`limit` defaults to 50; max 500 (file is rotated beyond that).

## `GET /api/info`

```json
{ "id": "AABBCCDDEEFF", "name": "Kitchen", "fw": "1.0.0", "mdns": "smartbin-kitchen" }
```

`id` is derived from `ESP.getEfuseMac()` and is unique per ESP32.

## `POST /api/reboot`

Response: `{ "ok": true }` (sent before reboot).

## Discovery

The PWA keeps a list of bins in `localStorage`. To add a bin, the user
enters the mDNS host (e.g. `smartbin-kitchen.local`) and the PWA fetches
`/api/info` to capture the unique `id`. The `id` is used to dedupe if the
same bin is added twice.

If mDNS fails, the user can fall back to entering the ESP32's IP address
directly.
