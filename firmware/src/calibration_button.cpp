// calibration_button.cpp — physical button for calibration without the PWA.
//
// Button behavior (toggle mode with LED feedback):
//   1. Press → enter calibration mode (LED blinks fast)
//   2. Press → measure and store empty_cm (LED blinks 2x)
//   3. Press → measure and store full_cm (LED blinks 3x, then exit)
//   4. Press → exit calibration mode (LED back to normal)
//
// The button is on PIN_CAL_BTN (GPIO 4), pulled up internally. Connect a
// momentary push button between GPIO 4 and GND.
//
// LED feedback uses non-blocking scheduling shared with main.cpp's
// tickLedFeedback() so the LED never blocks the loop (preserving the 50 Hz
// vibration sampling).

#include <Arduino.h>
#include "config.h"
#include "modules.h"

enum CalState { CAL_IDLE, CAL_WAIT_EMPTY, CAL_WAIT_FULL };
static CalState s_calState = CAL_IDLE;
static uint32_t s_lastPressMs = 0;
static bool     s_lastBtnState = HIGH;
static uint32_t s_calLedToggleMs = 0;
static bool     s_calLedState = false;

// Defined in main.cpp — non-static so multiple modules can schedule LED blinks.
extern uint8_t  s_ledBlinksLeft;
extern uint32_t s_ledBlinkNextMs;
extern bool     s_ledBlinkState;

// Schedule a number of LED blinks using the shared feedback state. Each blink
// is one on+off pair. Half-period defaults to 150 ms (~3 Hz).
static void scheduleLedBlinks(uint8_t count) {
    s_ledBlinksLeft   = count * 2;
    s_ledBlinkNextMs  = millis();
    s_ledBlinkState   = false;
}

void calibrationButtonBegin() {
    pinMode(PIN_CAL_BTN, INPUT_PULLUP);
}

void calibrationButtonTick() {
    uint32_t now = millis();
    bool btn = digitalRead(PIN_CAL_BTN) == LOW; // active low (pulled up)

    // Debounce: trigger on falling edge with 50ms guard.
    if (btn && !s_lastBtnState && (now - s_lastPressMs > 50)) {
        s_lastPressMs = now;

        if (s_calState == CAL_IDLE) {
            // Enter calibration mode.
            s_calState = CAL_WAIT_EMPTY;
            Serial.println("[cal_btn] Calibration mode ON — press to set EMPTY");
        } else if (s_calState == CAL_WAIT_EMPTY) {
            // Measure empty. ultrasonicMeasureCm() internally uses short delays
            // between shots; acceptable here since the user pressed the button
            // and is waiting.
            Serial.println("[cal_btn] Measuring empty...");
            float cm = calibrationMeasureAndStoreEmpty();
            if (cm > 0) {
                Serial.printf("[cal_btn] Empty set: %.1f cm\n", cm);
                scheduleLedBlinks(2);
                s_calState = CAL_WAIT_FULL;
                Serial.println("[cal_btn] Press to set FULL");
            } else {
                Serial.println("[cal_btn] Measurement failed, try again");
                scheduleLedBlinks(5);
            }
        } else if (s_calState == CAL_WAIT_FULL) {
            Serial.println("[cal_btn] Measuring full...");
            float cm = calibrationMeasureAndStoreFull();
            if (cm > 0) {
                Serial.printf("[cal_btn] Full set: %.1f cm\n", cm);
                scheduleLedBlinks(3);
                s_calState = CAL_IDLE;
                Serial.println("[cal_btn] Calibration complete! Exiting mode.");
            } else {
                Serial.println("[cal_btn] Measurement failed, try again");
                scheduleLedBlinks(5);
            }
        }
    }
    s_lastBtnState = btn;

    // LED indicator for calibration mode (5 Hz blink). Only drive the LED if
    // no feedback blink is in progress — tickLedFeedback() in main.cpp owns
    // the LED while pending blinks exist.
    if (s_ledBlinksLeft > 0) return; // let main.cpp's ticker handle it

    if (s_calState != CAL_IDLE) {
        if (now - s_calLedToggleMs > 100) { // 5 Hz
            s_calLedToggleMs = now;
            s_calLedState = !s_calLedState;
            digitalWrite(PIN_LED, s_calLedState ? HIGH : LOW);
        }
    } else {
        // Normal mode: solid LED on = ready.
        digitalWrite(PIN_LED, HIGH);
    }
}
