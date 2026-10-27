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
 *  - Per-relay power-outage state restore (one compact NVS state mask per change)
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

#define RELAY_ACTIVE_LOW true      // most 4-ch relay boards: LOW = ON (set false if Active HIGH)
#define STATUS_LED 2

// Power-on / Reboot Appliance Behavior:
// 0 = RESTORE_LAST_STATE (Default: Remembers previous state; if OFF it stays OFF, if ON it stays ON)
// 1 = ALWAYS_OFF         (Safe: All appliances start OFF after power cut)
// 2 = ALWAYS_ON          (All appliances turn ON after power cut)
#define BOOT_MODE_RESTORE    0
#define BOOT_MODE_ALWAYS_OFF 1
#define BOOT_MODE_ALWAYS_ON  2
#define DEFAULT_BOOT_MODE    BOOT_MODE_RESTORE

// Home Wi-Fi (2.4 GHz only - ESP32 cannot use 5 GHz)
const char* DEFAULT_WIFI_SSID = "YOUR_WIFI_NAME";
const char* DEFAULT_WIFI_PASS = "YOUR_WIFI_PASSWORD";

// Fallback hotspot
const char* AP_SSID = "AutoRoom-Setup";
const char* AP_PASS = "12345678";          // min 8 characters

const char* MDNS_HOST = "autoroom";        // http://autoroom.local

// Optional web password. Leave "" to disable. User name is always "admin".
// Strongly recommended: anyone on your Wi-Fi can switch your appliances otherwise.
#define WEB_USER "admin"
const char* WEB_PASSWORD = "";

// Set true if you have physical wall switches wired to GPIO 18, 19, 21, 22.
// Set false if you only use the web app / phone (prevents floating pins from randomly toggling relays).
#define ENABLE_PHYSICAL_SWITCHES true

// Set true only if USB power causes brown-out reboot loops. Prefer a better 5V supply.
#define DISABLE_BROWNOUT false

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
  uint8_t  swCounter;       // consecutive sample counter for EMI noise rejection
};

RelayChannel relays[NUM_RELAYS];

const char* DEFAULT_NAMES[NUM_RELAYS] = {
  "Main Light", "Ambient Light", "Bedside Light", "Ceiling Fan"
};

uint8_t  bootMode     = DEFAULT_BOOT_MODE;
bool     prefsReady   = false;
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

uint32_t relayStateMask() {
  uint32_t mask = 0;
  for (int i = 0; i < NUM_RELAYS; i++) {
    if (relays[i].state) mask |= (1UL << i);
  }
  return mask;
}

bool persistRelayStates() {
  if (!prefsReady) return false;
  // A single Preferences value keeps the four channels' state together.
  uint32_t expected = relayStateMask();
  size_t written = prefs.putUInt("relay_mask", expected);
  uint32_t verified = prefs.getUInt("relay_mask", 0xFFFFFFFFUL);
  bool ok = (written == sizeof(uint32_t)) && (verified == expected);
  if (!ok) Serial.printf("[NVS] ERROR: relay mask write/readback failed (expected 0x%08lX, read 0x%08lX)\n", (unsigned long)expected, (unsigned long)verified);
  return ok;
}

void applyRelay(int i, bool on) {
  if (i < 0 || i >= NUM_RELAYS) return;
  if (relays[i].state != on) {
    relays[i].state = on;
    digitalWrite(RELAY_PINS[i], relayLevel(on));
    bool saved = persistRelayStates();
    Serial.printf("[Relay %d: %s] -> %s (%s)\n", i + 1, relays[i].name.c_str(), on ? "ON" : "OFF", saved ? "saved" : "NOT SAVED");
  }
  if (!on) relays[i].timerActive = false;       // OFF always cancels the timer
}

void toggleRelay(int i) { applyRelay(i, !relays[i].state); }

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
  j += ",\"bootMode\":";
  j += bootMode;
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

