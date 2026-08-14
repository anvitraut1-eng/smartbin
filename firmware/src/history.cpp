// history.cpp — append-only JSON-lines event log on LittleFS.
//
// Each event is one line:  {"t":1731000000,"type":"emptied"}
// When the file exceeds HISTORY_MAX_BYTES we rewrite it keeping only the most
// recent HISTORY_MAX_EVENTS entries. The HTTP /api/history endpoint returns
// the last N events as a JSON array built here.

#include <Arduino.h>
#include "config.h"
#include "modules.h"
#include <LittleFS.h>
#include <vector>

static const size_t LINE_BUF = 80;

void historyBegin() {
    if (!LittleFS.begin(true)) {       // true = format on failure
        Serial.println("[history] LittleFS mount failed");
    }
}

void historyAppend(const char* type, uint32_t epochSeconds) {
    File f = LittleFS.open(HISTORY_PATH, FILE_APPEND);
    if (!f) {
        Serial.println("[history] cannot open for append");
        return;
    }
    char line[LINE_BUF];
    snprintf(line, sizeof(line), "{\"t\":%lu,\"type\":\"%s\"}\n",
             (unsigned long)epochSeconds, type);
    f.write((const uint8_t*)line, strlen(line));
    f.close();
    historyRotateIfLarge();
}

void historyRotateIfLarge() {
    File f = LittleFS.open(HISTORY_PATH, "r");
    if (!f) return;
    if (f.size() <= HISTORY_MAX_BYTES) { f.close(); return; }

    // Read all lines, keep the tail.
    std::vector<String> lines;
    lines.reserve(HISTORY_MAX_EVENTS + 8);
    while (f.available()) {
        String ln = f.readStringUntil('\n');
        if (ln.length()) lines.push_back(ln);
    }
    f.close();

    if (lines.size() > HISTORY_MAX_EVENTS) {
        lines.erase(lines.begin(),
                    lines.begin() + (lines.size() - HISTORY_MAX_EVENTS));
    }
    File out = LittleFS.open(HISTORY_PATH, "w");
    if (!out) return;
    for (const String& ln : lines) {
        out.write((const uint8_t*)ln.c_str(), ln.length());
        out.write('\n');
    }
    out.close();
}

// Return the last `limit` events as a JSON array string:
//   {"events":[{"t":...,"type":"emptied"}, ...]}
void historyReadLast(size_t limit, String& out) {
    File f = LittleFS.open(HISTORY_PATH, "r");
    // Read all lines into a buffer, then slice the tail. History files are
    // capped at ~32 KB so this is cheap.
    std::vector<String> lines;
    if (f) {
        while (f.available()) {
            String ln = f.readStringUntil('\n');
            if (ln.length()) lines.push_back(ln);
        }
        f.close();
    }

    out.reserve(64 + lines.size() * 48);
    out = "{\"events\":[";
    size_t start = lines.size() > limit ? lines.size() - limit : 0;
    bool first = true;
    for (size_t i = start; i < lines.size(); i++) {
        if (!first) out += ',';
        out += lines[i];
        first = false;
    }
    out += "]}";
}
