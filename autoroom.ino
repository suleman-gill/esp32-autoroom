/*
 * =================================================================================
 * ESP32 Smart Auto-Room Controller  (v2 - optimized)
 * =================================================================================
 * Files: AutoRoom.ino + webpage.h  (keep both inside a folder named "AutoRoom")
 *
 * Features:
 *  - 4-channel relay control (Active LOW module supported)
 *  - 4 physical wall switches (proper debounce, 2-way toggle behaviour)
 *  - Mobile web app (dark mode, works offline, no external libraries)
 *  - Live sync across phones (status polling)
 *  - Wi-Fi station mode with auto-reconnect + fallback hotspot "AutoRoom-Setup"
 *    (fallback keeps retrying your router in the background)
 *  - mDNS: http://autoroom.local
 *  - Renaming, per-appliance auto-off timers, scenes
 *  - Power-outage state restore (flash writes are batched to reduce wear)
 *  - Optional password protection for the web app / API
 *
 * Board: "ESP32 Dev Module"   Libraries: only the built-in ESP32 core ones.
 * =================================================================================
 */

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "webpage.h"

// ---------------------------------------------------------------------------------
// CONFIGURATION
// ---------------------------------------------------------------------------------
#define NUM_RELAYS 4

// Relay outputs and wall switches (switch wired between GPIO and GND)
const uint8_t RELAY_PINS[NUM_RELAYS]  = { 26, 27, 32, 33 };
const uint8_t SWITCH_PINS[NUM_RELAYS] = { 18, 19, 21, 22 };

#define RELAY_ACTIVE_LOW true      // most 4-ch relay boards: LOW = ON
#define STATUS_LED 2

// Home Wi-Fi (2.4 GHz only - ESP32 cannot use 5 GHz)
const char* DEFAULT_WIFI_SSID = "YOUR_WIFI_SSID";
const char* DEFAULT_WIFI_PASS = "YOUR_WIFI_PASSWORD";

// Fallback hotspot
const char* AP_SSID = "AutoRoom-Setup";
const char* AP_PASS = "12345678";          // min 8 characters

const char* MDNS_HOST = "autoroom";        // http://autoroom.local

// Optional web password. Leave "" to disable. User name is always "admin".
// Strongly recommended: anyone on your Wi-Fi can switch your appliances otherwise.
#define WEB_USER "admin"
const char* WEB_PASSWORD = "";

// Set true only if USB power causes brown-out reboot loops. Prefer a better 5V supply.
#define DISABLE_BROWNOUT false

#define SWITCH_DEBOUNCE_MS   40
#define STATE_SAVE_DELAY_MS  1500     // batch flash writes
#define WIFI_CONNECT_TIMEOUT 15000
#define WIFI_RETRY_AP_MS     120000   // while in fallback AP, retry router every 2 min
#define NAME_MAX_LEN         24

// ---------------------------------------------------------------------------------
// STATE
// ---------------------------------------------------------------------------------
WebServer server(80);
Preferences prefs;

struct RelayChannel {
  bool     state;
  String   name;
  bool     timerActive;
  uint32_t timerEnd;        // millis() deadline
  uint8_t  stableSw;        // debounced switch level
  uint8_t  lastRawSw;
  uint32_t lastSwChange;
};

RelayChannel relays[NUM_RELAYS];

const char* DEFAULT_NAMES[NUM_RELAYS] = {
  "Main Light", "Ambient Light", "Bedside Light", "Ceiling Fan"
};

bool     stateDirty   = false;
uint32_t stateDirtyAt = 0;
bool     apMode       = false;
bool     mdnsStarted  = false;
uint32_t lastApRetry  = 0;
uint32_t rebootAt     = 0;
String   cfgSsid, cfgPass;

// ---------------------------------------------------------------------------------
// HELPERS
// ---------------------------------------------------------------------------------
inline int relayLevel(bool on) {
  return (on != RELAY_ACTIVE_LOW) ? HIGH : LOW;   // active-low: on -> LOW
}

void applyRelay(int i, bool on) {
  if (i < 0 || i >= NUM_RELAYS) return;
  if (relays[i].state != on) {
    relays[i].state = on;
    digitalWrite(RELAY_PINS[i], relayLevel(on));
    stateDirty = true;
    stateDirtyAt = millis();
    Serial.printf("[Relay %d: %s] -> %s\n", i + 1, relays[i].name.c_str(), on ? "ON" : "OFF");
  }
  if (!on) relays[i].timerActive = false;       // OFF always cancels the timer
}

