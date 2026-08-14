#pragma once
// modules.h — forward declarations for all firmware modules, so main.cpp and
// web_server.cpp can call into them without each .cpp needing its own header.

#include "state.h"

// ultrasonic.cpp
void  ultrasonicBegin();
void  ultrasonicTick();          // call from loop() at >= 2 Hz
float ultrasonicCurrentCm();     // last filtered distance (cm)
float ultrasonicMeasureCm(uint8_t samples); // one-shot median (calibration)

// vibration.cpp
void vibrationBegin();
void vibrationTick();            // call from loop() as fast as possible
// Callback invoked when a sustained "emptied" pattern is confirmed.
// Implemented in main.cpp (writes history + updates last_emptied).
void onBinEmptied(uint32_t epochSeconds);

// calibration.cpp
void calibrationBegin();         // load NVS into g_state
float calibrationMeasureAndStoreEmpty(); // 10-shot median -> empty_cm -> NVS
float calibrationMeasureAndStoreFull();  // 10-shot median -> full_cm  -> NVS
void  calibrationSet(const String& binName, float emptyCm, float fullCm);
bool  calibrationIsReady();

// history.cpp
void historyBegin();             // mount LittleFS
void historyAppend(const char* type, uint32_t epochSeconds);
// Read up to `limit` most-recent events; JSON array string returned in `out`.
void historyReadLast(size_t limit, String& out);
void historyRotateIfLarge();

// wifi_provisioning.cpp
void wifiProvisioningBegin();    // WiFiManager captive portal + mDNS
bool wifiIsConnected();
String wifiMdnsHost();           // full hostname, e.g. "smartbin-kitchen"

// web_server.cpp
void webServerBegin();

// main.cpp owns the global state instance.
extern BinState g_state;
