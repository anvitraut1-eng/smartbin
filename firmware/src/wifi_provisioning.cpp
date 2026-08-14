// wifi_provisioning.cpp — get the ESP32 onto the user's home WiFi via a
// WiFiManager captive portal, then advertise it on the LAN with mDNS using
// a per-bin suffix (e.g. "kitchen" => smartbin-kitchen.local).
//
// First boot (no stored credentials): start an AP "SmartBin-Setup-<chipid>"
// and serve the portal at 192.168.4.1. The portal collects the WiFi SSID +
// password and an optional host suffix. After saving, reboot and join the
// home network. Hold the BOOT button (GPIO0) for >3s during boot to wipe
// NVS and re-enter the portal.

#include "config.h"
#include "modules.h"
#include <WiFiManager.h>       // tzapu/WiFiManager
#include <ESPmDNS.h>
#include <WiFi.h>
#include <Preferences.h>
#include <time.h>

static Preferences s_prefs;
static String s_mdnsHost;

// Custom portal parameter: host suffix.
static char s_suffixBuf[24] = {0};

static void configureTime() {
    // The vibration "emptied" timestamps need wall-clock time. Use SNTP over
    // the now-connected WiFi. Best-effort — if it fails, vibration.cpp falls
    // back to uptime-based seconds.
    configTzTime("UTC0", "pool.ntp.org", "time.google.com");
}

bool wifiIsConnected() { return WiFi.status() == WL_CONNECTED; }

String wifiMdnsHost() { return s_mdnsHost; }

static String buildHost(const String& suffix) {
    if (suffix.length() == 0) return String(MDNS_PREFIX);
    return String(MDNS_PREFIX) + "-" + suffix;
}

void wifiProvisioningBegin() {
    s_prefs.begin(NVS_NAMESPACE, false);

    // --- Wipe on BOOT button held >3s at power-up ---
    // (PIN_BTN is GPIO0, also the boot/flash button on most dev boards.)
    pinMode(PIN_BTN, INPUT_PULLUP);
    if (digitalRead(PIN_BTN) == LOW) {
        uint32_t t0 = millis();
        while (digitalRead(PIN_BTN) == LOW && (millis() - t0) < 4000) {
            delay(20);
        }
        if (digitalRead(PIN_BTN) == LOW) {
            Serial.println("[wifi] BOOT held >3s — wiping credentials");
            WiFiManager wm;
            wm.resetSettings();
            s_prefs.clear();
            s_mdnsHost = String(MDNS_PREFIX);
        }
    }

    // Load saved suffix (if any) for the mDNS name and to pre-fill the portal.
    String savedSuffix = s_prefs.getString("host_suffix", "");
    strncpy(s_suffixBuf, savedSuffix.c_str(), sizeof(s_suffixBuf) - 1);

    WiFiManager wm;
    wm.setDebugOutput(true);
    wm.setConfigPortalTimeout(300); // 5 min, then reboot

    // Custom parameter for the host suffix.
    WiFiManagerParameter suffixParam("suffix", "Bin name suffix (e.g. kitchen)", s_suffixBuf, sizeof(s_suffixBuf));
    wm.addParameter(&suffixParam);

    // AP name shown during provisioning.
    String apName = String("SmartBin-Setup-") + g_state.chip_id.substring(0, 4);

    Serial.printf("[wifi] starting portal AP=%s (if not configured)\n", apName.c_str());

    // autoConnect: tries saved creds first, opens portal on failure.
    bool connected = wm.autoConnect(apName.c_str(), "smartbin"); // default AP passphrase
    if (!connected) {
        Serial.println("[wifi] portal timed out, rebooting");
        delay(500);
        ESP.restart();
    }

    // Persist the suffix the user entered (or keep the saved one).
    String entered(suffixParam.getValue());
    entered.trim();
    if (entered.length() > 0) {
        s_prefs.putString("host_suffix", entered);
        s_mdnsHost = buildHost(entered);
    } else {
        s_mdnsHost = buildHost(savedSuffix);
    }

    // mDNS: answer <host>.local
    if (MDNS.begin(s_mdnsHost.c_str())) {
        MDNS.addService("http", "tcp", HTTP_PORT);
        Serial.printf("[wifi] mDNS: http://%s.local\n", s_mdnsHost.c_str());
    } else {
        Serial.println("[wifi] MDNS.begin failed");
    }

    configureTime();
    Serial.printf("[wifi] joined WiFi, IP=%s\n", WiFi.localIP().toString().c_str());
}