void toggleRelay(int i) { applyRelay(i, !relays[i].state); }

void flushStateIfNeeded() {
  if (stateDirty && (millis() - stateDirtyAt) > STATE_SAVE_DELAY_MS) {
    stateDirty = false;
    for (int i = 0; i < NUM_RELAYS; i++) {
      char key[16];
      snprintf(key, sizeof(key), "state_%d", i);
      prefs.putBool(key, relays[i].state);
    }
  }
}

String jsonEscape(const String& s) {
  String o;
  o.reserve(s.length() + 4);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') { o += '\\'; o += c; }
    else if ((uint8_t)c >= 0x20) o += c;        // drop control characters
  }
  return o;
}

String cleanName(String n) {
  n.trim();
  String o;
  for (size_t i = 0; i < n.length() && o.length() < NAME_MAX_LEN; i++) {
    char c = n[i];
    if ((uint8_t)c >= 0x20 && c != 0x7F) o += c;
  }
  o.trim();
  return o;
}

bool authorized() {
  if (strlen(WEB_PASSWORD) == 0) return true;
  if (server.authenticate(WEB_USER, WEB_PASSWORD)) return true;
  server.requestAuthentication();
  return false;
}

#define REQUIRE_AUTH() if (!authorized()) return

// ---------------------------------------------------------------------------------
// WEB HANDLERS
// ---------------------------------------------------------------------------------
void handleRoot() {
  REQUIRE_AUTH();
  server.sendHeader("Cache-Control", "no-cache");
  server.send_P(200, "text/html", INDEX_HTML);
}

void handleStatus() {
  REQUIRE_AUTH();
  bool sta = (WiFi.status() == WL_CONNECTED);
  uint32_t now = millis();

  String j;
  j.reserve(640);
  j += "{\"ip\":\"";
  j += sta ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
  j += "\",\"mode\":\"";
  j += sta ? "wifi" : "hotspot";
  j += "\",\"rssi\":";
  j += sta ? String(WiFi.RSSI()) : String(0);
  j += ",\"relays\":[";

  for (int i = 0; i < NUM_RELAYS; i++) {
    uint32_t remaining = 0;
    if (relays[i].timerActive) {
      int32_t diff = (int32_t)(relays[i].timerEnd - now);
      if (diff > 0) remaining = (uint32_t)diff / 1000;
    }
    if (i) j += ',';
    j += "{\"id\":";            j += (i + 1);
    j += ",\"state\":";         j += relays[i].state ? "true" : "false";
    j += ",\"name\":\"";        j += jsonEscape(relays[i].name);
    j += "\",\"timerSeconds\":"; j += remaining;
    j += '}';
  }
  j += "]}";

  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", j);
}

void handleRelayControl() {
  REQUIRE_AUTH();
  if (!server.hasArg("id") || !server.hasArg("state")) {
    server.send(400, "text/plain", "Missing arguments");
    return;
  }
  int id = server.arg("id").toInt() - 1;
  if (id < 0 || id >= NUM_RELAYS) {
    server.send(400, "text/plain", "Invalid relay id");
    return;
  }
  applyRelay(id, server.arg("state").toInt() == 1);
  server.send(200, "text/plain", "OK");
}

void handleMasterControl() {
  REQUIRE_AUTH();
  if (!server.hasArg("action")) {
    server.send(400, "text/plain", "Missing action");
    return;
  }
  String a = server.arg("action");

  // Scenes: {Main, Ambient, Bedside, Fan}
  bool s[NUM_RELAYS];
  if      (a == "all_on")  { s[0] = s[1] = s[2] = s[3] = true; }
  else if (a == "all_off") { s[0] = s[1] = s[2] = s[3] = false; }
  else if (a == "night")   { s[0] = false; s[1] = false; s[2] = false; s[3] = true; }
  else if (a == "movie")   { s[0] = false; s[1] = true;  s[2] = false; s[3] = true; }
  else if (a == "reading") { s[0] = false; s[1] = false; s[2] = true;  s[3] = true; }
  else { server.send(400, "text/plain", "Unknown action"); return; }

  for (int i = 0; i < NUM_RELAYS; i++) applyRelay(i, s[i]);
  server.send(200, "text/plain", "OK");
}

