#pragma once
// config.h — compile-time configuration, GPIO pins and tunables for the Smart Bin firmware.
// Change pin assignments here if you wire your ESP32 differently.

#include <cstdint>

// ---------------------------------------------------------------------------
// GPIO pins
// ---------------------------------------------------------------------------
static const uint8_t PIN_TRIG = 5;    // HC-SR04 Trig  (ESP32 -> sensor, output)
static const uint8_t PIN_ECHO = 18;   // HC-SR04 Echo  (sensor -> ESP32, input, 5V -> divider)
static const uint8_t PIN_VIB  = 19;   // SW-420 DO     (sensor -> ESP32, input, 3.3V)
static const uint8_t PIN_LED  = 2;    // On-board LED / status (output)
static const uint8_t PIN_BTN  = 0;    // Boot button — hold >3s at boot to wipe NVS

// ---------------------------------------------------------------------------
// Ultrasonic (HC-SR04)
// ---------------------------------------------------------------------------
static const uint32_t ULTRASONIC_INTERVAL_MS = 500;   // 2 Hz polling
static const uint32_t ECHO_TIMEOUT_US        = 25000; // 25 ms (~4.3 m max range)
static const uint8_t  ULTRASONIC_MEDIAN_N    = 5;     // median filter window
// Speed of sound at 20C: 343 m/s => 0.0343 cm/us, round-trip => /2
static constexpr float SOUND_CM_PER_US = 0.0343f / 2.0f;

// ---------------------------------------------------------------------------
// Vibration (SW-420) — pattern-based "emptied" detection
// ---------------------------------------------------------------------------
static const uint32_t VIB_POLL_MS      = 20;      // 50 Hz sampling
static const uint32_t VIB_PULSE_MIN_MS = 20;      // ignore pulses shorter than this (bounce / single knock)
static const uint32_t VIB_WINDOW_MS    = 4000;    // sliding window length
static const uint32_t VIB_COOLDOWN_MS  = 60000;   // suppress repeat events for 60s
static const uint8_t  VIB_DUTY_REQ_PCT = 35;      // % of window HIGH required to fire an event

// ---------------------------------------------------------------------------
// Storage
// ---------------------------------------------------------------------------
static const char* NVS_NAMESPACE       = "smartbin";
static const char* HISTORY_PATH        = "/history.json";
static const size_t HISTORY_MAX_BYTES  = 32 * 1024; // rotate when larger
static const size_t HISTORY_MAX_EVENTS = 500;

// ---------------------------------------------------------------------------
// HTTP server
// ---------------------------------------------------------------------------
static const uint16_t HTTP_PORT = 80;

// ---------------------------------------------------------------------------
// Firmware identity
// ---------------------------------------------------------------------------
static const char* FW_VERSION = "1.0.0";

// Convenience: build the mDNS hostname prefix. Suffix is appended at runtime
// (e.g. "smartbin" + "-" + "kitchen" => "smartbin-kitchen").
static const char* MDNS_PREFIX = "smartbin";

// Default friendly name shown before the user sets one.
static const char* DEFAULT_BIN_NAME = "Smart Bin";
