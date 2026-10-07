# 🏠 ESP32 Smart Auto-Room Controller (v2 - Offline & Cloud-Free)

[![Platform: ESP32](https://img.shields.io/badge/Platform-ESP32-blue.svg)](https://www.espressif.com/)
[![Framework: Arduino](https://img.shields.io/badge/Framework-Arduino-teal.svg)](https://www.arduino.cc/)
[![Dependencies: Zero](https://img.shields.io/badge/Dependencies-Zero%20External%20Libs-green.svg)](#)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

A high-performance, standalone 4-channel home automation controller powered by the ESP32. It enables synchronized control of room appliances through **physical wall switches** and a **modern mobile web application** running entirely on the ESP32 with **zero cloud dependencies** and **no external libraries**.

---

## ✨ Key Features

- **⚡ Dual 2-Way Switching**: Physical wall switches and the web application operate seamlessly in a 2-way toggle setup. Flipping a physical switch immediately toggles the appliance regardless of its previous web state.
- **📱 Embedded Offline Web App**: Complete responsive dashboard (Dark/Light theme) stored in flash memory (`PROGMEM`). Works offline without requiring internet access or external CDN assets.
- **🔄 State Persistence (Power-Cut Memory)**: Retains appliance states across reboots or power outages using ESP32 `Preferences` (NVS). State writes are debounced/batched to maximize flash memory lifespan.
- **⏱️ Per-Appliance Auto-Off Timers**: Set custom countdown timers (15m, 30m, 1h, 2h, or up to 24h) for fans and lights directly from the interface.
- **🎭 Lighting Presets & Scenes**: Instant one-tap presets: *All ON*, *All OFF*, *Night Mode*, *Movie Mode*, and *Reading Mode*.
- **📶 Fallback Access Point & Auto-Reconnect**: If the router is unreachable, the ESP32 automatically spawns an `AutoRoom-Setup` Wi-Fi hotspot (`192.168.4.1`) with a built-in configuration portal, while continuing background reconnect attempts to your router.
- **🏷️ In-App Device Renaming**: Customize appliance names directly from your smartphone browser; names are saved permanently to non-volatile flash.
- **🌐 Zero-Config mDNS**: Access the web controller on local networks via `http://autoroom.local`.

---

## 📋 Hardware Pinout

| ESP32 Pin | Function | Connected Component | Logic / Mode |
|---|---|---|---|
| **GPIO 26** | Relay 1 Output | Relay Module `IN1` (Main Light) | Active LOW |
| **GPIO 27** | Relay 2 Output | Relay Module `IN2` (Ambient Light) | Active LOW |
| **GPIO 32** | Relay 3 Output | Relay Module `IN3` (Bedside Light) | Active LOW |
| **GPIO 33** | Relay 4 Output | Relay Module `IN4` (Ceiling Fan) | Active LOW |
| **GPIO 18** | Switch 1 Input | Physical Wall Switch 1 to `GND` | Internal `INPUT_PULLUP` |
| **GPIO 19** | Switch 2 Input | Physical Wall Switch 2 to `GND` | Internal `INPUT_PULLUP` |
| **GPIO 21** | Switch 3 Input | Physical Wall Switch 3 to `GND` | Internal `INPUT_PULLUP` |
| **GPIO 22** | Switch 4 Input | Physical Wall Switch 4 to `GND` | Internal `INPUT_PULLUP` |
| **GPIO 2** | Status Indicator | Onboard Blue LED | Blinking = Connecting, Solid = Connected |
| **VIN / 5V** | System Power | 5V 2A DC Power Supply | Shared with Relay VCC |
| **GND** | System Ground | Common Ground | Common to ESP32 & Relay GND |

---

## ⚡ Circuit & Wiring Schematics

### 1. Low-Voltage DC Connections

```
        +--------------------------------------------------------+
        |                 5V 2A DC Power Supply                  |
        |              [ +5V ]            [ GND ]                |
        +-----------------+------------------+-------------------+
                          |                  |
              +-----------+                  +------------+
              |                                           |
              v                                           v
    +-------------------+                       +-------------------+
    |    ESP32 Board    |                       | 4-Ch Relay Module |
    |                   |                       |                   |
    |  [ VIN / 5V ] <---+                       |  [ VCC ] <--------+ (5V)
    |  [ GND ]      <-------------------------> |  [ GND ] <--------+ (GND)
    |                   |                       |                   |
    |  [ GPIO 26 ] ------> (IN1: Main Light) -> |  [ IN1 ]          |
    |  [ GPIO 27 ] ------> (IN2: Ambient) ----> |  [ IN2 ]          |
    |  [ GPIO 32 ] ------> (IN3: Bedside) ----> |  [ IN3 ]          |
    |  [ GPIO 33 ] ------> (IN4: Ceiling Fan)-> |  [ IN4 ]          |
    |                   |                       +-------------------+
    |                   |
    |  [ GPIO 18 ] -----+---> [ Wall Switch 1 ] ---+
    |  [ GPIO 19 ] -----+---> [ Wall Switch 2 ] ---+
    |  [ GPIO 21 ] -----+---> [ Wall Switch 3 ] ---+
    |  [ GPIO 22 ] -----+---> [ Wall Switch 4 ] ---+
    |                   |                          |
    |  [ GND ] <--------+--------------------------+ (Switches connect to GND)
    +-------------------+
```

> **Safety Note on Switches**: Physical switches connect between the respective GPIO and **GND**. The ESP32's internal pull-up resistors supply 3.3V at microamps of current, making the wall switch wiring completely isolated and low voltage.

---

### 2. High-Voltage AC Mains Wiring (220V / 110V)

> ⚠️ **DANGER - HIGH VOLTAGE**: Disconnect main breakers before wiring AC mains. Keep all high-voltage connections isolated inside an enclosure box.

```
       [ AC Mains 220V / 110V ]
         |
         +--- LIVE (L) ----------------+
         |                             |
         +--- NEUTRAL (N) -------+     |
                                 |     |
                                 |     v
                      +----------|-----------------------+
                      |          |     Relay (NO Mode)   |
                      |          |     [ COM ] <---------+ (Mains Live)
                      |          |     [ NO  ] ----+
                      |          |     [ NC  ]     |
                      |          +-----------------|-----+
                      |                            |
                      |                            v
                      |                       [Appliance]
                      |                            |
                      +----------------------------+ (Neutral Return)
```

---

## 🛠️ Installation & Flashing Guide

### Requirements
- **Arduino IDE** (v1.8.19 or v2.x)
- **ESP32 Board Package**: Install via Arduino Board Manager (`esp32` by Espressif Systems).
- **Libraries**: No third-party libraries required. Uses standard built-in ESP32 core libraries:
  - `<WiFi.h>`
  - `<WebServer.h>`
  - `<ESPmDNS.h>`
  - `<Preferences.h>`

### Steps
1. Place both `autoroom.ino` and `webpage.h` inside a folder named **`autoroom`**.
2. Open `autoroom.ino` in Arduino IDE.
3. In the Arduino IDE menu:
   - **Tools -> Board**: Select `ESP32 Dev Module` (or your specific ESP32 variant).
   - **Tools -> Flash Frequency**: `80MHz`.
   - **Tools -> Upload Speed**: `921600` (or `115200`).
   - **Tools -> Port**: Select your ESP32 COM port.
4. (Optional) Set your initial Wi-Fi credentials in `autoroom.ino`:
   ```cpp
   const char* DEFAULT_WIFI_SSID = "YOUR_WIFI_SSID";
   const char* DEFAULT_WIFI_PASS = "YOUR_WIFI_PASSWORD";
   ```
5. Click **Upload**.

---

## 📡 First-Time Wi-Fi Setup

If no Wi-Fi credentials are set, or if the configured router is out of range:
1. The ESP32 starts its fallback hotspot:
   - **SSID**: `AutoRoom-Setup`
   - **Password**: `12345678`
2. Connect to the hotspot from your smartphone or computer.
3. Open a browser and navigate to:
   - **`http://192.168.4.1`**
4. Click the **Gear icon (⚙️)** in the top right corner, input your home 2.4 GHz Wi-Fi details, and click **Save**.
5. The ESP32 will reboot and connect to your home Wi-Fi network. Access it at `http://autoroom.local` or its assigned local IP address.

---

## 🔌 REST API Reference

The device exposes standard HTTP endpoints for automation integration (Home Assistant, Shortcuts, Tasker, cURL, etc.):

| Endpoint | Method | Parameters | Description |
|---|---|---|---|
| `/api/status` | `GET` | — | Returns JSON payload with device IP, mode, RSSI, and relay states. |
| `/api/relay` | `POST` | `id` (1–4), `state` (0 or 1) | Turn a specific relay ON or OFF. |
| `/api/master` | `POST` | `action` (`all_on`, `all_off`, `night`, `movie`, `reading`) | Execute multi-relay preset scenes. |
| `/api/timer` | `POST` | `id` (1–4), `minutes` (1–1440, or 0 to cancel) | Configure auto-off countdown timer. |
| `/api/rename` | `POST` | `id` (1–4), `name` (string) | Rename an appliance in non-volatile flash. |
| `/api/wifi` | `POST` | `ssid`, `pass` | Update Wi-Fi credentials and trigger soft reboot. |

### Example cURL Commands:
```bash
# Toggle Relay 1 ON
curl -X POST "http://autoroom.local/api/relay" -d "id=1&state=1"

# Activate Night Scene
curl -X POST "http://autoroom.local/api/master" -d "action=night"

# Set a 30-minute auto-off timer on Relay 4 (Fan)
curl -X POST "http://autoroom.local/api/timer" -d "id=4&minutes=30"
```

---

## 📁 Repository Structure

```
autoroom/
├── autoroom.ino        # Main ESP32 firmware & HTTP API server
├── webpage.h           # Embedded zero-dependency responsive web application
├── Wiring_Circuit.md   # Circuit diagrams, pinout tables, and AC wiring guide
└── README.md           # Project documentation and setup guide
```

---

## 📄 License
This project is open-source under the MIT License.
