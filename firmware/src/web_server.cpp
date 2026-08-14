// web_server.cpp — AsyncWebServer exposing the Smart Bin JSON API on port 80.
//
// All responses carry permissive CORS headers so the PWA (served from a
// different origin like github.io) can call freely. JSON is built with
// ArduinoJson. Handlers are async callbacks — they never block loop(), so the
// 50 Hz vibration sampling keeps running while a request is served.

#include <Arduino.h>
#include "config.h"
#include "modules.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

static AsyncWebServer s_server(HTTP_PORT);

// Apply CORS + content type to every response.
static void cors(AsyncWebServerResponse* resp) {
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type");
    resp->addHeader("Cache-Control", "no-store");
}

static void sendJson(AsyncWebServerRequest* req, const String& body) {
    AsyncWebServerResponse* resp = req->beginResponse(200, "application/json", body);
    cors(resp);
    req->send(resp);
}

static void sendOk(AsyncWebServerRequest* req) {
    sendJson(req, "{\"ok\":true}");
}

// Parse a small JSON body from the request. Works for our tiny payloads.
static bool getJsonField(const String& body, const char* key, String& out) {
    // Naive scan: "key":"value"  or  "key":123.4
    String needle = String("\"") + key + "\":";
    int k = body.indexOf(needle);
    if (k < 0) return false;
    int i = k + needle.length();
    while (i < (int)body.length() && body[i] == ' ') i++;
    if (i >= (int)body.length()) return false;
    if (body[i] == '"') {
        int j = body.indexOf('"', i + 1);
        if (j < 0) return false;
        out = body.substring(i + 1, j);
        return true;
    }
    int j = i;
    while (j < (int)body.length() &&
           body[j] != ',' && body[j] != '}' && body[j] != ' ')
        j++;
    out = body.substring(i, j);
    return true;
}

// Callback for AsyncWebServer body upload (POST bodies arrive here).
static String s_lastBody;
static void onBody(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t total) {
    s_lastBody = String((const char*)data, len);
}

void webServerBegin() {
    // CORS preflight for any /api route.
    s_server.on("/api", HTTP_OPTIONS, [](AsyncWebServerRequest* req){ sendOk(req); });
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type");

    // --- GET /api/state ---
    s_server.on("/api/state", HTTP_GET, [](AsyncWebServerRequest* req) {
        JsonDocument doc;
        doc["fill_pct"]            = (int)(g_state.fill_pct + 0.5f);
        doc["distance_cm"]         = g_state.distance_cm;
        doc["empty_cm"]            = g_state.empty_cm;
        doc["full_cm"]             = g_state.full_cm;
        doc["calibrated"]          = g_state.calibrated;
        doc["sensor_out_of_range"] = g_state.sensor_out_of_range;
        doc["vibrating"]           = g_state.vibrating;
        doc["vib_duty_pct"]        = g_state.vib_duty_pct;
        doc["last_emptied"]        = g_state.last_emptied;
        doc["uptime_s"]            = (uint32_t)(millis() / 1000UL);
        doc["rssi"]                = WiFi.RSSI();
        String out; serializeJson(doc, out);
        sendJson(req, out);
    });

    // --- GET /api/calibration ---
    s_server.on("/api/calibration", HTTP_GET, [](AsyncWebServerRequest* req) {
        JsonDocument doc;
        doc["bin_name"]    = g_state.bin_name;
        doc["host_suffix"] = g_state.host_suffix;
        doc["empty_cm"]    = g_state.empty_cm;
        doc["full_cm"]     = g_state.full_cm;
        String out; serializeJson(doc, out);
        sendJson(req, out);
    });

    // --- POST /api/calibrate  { bin_name?, empty_cm?, full_cm? } ---
    // onBody captures the payload into s_lastBody first; this handler then
    // parses it and persists via calibration.cpp.
    s_server.on("/api/calibrate", HTTP_POST,
        [](AsyncWebServerRequest* req){
            String binName, emptyStr, fullStr;
            bool haveE = false, haveF = false;
            float emptyCm = 0, fullCm = 0;
            if (getJsonField(s_lastBody, "bin_name", binName)) { /* set below */ }
            if (getJsonField(s_lastBody, "empty_cm", emptyStr)) { emptyCm = emptyStr.toFloat(); haveE = true; }
            if (getJsonField(s_lastBody, "full_cm",  fullStr))  { fullCm  = fullStr.toFloat();  haveF = true; }
            if (haveE || haveF || binName.length()) {
                calibrationSet(binName, haveE ? emptyCm : 0.0f, haveF ? fullCm : 0.0f);
            }
            s_lastBody = "";
            sendOk(req);
        }, nullptr, onBody);

    // --- POST /api/calibrate/empty ---
    s_server.on("/api/calibrate/empty", HTTP_POST, [](AsyncWebServerRequest* req) {
        float cm = calibrationMeasureAndStoreEmpty();
        String body = (cm < 0)
            ? String("{\"ok\":false,\"error\":\"read_failed\"}")
            : String("{\"ok\":true,\"empty_cm\":") + String(cm, 1) + "}";
        sendJson(req, body);
    });

    // --- POST /api/calibrate/full ---
    s_server.on("/api/calibrate/full", HTTP_POST, [](AsyncWebServerRequest* req) {
        float cm = calibrationMeasureAndStoreFull();
        String body = (cm < 0)
            ? String("{\"ok\":false,\"error\":\"read_failed\"}")
            : String("{\"ok\":true,\"full_cm\":") + String(cm, 1) + "}";
        sendJson(req, body);
    });

    // --- GET /api/history?limit=50 ---
    s_server.on("/api/history", HTTP_GET, [](AsyncWebServerRequest* req) {
        size_t limit = 50;
        if (req->hasParam("limit")) {
            limit = (size_t)req->getParam("limit")->value().toInt();
            if (limit == 0) limit = 50;
        }
        String out; historyReadLast(limit, out);
        sendJson(req, out);
    });

    // --- GET /api/info ---
    s_server.on("/api/info", HTTP_GET, [](AsyncWebServerRequest* req) {
        JsonDocument doc;
        doc["id"]   = g_state.chip_id;
        doc["name"] = g_state.bin_name;
        doc["fw"]   = FW_VERSION;
        doc["mdns"] = wifiMdnsHost();
        String out; serializeJson(doc, out);
        sendJson(req, out);
    });

    // --- POST /api/reboot ---
    s_server.on("/api/reboot", HTTP_POST, [](AsyncWebServerRequest* req) {
        sendOk(req);
        delay(150);
        ESP.restart();
    });

    s_server.onNotFound([](AsyncWebServerRequest* req) {
        if (req->method() == HTTP_OPTIONS) { sendOk(req); return; }
        req->send(404, "application/json", "{\"error\":\"not_found\"}");
    });

    s_server.begin();
    Serial.println("[web] server listening on :80");
}