void handleBootMode() {
  REQUIRE_AUTH();
  if (!prefsReady) { server.send(503, "text/plain", "Persistent storage unavailable"); return; }
  if (!server.hasArg("mode")) {
    server.send(400, "text/plain", "Missing mode");
    return;
  }
  int m = server.arg("mode").toInt();
  if (m < 0 || m > 2) {
    server.send(400, "text/plain", "Invalid mode (0=Restore, 1=Off, 2=On)");
    return;
  }
  bootMode = (uint8_t)m;
  if (prefs.putUChar("boot_mode", bootMode) != sizeof(uint8_t)) {
    server.send(500, "text/plain", "Could not save boot mode");
    return;
  }
  Serial.printf("[Config] Boot mode set to: %d (%s)\n", bootMode,
                bootMode == 0 ? "Restore Last State" : (bootMode == 1 ? "Always OFF" : "Always ON"));
  server.send(200, "text/plain", "Boot mode saved");
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
  if (!prefsReady) { server.send(503, "text/plain", "Persistent storage unavailable"); return; }
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
  if (!prefsReady) { server.send(503, "text/plain", "Persistent storage unavailable"); return; }
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
  if (!persistRelayStates()) {
    server.send(500, "text/plain", "Could not save relay states");
    return;
  }

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
  if (!ENABLE_PHYSICAL_SWITCHES) return;
  if (millis() < 2000) return;   // Prevent false triggers during power-on stabilization

  static uint32_t lastSampleTime = 0;
  uint32_t now = millis();
  if (now - lastSampleTime < 20) return;   // 20ms sample rate (spans 50Hz/60Hz AC period)
  lastSampleTime = now;

  for (int i = 0; i < NUM_RELAYS; i++) {
    uint8_t raw = digitalRead(SWITCH_PINS[i]);
    if (raw != relays[i].stableSw) {
      relays[i].swCounter++;
      // Require 8 consecutive stable readings (~160ms) to confirm genuine switch flip
      if (relays[i].swCounter >= 8) {
        relays[i].stableSw = raw;
        relays[i].swCounter = 0;
        toggleRelay(i);
        Serial.printf("[Switch %d] -> relay %s\n", i + 1, relays[i].state ? "ON" : "OFF");
      }
    } else {
      relays[i].swCounter = 0;             // Reset on any noise glitch
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
  // Establish the inactive relay level immediately. Active-low relay inputs can
  // otherwise float during the serial/NVS/Wi-Fi startup delay and click ON.
  for (int i = 0; i < NUM_RELAYS; i++) {
    digitalWrite(RELAY_PINS[i], relayLevel(false));
    pinMode(RELAY_PINS[i], OUTPUT);
    relays[i].state = false;
  }

#if DISABLE_BROWNOUT
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
#endif

  Serial.begin(115200);
  delay(300);

  // Initialize NVS Preferences storage (CRITICAL for persisting state across reboots)
  prefsReady = prefs.begin("autoroom", false);
  if (!prefsReady) {
    Serial.println("[NVS] Warning: Failed to open 'autoroom' preferences namespace!");
  } else {
    Serial.println("[NVS] Preferences storage active.");
  }

  pinMode(STATUS_LED, OUTPUT);
  Serial.println("\n=== ESP32 SMART AUTO-ROOM CONTROLLER v2 ===");
  esp_reset_reason_t rstReason = esp_reset_reason();
  if (rstReason == ESP_RST_BROWNOUT) {
    Serial.println("[WARNING] Brownout detected! Your 5V power supply dropped below operating voltage.");
  } else if (rstReason == ESP_RST_POWERON) {
    Serial.println("[System] Normal power-on reset.");
  } else {
    Serial.printf("[System] Reset reason code: %d\n", (int)rstReason);
  }

  // Read saved boot mode (0=Restore last state, 1=Always start OFF, 2=Always start ON)
  bootMode = prefsReady ? prefs.getUChar("boot_mode", DEFAULT_BOOT_MODE) : DEFAULT_BOOT_MODE;
  if (bootMode > BOOT_MODE_ALWAYS_ON) bootMode = DEFAULT_BOOT_MODE;
  uint32_t savedMask = prefsReady ? prefs.getUInt("relay_mask", 0xFFFFFFFFUL) : 0xFFFFFFFFUL;
  bool hasSavedMask = (savedMask != 0xFFFFFFFFUL);
  Serial.printf("[NVS] Saved relay mask: 0x%08lX (%s)\n",
                (unsigned long)savedMask, hasSavedMask ? "found" : "not found; checking legacy channel keys");
  Serial.printf("[System] Boot Mode: %s (%d)\n",
                bootMode == BOOT_MODE_RESTORE ? "Restore Previous State" :
                (bootMode == BOOT_MODE_ALWAYS_OFF ? "Always Start OFF" : "Always Start ON"),
                bootMode);

  // Apply each saved relay level; pins were initialized to OFF at the start of setup().
  for (int i = 0; i < NUM_RELAYS; i++) {
    char k[16];
    snprintf(k, sizeof(k), "name_%d", i);
    relays[i].name = prefsReady ? prefs.getString(k, DEFAULT_NAMES[i]) : String(DEFAULT_NAMES[i]);

    bool initialState = false;
    if (bootMode == BOOT_MODE_RESTORE) {
      if (hasSavedMask) {
        initialState = (savedMask & (1UL << i)) != 0;
      } else if (prefsReady) {
        // Migrate installations that used the earlier per-channel state keys.
        snprintf(k, sizeof(k), "state_%d", i);
        initialState = prefs.getBool(k, false);
      }
    } else if (bootMode == BOOT_MODE_ALWAYS_ON) {
      initialState = true;
    } else {
      initialState = false; // BOOT_MODE_ALWAYS_OFF
    }

    relays[i].state = initialState;
    relays[i].timerActive = false;
    digitalWrite(RELAY_PINS[i], relayLevel(initialState));

    Serial.printf("[Relay %d: %s] initialized %s\n", i + 1, relays[i].name.c_str(), initialState ? "ON" : "OFF");
  }
  // Migrate old state_0..state_3 entries, or save effective forced-mode states.
  if (bootMode != BOOT_MODE_RESTORE || !hasSavedMask) persistRelayStates();

  // Physical wall switches: stabilize pull-ups before taking initial baseline reading
  if (ENABLE_PHYSICAL_SWITCHES) {
    for (int i = 0; i < NUM_RELAYS; i++) {
      pinMode(SWITCH_PINS[i], INPUT_PULLUP);
    }
    delay(50); // Give internal pull-ups time to charge capacitance on switch lines
    for (int i = 0; i < NUM_RELAYS; i++) {
      relays[i].stableSw = digitalRead(SWITCH_PINS[i]);
      relays[i].swCounter = 0;
    }
  }

  // Wi-Fi credentials: flash overrides defaults
  cfgSsid = prefsReady ? prefs.getString("wifi_ssid", "") : "";
  cfgPass = prefsReady ? prefs.getString("wifi_pass", "") : "";
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
  server.on("/api/status",   HTTP_GET,  handleStatus);
  server.on("/api/relay",    HTTP_POST, handleRelayControl);
  server.on("/api/bootmode", HTTP_POST, handleBootMode);
  server.on("/api/master",   HTTP_POST, handleMasterControl);
  server.on("/api/rename",   HTTP_POST, handleRename);
  server.on("/api/timer",    HTTP_POST, handleTimer);
  server.on("/api/wifi",     HTTP_POST, handleWifiConfig);
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

  static uint32_t lastWifiCheck = 0;
  if (millis() - lastWifiCheck > 2000) {
    lastWifiCheck = millis();
    maintainWifi();
  }

  if (rebootAt && (int32_t)(millis() - rebootAt) >= 0) ESP.restart();

  delay(1);   // yield to the Wi-Fi/TCP stack
}
