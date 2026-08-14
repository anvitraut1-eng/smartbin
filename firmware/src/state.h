#pragma once
// state.h — central runtime state shared across modules.
//
// One global `g_state` instance holds everything the HTTP API surfaces.
// Modules write into it; web_server.cpp serializes it to JSON.

#include <Arduino.h>

// Calibration + identity stored in NVS and mirrored here at boot.
struct BinState {
    // Identity
    String bin_name;     // friendly name, e.g. "Kitchen"
    String host_suffix;  // mDNS suffix, e.g. "kitchen" => smartbin-kitchen.local
    String chip_id;      // derived from ESP.getEfuseMac()

    // Calibration (cm)
    float empty_cm;      // distance when bin is empty (sensor far from contents)
    float full_cm;       // distance when bin is full (sensor close to contents)
    bool  calibrated;    // false until both empty_cm and full_cm are set

    // Live sensor readings
    float    distance_cm;        // last filtered ultrasonic reading
    float    fill_pct;           // 0..100
    bool     sensor_out_of_range;// distance_cm > empty_cm + margin
    bool     vibrating;          // SW-420 currently HIGH
    uint8_t  vib_duty_pct;       // current window duty % (debug/tuning)

    // Events
    uint32_t last_emptied;       // unix epoch seconds of last "emptied" event (0 = never)

    // Connectivity diagnostics
    uint32_t uptime_s;
    int32_t  rssi;
};

extern BinState g_state;

inline void stateResetLive() {
    // Zero only the live/transient fields; keep identity + calibration.
    g_state.distance_cm          = 0.0f;
    g_state.fill_pct             = 0.0f;
    g_state.sensor_out_of_range  = false;
    g_state.vibrating            = false;
    g_state.vib_duty_pct         = 0;
}
