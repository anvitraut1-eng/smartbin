// main.cpp — Smart Bin firmware entry point.
//
// Wiring (see docs/wiring.md):
//   HC-SR04: Trig->GPIO5, Echo->GPIO18 (via 1k/2k divider), VCC->5V
//   SW-420:  DO->GPIO19, VCC->3.3V
//   Status:  on-board LED GPIO2
//   Reset:   BOOT button GPIO0 (hold >3s at boot to wipe NVS)
//
// Flow: boot -> provision WiFi -> mDNS -> start HTTP API -> loop sampling
// the sensors. The vibration module calls onBinEmptied() when a sustained
// shake pattern is confirmed.

#include "config.h"
#include "modules.h"
#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>

// The single global state instance (declared extern in state.h).
BinState g_state;

static uint32_t s_lastDiagMs = 0;

// Called by vibration.cpp when an "emptied" pattern is confirmed.
void onBinEmptied(uint32_t epochSeconds) {
    g_state.last_emptied = epochSeconds;
    historyAppend("emptied", epochSeconds);
    Serial.printf("[main] EMPTIED event logged @ %lu\n", (unsigned long)epochSeconds);

    // Blink the LED a few times as user feedback.
    for (uint8_t i = 0; i < 4; i++) {
        digitalWrite(PIN_LED, HIGH); delay(80);
        digitalWrite(PIN_LED, LOW);  delay(80);
    }
}

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n[boot] Smart Bin starting");

    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, LOW);

    // Order matters: identity/calibration must load before WiFi (which may use
    // the suffix) and before the web server reads g_state.
    calibrationBegin();          // loads bin_name, host_suffix, empty/full cm
    historyBegin();              // mount LittleFS

    // g_state.host_suffix is loaded by calibrationBegin via the same NVS
    // namespace; mirror it for the wifi module's convenience.
    {
        Preferences prefs;
        prefs.begin(NVS_NAMESPACE, true);
        g_state.host_suffix = prefs.getString("host_suffix", "");
        prefs.end();
    }

    Serial.printf("[boot] chip_id=%s suffix=%s\n",
                  g_state.chip_id.c_str(),
                  g_state.host_suffix.length() ? g_state.host_suffix.c_str() : "(none)");

    wifiProvisioningBegin();     // captive portal + mDNS + SNTP
    ultrasonicBegin();
    vibrationBegin();
    webServerBegin();

    digitalWrite(PIN_LED, HIGH); // solid on = ready
    Serial.println("[boot] ready");
}

void loop() {
    // Fast path: vibration is sampled at 50 Hz and self-throttles.
    vibrationTick();
    // Ultrasonic self-throttles to 2 Hz.
    ultrasonicTick();

    // Periodic diagnostics to serial (every 10 s).
    uint32_t now = millis();
    if (now - s_lastDiagMs > 10000UL) {
        s_lastDiagMs = now;
        g_state.uptime_s = now / 1000UL;
        if (WiFi.status() == WL_CONNECTED) g_state.rssi = WiFi.RSSI();
        Serial.printf("[diag] dist=%.1fcm fill=%.0f%% vib=%s duty=%u%% emptied=%lu uptime=%lus rssi=%ld\n",
                      g_state.distance_cm, g_state.fill_pct,
                      g_state.vibrating ? "Y" : "n", g_state.vib_duty_pct,
                      (unsigned long)g_state.last_emptied,
                      (unsigned long)g_state.uptime_s, (long)g_state.rssi);
    }

    // Yield to the async WiFi/TCP stack. No delay() here — vibrationTick and
    // ultrasonicTick use millis() gating, not blocking waits.
    yield();
}