void handleRename() {
  REQUIRE_AUTH();
  if (!server.hasArg("id") || !server.hasArg("name")) {
    server.send(400, "text/plain", "Missing arguments");
    return;
  }
  int id = server.arg("id").toInt() - 1;
  String n = cleanName(server.arg("name"));
  if (id < 0 || id >= NUM_RELAYS || n.length() == 0) {
    server.send(400, "text/plain", "Invalid id or name");
    return;
  }
  relays[id].name = n;
  char key[16];
  snprintf(key, sizeof(key), "name_%d", id);
  prefs.putString(key, n);
  server.send(200, "text/plain", "Renamed");
}

void handleTimer() {
  REQUIRE_AUTH();
  if (!server.hasArg("id") || !server.hasArg("minutes")) {
    server.send(400, "text/plain", "Missing arguments");
    return;
  }
  int id = server.arg("id").toInt() - 1;
  long minutes = server.arg("minutes").toInt();
  if (id < 0 || id >= NUM_RELAYS) {
    server.send(400, "text/plain", "Invalid id");
    return;
  }
  if (minutes > 0) {
    if (minutes > 1440) minutes = 1440;          // max 24 h
    applyRelay(id, true);
    relays[id].timerEnd = millis() + (uint32_t)minutes * 60000UL;
    relays[id].timerActive = true;
    Serial.printf("[Timer] Relay %d auto-off in %ld min\n", id + 1, minutes);
  } else {
    relays[id].timerActive = false;
    Serial.printf("[Timer] Cancelled for relay %d\n", id + 1);
  }
  server.send(200, "text/plain", "Timer configured");
}

void handleWifiConfig() {
  REQUIRE_AUTH();
  if (!server.hasArg("ssid")) {
    server.send(400, "text/plain", "Missing SSID");
    return;
  }
  String ssid = server.arg("ssid");
  String pass = server.hasArg("pass") ? server.arg("pass") : "";
  ssid.trim();
  if (ssid.length() == 0 || ssid.length() > 32 || pass.length() > 63 ||
      (pass.length() > 0 && pass.length() < 8)) {
    server.send(400, "text/plain", "Invalid SSID or password (8-63 chars)");
    return;
  }
  prefs.putString("wifi_ssid", ssid);
  prefs.putString("wifi_pass", pass);
  stateDirty = true;                 // make sure relay states are flushed before reboot
  stateDirtyAt = 0;
  flushStateIfNeeded();

  server.send(200, "text/plain", "Saved. Restarting...");
  rebootAt = millis() + 1000;        // restart from loop(), after the reply is sent
}

void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

// ---------------------------------------------------------------------------------
// INPUTS / TIMERS
// ---------------------------------------------------------------------------------
void checkPhysicalSwitches() {
  uint32_t now = millis();
  for (int i = 0; i < NUM_RELAYS; i++) {
    uint8_t raw = digitalRead(SWITCH_PINS[i]);
    if (raw != relays[i].lastRawSw) {            // edge: restart debounce window
      relays[i].lastRawSw = raw;
      relays[i].lastSwChange = now;
    }
    if (raw != relays[i].stableSw && (now - relays[i].lastSwChange) >= SWITCH_DEBOUNCE_MS) {
      relays[i].stableSw = raw;                  // signal held steady -> accept
      toggleRelay(i);                            // any flip of a 2-way switch toggles
      Serial.printf("[Switch %d] -> relay %s\n", i + 1, relays[i].state ? "ON" : "OFF");
    }
  }
}

void checkTimers() {
  uint32_t now = millis();
  for (int i = 0; i < NUM_RELAYS; i++) {
    if (relays[i].timerActive && (int32_t)(now - relays[i].timerEnd) >= 0) {
      Serial.printf("[Timer] Relay %d expired -> OFF\n", i + 1);
      applyRelay(i, false);                      // also clears timerActive
    }
  }
}

// ---------------------------------------------------------------------------------
// WI-FI
// ---------------------------------------------------------------------------------
void startMdns() {
  if (mdnsStarted) return;
  if (MDNS.begin(MDNS_HOST)) {
    MDNS.addService("http", "tcp", 80);
    mdnsStarted = true;
    Serial.printf("[mDNS] http://%s.local\n", MDNS_HOST);
  }
}

bool connectStation() {
  Serial.printf("[Wi-Fi] Connecting to \"%s\" ", cfgSsid.c_str());
  WiFi.persistent(false);
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(MDNS_HOST);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);                          // faster web response
  WiFi.setTxPower(WIFI_POWER_15dBm);
  WiFi.begin(cfgSsid.c_str(), cfgPass.c_str());

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < WIFI_CONNECT_TIMEOUT) {
    delay(300);
    digitalWrite(STATUS_LED, !digitalRead(STATUS_LED));
    Serial.print('.');
  }
  Serial.println();
  return WiFi.status() == WL_CONNECTED;
}

