// calibration_button.cpp — physical button for calibration without the PWA.
//
// Button behavior (toggle mode with LED feedback):
//   1. Press → enter calibration mode (LED blinks fast)
//   2. Press → measure and store empty_cm (LED solid 2s)
//   3. Press → measure and store full_cm (LED solid 2s)
//   4. Press → exit calibration mode (LED back to normal)
//
// The button is on PIN_CAL_BTN (GPIO 4), pulled up internally. Connect a
// momentary push button between GPIO 4 and GND.

#include "config.h"
#include "modules.h"
#include <Arduino.h>

enum CalState { CAL_IDLE, CAL_WAIT_EMPTY, CAL_WAIT_FULL };
static CalState s_calState = CAL_IDLE;
static uint32_t s_lastPressMs = 0;
static bool s_lastBtnState = HIGH;
static uint32_t s_ledBlinkMs = 0;

void calibrationButtonBegin() {
    pinMode(PIN_CAL_BTN, INPUT_PULLUP);
}

static void ledFeedback(uint8_t blinks) {
    for (uint8_t i = 0; i < blinks; i++) {
        digitalWrite(PIN_LED, HIGH); delay(150);
        digitalWrite(PIN_LED, LOW);  delay(150);
    }
}

void calibrationButtonTick() {
    uint32_t now = millis();
    bool btn = digitalRead(PIN_CAL_BTN) == LOW; // active low (pulled up)

    // Debounce: only trigger on falling edge with 50ms debounce.
    if (btn && !s_lastBtnState && (now - s_lastPressMs > 50)) {
        s_lastPressMs = now;

        if (s_calState == CAL_IDLE) {
            // Enter calibration mode.
            s_calState = CAL_WAIT_EMPTY;
            Serial.println("[cal_btn] Calibration mode ON — press to set EMPTY");
        } else if (s_calState == CAL_WAIT_EMPTY) {
            // Measure empty.
            Serial.println("[cal_btn] Measuring empty...");
            digitalWrite(PIN_LED, HIGH);
            float cm = calibrationMeasureAndStoreEmpty();
            digitalWrite(PIN_LED, LOW);
            if (cm > 0) {
                Serial.printf("[cal_btn] Empty set: %.1f cm\n", cm);
                ledFeedback(2);
                s_calState = CAL_WAIT_FULL;
                Serial.println("[cal_btn] Press to set FULL");
            } else {
                Serial.println("[cal_btn] Measurement failed, try again");
                ledFeedback(5); // rapid blinks = error
            }
        } else if (s_calState == CAL_WAIT_FULL) {
            // Measure full.
            Serial.println("[cal_btn] Measuring full...");
            digitalWrite(PIN_LED, HIGH);
            float cm = calibrationMeasureAndStoreFull();
            digitalWrite(PIN_LED, LOW);
            if (cm > 0) {
                Serial.printf("[cal_btn] Full set: %.1f cm\n", cm);
                ledFeedback(3);
                s_calState = CAL_IDLE;
                Serial.println("[cal_btn] Calibration complete! Exiting mode.");
            } else {
                Serial.println("[cal_btn] Measurement failed, try again");
                ledFeedback(5);
            }
        }
    }
    s_lastBtnState = btn;

    // LED indicator: fast blink when in calibration mode.
    if (s_calState != CAL_IDLE) {
        if (now - s_ledBlinkMs > 200) {
            s_ledBlinkMs = now;
            digitalWrite(PIN_LED, !digitalRead(PIN_LED));
        }
    } else {
        // Normal mode: LED solid on when ready.
        if (now - s_ledBlinkMs > 1000) {
            s_ledBlinkMs = now;
            digitalWrite(PIN_LED, HIGH);
        }
    }
}
