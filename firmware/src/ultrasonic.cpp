// ultrasonic.cpp — HC-SR04 driver with a 5-sample median filter.
//
// Reading strategy: trigger a 10us pulse, measure echo width with pulseIn()
// (25ms timeout), convert to cm, push into a ring buffer, return the median
// of the last N samples. The median drops the occasional wild reading that
// happens when an echo bounces off a bin wall.

#include "config.h"
#include "state.h"
#include <Arduino.h>

static float    s_ring[ULTRASONIC_MEDIAN_N];
static uint8_t  s_ringIdx = 0;
static uint8_t  s_ringCount = 0;
static uint32_t s_lastTickMs = 0;

static float medianOf(float* arr, uint8_t n) {
    // Insertion sort a small copy (n <= 10) and pick the middle.
    float tmp[10];
    for (uint8_t i = 0; i < n; i++) tmp[i] = arr[i];
    for (uint8_t i = 1; i < n; i++) {
        float key = tmp[i];
        int8_t  j  = i - 1;
        while (j >= 0 && tmp[j] > key) { tmp[j + 1] = tmp[j]; j--; }
        tmp[j + 1] = key;
    }
    return n % 2 ? tmp[n / 2] : (tmp[n / 2 - 1] + tmp[n / 2]) / 2.0f;
}

void ultrasonicBegin() {
    pinMode(PIN_TRIG, OUTPUT);
    pinMode(PIN_ECHO, INPUT);
    digitalWrite(PIN_TRIG, LOW);
    s_ringIdx = s_ringCount = 0;
    s_lastTickMs = millis();
}

// Fire one shot and return distance in cm (0.0 on timeout).
static float singleShotCm() {
    digitalWrite(PIN_TRIG, LOW);
    delayMicroseconds(2);
    digitalWrite(PIN_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_TRIG, LOW);

    uint32_t dur = pulseIn(PIN_ECHO, HIGH, ECHO_TIMEOUT_US); // us
    if (dur == 0) return 0.0f;                                // no echo / out of range
    return dur * SOUND_CM_PER_US;
}

float ultrasonicMeasureCm(uint8_t samples) {
    // Take `samples` raw shots and return their median — used by calibration
    // for a more reliable one-off measurement.
    float buf[10];
    if (samples > 10) samples = 10;
    for (uint8_t i = 0; i < samples; i++) {
        buf[i] = singleShotCm();
        delay(40); // give the sensor time to settle between shots
    }
    return medianOf(buf, samples);
}

float ultrasonicCurrentCm() {
    if (s_ringCount == 0) return 0.0f;
    return medianOf(s_ring, s_ringCount);
}

void ultrasonicTick() {
    uint32_t now = millis();
    if (now - s_lastTickMs < ULTRASONIC_INTERVAL_MS) return;
    s_lastTickMs = now;

    float cm = singleShotCm();
    if (cm <= 0.0f) return; // skip bad reads rather than poisoning the filter

    s_ring[s_ringIdx] = cm;
    s_ringIdx = (s_ringIdx + 1) % ULTRASONIC_MEDIAN_N;
    if (s_ringCount < ULTRASONIC_MEDIAN_N) s_ringCount++;

    // Update shared state.
    float filtered = medianOf(s_ring, s_ringCount);
    g_state.distance_cm = filtered;

    // Fill percentage, clamped 0..100. Empty = sensor far from contents
    // (large distance); full = sensor close to contents (small distance).
    if (g_state.calibrated && g_state.empty_cm > g_state.full_cm) {
        float span = g_state.empty_cm - g_state.full_cm;
        float pct  = (g_state.empty_cm - filtered) / span * 100.0f;
        if (pct < 0.0f) pct = 0.0f;
        if (pct > 100.0f) pct = 100.0f;
        g_state.fill_pct = pct;
        // Sensor sees past the rim (bin removed / tipped) => report out of range.
        g_state.sensor_out_of_range = (filtered > g_state.empty_cm + 2.0f);
    } else {
        g_state.fill_pct = 0.0f;
        g_state.sensor_out_of_range = false;
    }
}
