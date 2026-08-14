// Unit test for the vibration pattern-detection core logic.
//
// The firmware's vibration.cpp is Arduino-coupled (digitalRead, millis), so
// this test re-implements the pure algorithm against an injected sample stream
// and verifies the boundary behavior: single knocks don't fire, sustained
// shakes do, and the cooldown suppresses repeats.
//
// Run:  pio test -e native -f test_vibration

#include <unity.h>
#include <vector>
#include <cstdint>

// Mirror of the tunables in config.h.
static const uint32_t POLL_MS      = 20;
static const uint32_t PULSE_MIN_MS = 20;
static const uint32_t WINDOW_MS    = 4000;
static const uint32_t COOLDOWN_MS  = 60000;
static const uint8_t  DUTY_REQ_PCT = 35;

struct VibCore {
    uint32_t windowStartMs    = 0;
    uint32_t highMsInWindow   = 0;
    uint32_t highRunStartMs   = 0;
    bool     currentlyHigh    = false;
    uint32_t lastEventMs      = 0;
    int      events           = 0;

    // Feed a sequence of (relative) HIGH/LOW samples, one per POLL_MS tick.
    void feed(const std::vector<bool>& samples, uint32_t startMs) {
        uint32_t now = startMs;
        for (bool raw : samples) {
            if (raw && !currentlyHigh) { currentlyHigh = true; highRunStartMs = now; }
            else if (!raw && currentlyHigh) {
                uint32_t run = now - highRunStartMs;
                if (run >= PULSE_MIN_MS) highMsInWindow += run;
                currentlyHigh = false;
            }
            now += POLL_MS;
            if (now - windowStartMs >= WINDOW_MS) {
                uint32_t duty = (highMsInWindow * 100UL) / WINDOW_MS;
                bool pastCd = (lastEventMs == 0) || (now - lastEventMs >= COOLDOWN_MS);
                if (duty >= DUTY_REQ_PCT && pastCd) { events++; lastEventMs = now; }
                windowStartMs = now; highMsInWindow = 0;
            }
        }
    }
};

// A single 150ms knock inside a 4s window => ~3.75% duty => no event.
void test_single_knock_does_not_fire(void) {
    VibCore v; v.windowStartMs = 0;
    std::vector<bool> samples(200, false); // 4s = 200 ticks @ 20ms
    // 150ms HIGH = ~7-8 ticks, in the middle.
    for (int i = 90; i < 98; i++) samples[i] = true;
    v.feed(samples, 0);
    TEST_ASSERT_EQUAL_INT(0, v.events);
}

// ~2.5s of sustained HIGH => ~62% duty => fires exactly once.
void test_sustained_shake_fires_once(void) {
    VibCore v; v.windowStartMs = 0;
    std::vector<bool> samples(200, false);
    for (int i = 20; i < 145; i++) samples[i] = true; // 2.5s HIGH
    v.feed(samples, 0);
    TEST_ASSERT_EQUAL_INT(1, v.events);
}

// Two shakes within the cooldown => only the first fires.
void test_cooldown_suppresses_repeat(void) {
    VibCore v; v.windowStartMs = 0;
    // First window: sustained shake.
    std::vector<bool> w1(200, false);
    for (int i = 0; i < 130; i++) w1[i] = true;
    v.feed(w1, 0);
    TEST_ASSERT_EQUAL_INT(1, v.events);
    // Second window immediately after (well within 60s cooldown): another shake.
    std::vector<bool> w2(200, false);
    for (int i = 0; i < 130; i++) w2[i] = true;
    v.feed(w2, WINDOW_MS);
    TEST_ASSERT_EQUAL_INT(1, v.events); // still 1
}

// Just below threshold (~30% duty) => no event.
void test_below_threshold_no_fire(void) {
    VibCore v; v.windowStartMs = 0;
    std::vector<bool> samples(200, false);
    for (int i = 0; i < 60; i++) samples[i] = true; // 1.2s HIGH => 30%
    v.feed(samples, 0);
    TEST_ASSERT_EQUAL_INT(0, v.events);
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_single_knock_does_not_fire);
    RUN_TEST(test_sustained_shake_fires_once);
    RUN_TEST(test_cooldown_suppresses_repeat);
    RUN_TEST(test_below_threshold_no_fire);
    return UNITY_END();
}
