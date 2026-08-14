// vibration.cpp — pattern-based "bin was emptied" detector for the SW-420.
//
// The SW-420 gives a DIGITAL output: HIGH while vibration exceeds the on-board
// pot threshold, LOW otherwise. We sample at 50 Hz and look for a *sustained*
// burst — the signature of a bin being lifted/tipped/shaken to empty it — rather
// than a single knock (lid closing, a bump) which we must ignore.
//
// Algorithm (see plan):
//   - 50 Hz poll (millis()-driven, never delay()).
//   - Track consecutive-HIGH runs; only count runs >= PULSE_MIN_MS (debounce).
//   - Accumulate total HIGH-ms inside a WINDOW_MS sliding window.
//   - When the window elapses, compute duty %; if >= DUTY_REQ_PCT and we're past
//     the COOLDOWN_MS since the last event, fire onBinEmptied().
//
// Tunables live in config.h so they can be tweaked without touching this file.

#include "config.h"
#include "modules.h"
#include <Arduino.h>

struct VibState {
    uint32_t lastPollMs;
    uint32_t windowStartMs;
    uint32_t highMsInWindow;
    uint32_t highRunStartMs;   // when the current HIGH run began
    bool     currentlyHigh;
    uint32_t lastEventMs;
    uint8_t  lastDutyPct;
};

static VibState s;

void vibrationBegin() {
    pinMode(PIN_VIB, INPUT);
    s.lastPollMs       = millis();
    s.windowStartMs    = millis();
    s.highMsInWindow   = 0;
    s.highRunStartMs   = 0;
    s.currentlyHigh    = false;
    s.lastEventMs      = 0;
    s.lastDutyPct      = 0;
}

void vibrationTick() {
    uint32_t now = millis();
    if (now - s.lastPollMs < VIB_POLL_MS) return;
    s.lastPollMs = now;

    bool raw = digitalRead(PIN_VIB) == HIGH;
    g_state.vibrating = raw;

    // Edge tracking: accumulate valid HIGH-run durations into the window.
    if (raw && !s.currentlyHigh) {
        s.currentlyHigh  = true;
        s.highRunStartMs = now;
    } else if (!raw && s.currentlyHigh) {
        uint32_t runMs = now - s.highRunStartMs;
        if (runMs >= VIB_PULSE_MIN_MS) {
            s.highMsInWindow += runMs;
        }
        s.currentlyHigh = false;
    }

    // Window evaluation.
    if (now - s.windowStartMs >= VIB_WINDOW_MS) {
        uint32_t duty = (s.highMsInWindow * 100UL) / VIB_WINDOW_MS;
        s.lastDutyPct = (uint8_t)duty;
        g_state.vib_duty_pct = s.lastDutyPct;

        bool pastCooldown = (s.lastEventMs == 0) ||
                            (now - s.lastEventMs >= VIB_COOLDOWN_MS);
        if (duty >= VIB_DUTY_REQ_PCT && pastCooldown) {
            s.lastEventMs = now;
            uint32_t epoch = (uint32_t)time(nullptr); // requires SNTP (set in wifi)
            if (epoch == 0) epoch = now / 1000UL;     // fallback to uptime-based
            onBinEmptied(epoch);
            if (g_state.calibrated) {
                // After an empty, the bin is (briefly) empty — reflect that.
                g_state.fill_pct = 0.0f;
            }
        }
        // Reset for the next window.
        s.windowStartMs  = now;
        s.highMsInWindow = 0;
    }
}