void startAccessPoint() {
  apMode = true;
  // AP+STA lets us keep retrying the router while the hotspot stays available
  WiFi.mode(cfgSsid.length() ? WIFI_AP_STA : WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  lastApRetry = millis();
  Serial.printf("[Wi-Fi AP] Join \"%s\" (password %s) then open http://%s\n",
                AP_SSID, AP_PASS, WiFi.softAPIP().toString().c_str());
}

void maintainWifi() {
  bool connected = (WiFi.status() == WL_CONNECTED);
  digitalWrite(STATUS_LED, connected ? HIGH : LOW);

  if (connected) {
    if (!mdnsStarted) startMdns();
    return;
  }
  mdnsStarted = false;                           // re-announce after reconnect

  if (apMode && cfgSsid.length() && (millis() - lastApRetry) > WIFI_RETRY_AP_MS) {
    lastApRetry = millis();
    Serial.println("[Wi-Fi] Retrying router from fallback mode...");
    WiFi.begin(cfgSsid.c_str(), cfgPass.c_str());
  }
}

// ---------------------------------------------------------------------------------
// SETUP
// ---------------------------------------------------------------------------------
void setup() {
#if DISABLE_BROWNOUT
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
#endif

  Serial.begin(115200);
  delay(300);

  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(STATUS_LED, LOW);

  Serial.println("\n=== ESP32 SMART AUTO-ROOM CONTROLLER v2 ===");

  prefs.begin("autoroom", false);

  // Relays: preload the saved level BEFORE enabling the output (no boot glitch)
  for (int i = 0; i < NUM_RELAYS; i++) {
    char k[16];
    snprintf(k, sizeof(k), "name_%d", i);
    relays[i].name = prefs.getString(k, DEFAULT_NAMES[i]);

    snprintf(k, sizeof(k), "state_%d", i);
    bool saved = prefs.getBool(k, false);

    relays[i].state = saved;
    relays[i].timerActive = false;
    digitalWrite(RELAY_PINS[i], relayLevel(saved));
    pinMode(RELAY_PINS[i], OUTPUT);

    pinMode(SWITCH_PINS[i], INPUT_PULLUP);
    delay(2);
    relays[i].stableSw = relays[i].lastRawSw = digitalRead(SWITCH_PINS[i]);
    relays[i].lastSwChange = millis();

    Serial.printf("[Relay %d: %s] restored %s\n", i + 1, relays[i].name.c_str(), saved ? "ON" : "OFF");
  }

  // Wi-Fi credentials: flash overrides defaults
  cfgSsid = prefs.getString("wifi_ssid", "");
  cfgPass = prefs.getString("wifi_pass", "");
  if (cfgSsid.length() == 0 && strlen(DEFAULT_WIFI_SSID) > 0 &&
      strcmp(DEFAULT_WIFI_SSID, "YOUR_WIFI_NAME") != 0) {
    cfgSsid = DEFAULT_WIFI_SSID;
    cfgPass = DEFAULT_WIFI_PASS;
  }

  bool connected = false;
  if (cfgSsid.length() > 0) connected = connectStation();

  if (connected) {
    digitalWrite(STATUS_LED, HIGH);
    Serial.printf("[Wi-Fi] Connected. Open http://%s\n", WiFi.localIP().toString().c_str());
    startMdns();
  } else {
    Serial.println("[Wi-Fi] Router not reached (check name/password, 2.4 GHz only).");
    startAccessPoint();
  }

  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/relay",  HTTP_POST, handleRelayControl);
  server.on("/api/master", HTTP_POST, handleMasterControl);
  server.on("/api/rename", HTTP_POST, handleRename);
  server.on("/api/timer",  HTTP_POST, handleTimer);
  server.on("/api/wifi",   HTTP_POST, handleWifiConfig);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("[Server] Ready.");
}

// ---------------------------------------------------------------------------------
// LOOP
// ---------------------------------------------------------------------------------
void loop() {
  server.handleClient();
  checkPhysicalSwitches();
  checkTimers();
  flushStateIfNeeded();

  static uint32_t lastWifiCheck = 0;
  if (millis() - lastWifiCheck > 2000) {
    lastWifiCheck = millis();
    maintainWifi();
  }

  if (rebootAt && (int32_t)(millis() - rebootAt) >= 0) ESP.restart();

  delay(1);   // yield to the Wi-Fi/TCP stack
}
