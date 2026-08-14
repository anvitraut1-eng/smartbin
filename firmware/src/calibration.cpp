// calibration.cpp — persistence of bin identity + empty/full distances in NVS.
//
// Uses the Preferences library (NVS wrapper). On boot we load saved values into
// g_state; the one-tap calibration endpoints call measureAndStoreEmpty/Full,
// which take a 10-sample median ultrasonic reading and persist it.

#include "config.h"
#include "modules.h"
#include <Preferences.h>
#include <Arduino.h>

static Preferences s_prefs;

static String deriveChipId() {
    uint64_t mac = ESP.getEfuseMac();
    char buf[13];
    snprintf(buf, sizeof(buf), "%04X%08X",
             (uint16_t)(mac >> 32), (uint32_t)mac);
    return String(buf);
}

void calibrationBegin() {
    s_prefs.begin(NVS_NAMESPACE, false); // read/write

    g_state.chip_id    = deriveChipId();
    g_state.bin_name   = s_prefs.getString("bin_name", DEFAULT_BIN_NAME);
    g_state.host_suffix = s_prefs.getString("host_suffix", "");

    // Sentinel values: empty_cm/full_cm of 0.0 mean "not calibrated yet".
    float e = s_prefs.getFloat("empty_cm", 0.0f);
    float f = s_prefs.getFloat("full_cm", 0.0f);
    if (e > 0.0f && f > 0.0f && e > f) {
        g_state.empty_cm    = e;
        g_state.full_cm     = f;
        g_state.calibrated  = true;
    } else {
        g_state.empty_cm   = 0.0f;
        g_state.full_cm    = 0.0f;
        g_state.calibrated = false;
    }
}

bool calibrationIsReady() { return g_state.calibrated; }

float calibrationMeasureAndStoreEmpty() {
    float cm = ultrasonicMeasureCm(10);
    if (cm <= 0.0f) return -1.0f; // read failed
    s_prefs.putFloat("empty_cm", cm);
    g_state.empty_cm = cm;
    g_state.calibrated = (g_state.full_cm > 0.0f && cm > g_state.full_cm);
    return cm;
}

float calibrationMeasureAndStoreFull() {
    float cm = ultrasonicMeasureCm(10);
    if (cm <= 0.0f) return -1.0f;
    s_prefs.putFloat("full_cm", cm);
    g_state.full_cm = cm;
    g_state.calibrated = (g_state.empty_cm > 0.0f && g_state.empty_cm > cm);
    return cm;
}

void calibrationSet(const String& binName, float emptyCm, float fullCm) {
    if (binName.length() > 0) {
        s_prefs.putString("bin_name", binName);
        g_state.bin_name = binName;
    }
    if (emptyCm > 0.0f) {
        s_prefs.putFloat("empty_cm", emptyCm);
        g_state.empty_cm = emptyCm;
    }
    if (fullCm > 0.0f) {
        s_prefs.putFloat("full_cm", fullCm);
        g_state.full_cm = fullCm;
    }
    g_state.calibrated = (g_state.empty_cm > 0.0f &&
                          g_state.full_cm  > 0.0f &&
                          g_state.empty_cm > g_state.full_cm);
}
