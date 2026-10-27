# 🏠 ESP32 Smart Auto-Room Controller (v2 - Offline & Cloud-Free)

[![Platform: ESP32](https://img.shields.io/badge/Platform-ESP32-blue.svg)](https://www.espressif.com/)
[![Framework: Arduino](https://img.shields.io/badge/Framework-Arduino-teal.svg)](https://www.arduino.cc/)
[![Dependencies: Zero](https://img.shields.io/badge/Dependencies-Zero%20External%20Libs-green.svg)](#)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

A high-performance, standalone 4-channel smart room automation controller powered by the ESP32. It provides synchronized two-way control over physical wall switches and a modern, glassmorphic dark-mode web application running 100% offline from the ESP32's flash memory (`PROGMEM`) with **zero cloud dependencies** and **no external libraries**.

---

## ✨ Key Features

- **⚡ Synchronized 2-Way Switching**: Physical wall switches and the mobile web app operate in a 2-way toggle configuration. Toggling a wall switch flips the appliance state immediately regardless of the web app status, and vice-versa.
- **🛡️ Glitch-Free Boot Protection**: Relay GPIOs are clamped to their inactive state at the earliest line of `setup()` to prevent relay coil clicks during boot/reset.
- **🔇 Multi-Sample EMI Noise Rejection**: Physical switch inputs use 20ms sampling requiring 8 consecutive stable readings (~160ms total) to eliminate false triggers caused by AC inductive load spikes (fans, fluorescent ballasts).
- **🔄 Power-Cut State Restoration (NVS Memory)**: Remembers each appliance's state across power outages and reboots using ESP32 `Preferences` non-volatile storage. Relays are tracked in a compact 32-bit bitmask (`relay_mask`) to minimize flash wear.
- **📱 Responsive Offline Web App**: Self-contained, responsive dark-mode dashboard with SVG iconography, animated toggles, progress bars, and modal sheets. Served directly from ESP32 flash without external CDNs or internet.
- **⏱️ Per-Appliance Auto-Off Timers**: Set custom auto-off countdown timers (15m, 30m, 1h, 2h, 4h, 8h, or custom up to 24 hours) for lights and fans, with real-time countdown display and cancellation.
- **🎭 One-Tap Quick Scenes**: Instant room mood presets:
  - ☾ **Night Mode**: All lights OFF, Fan stays ON.
  - ◷ **Movie Mode**: Ambient light ON, Fan ON, Main & Bedside lights OFF.
  - ▤ **Reading Mode**: Bedside light ON, Fan ON, Main & Ambient lights OFF.
  - ✦ **Bright / All ON**: Turns all 4 channels ON.
  - ○ **All OFF**: Turns all 4 channels OFF.
- **🏷️ In-App Device Renaming**: Customize appliance names directly from your browser; names persist in non-volatile flash storage.
- **📶 Fallback Hotspot & Background Auto-Reconnect**: If your router is unavailable, the ESP32 automatically broadcasts a fallback Wi-Fi hotspot (`AutoRoom-Setup` on `192.168.4.1`) while continuing background reconnection attempts to the router every 2 minutes.
- **🌐 Zero-Config mDNS**: Access the dashboard locally via `http://autoroom.local`.
- **🔒 Optional Password Protection**: Configurable HTTP Basic Authentication (`WEB_USER` / `WEB_PASSWORD`) protects web and API control on shared home networks.

---

## 📋 Hardware Pinout Configuration

| ESP32 Pin | Function | Connected Component | Logic / Mode |
|---|---|---|---|
| **GPIO 26** | Relay 1 Output | Relay Module `IN1` (Default: Main Light) | Active LOW (`LOW` = ON) |
| **GPIO 27** | Relay 2 Output | Relay Module `IN2` (Default: Ambient Light) | Active LOW (`LOW` = ON) |
| **GPIO 32** | Relay 3 Output | Relay Module `IN3` (Default: Bedside Light) | Active LOW (`LOW` = ON) |
| **GPIO 33** | Relay 4 Output | Relay Module `IN4` (Default: Ceiling Fan) | Active LOW (`LOW` = ON) |
| **GPIO 18** | Switch 1 Input | Wall Switch 1 dry contact to `GND` | Internal `INPUT_PULLUP` |
| **GPIO 19** | Switch 2 Input | Wall Switch 2 dry contact to `GND` | Internal `INPUT_PULLUP` |
| **GPIO 21** | Switch 3 Input | Wall Switch 3 dry contact to `GND` | Internal `INPUT_PULLUP` |
| **GPIO 22** | Switch 4 Input | Wall Switch 4 dry contact to `GND` | Internal `INPUT_PULLUP` |
| **GPIO 2** | Status Indicator | Onboard Blue LED | Blinking = Connecting, Solid = Connected |
| **VIN / 5V** | System Power | 5V 2A DC Regulated Power Supply | Shared with Relay Board `VCC` |
| **GND** | System Ground | Power Supply Ground | Common ground between ESP32 and Relay board |

> [!NOTE]
> Most standard 4-channel relay modules are **Active LOW** (`RELAY_ACTIVE_LOW true`), meaning sending a `LOW` logic signal energizes the relay coil. If your relay module is Active HIGH, set `RELAY_ACTIVE_LOW false` in `AutoRoom.ino`.

---

## ⚡ Circuit & Wiring Schematics

### 1. Low-Voltage DC Wiring (ESP32, Relay Module & Switches)

```
       +-------------------------------------------------------------+
       |                  5V 2A Regulated DC Supply                  |
       |             [ +5V ]                     [ GND ]             |
       +----------------+---------------------------+----------------+
                        |                           |
            +-----------+                           +-----------+
            |                                                   |
            v                                                   v
   +-----------------+                                 +-----------------+
   |   ESP32 Board   |                                 |4-Ch Relay Board |
   |                 |                                 |                 |
   | [ VIN / 5V ] <--+                                 | [ VCC ] <-------+ (5V)
   | [ GND ]      <--+-------------------------------->| [ GND ]         |
   |                 |                                 |                 |
   | [ GPIO 26 ] ----+-------------------------------->| [ IN1 ] (Relay 1)
   | [ GPIO 27 ] ----+-------------------------------->| [ IN2 ] (Relay 2)
   | [ GPIO 32 ] ----+-------------------------------->| [ IN3 ] (Relay 3)
   | [ GPIO 33 ] ----+-------------------------------->| [ IN4 ] (Relay 4)
   |                 |                                 +-----------------+
   | [ GPIO 18 ] ----+-----\  o Wall Switch 1
   | [ GPIO 19 ] ----+-----\  o Wall Switch 2
   | [ GPIO 21 ] ----+-----\  o Wall Switch 3
   | [ GPIO 22 ] ----+-----\  o Wall Switch 4
   |                 |       |
   | [ GND ] <-------+-------+ (All switches return to ESP32 GND)
   +-----------------+
```

### 2. High-Voltage AC Mains Wiring (Relay to Appliances)

```
   AC Mains Live (L) -----------------+--------+--------+--------+
                                      |        |        |        |
                                      v        v        v        v
                                    [COM1]   [COM2]   [COM3]   [COM4]
                                   +----------------------------------+
                                   |         4-Channel Relay          |
                                   +----------------------------------+
                                    [NO1]    [NO2]    [NO3]    [NO4]
                                      |        |        |        |
                                      v        v        v        v
                                   Appliance Appliance Appliance Appliance
                                      #1       #2       #3       #4
                                      |        |        |        |
   AC Mains Neutral (N) --------------+--------+--------+--------+
```

> [!CAUTION]
> **Mains Voltage Safety Warning**: AC mains voltage (110V-240V) can cause severe injury or death. Always disconnect mains power at the breaker panel before touching wiring. Use rated enclosures and ensure common ground between low-voltage supplies. Physical wall switches carry only low-voltage DC logic (3.3V) to `GND`. **Never connect AC mains to GPIO pins!**

---

## 📱 Web Application & User Interface

The embedded dashboard is fully self-contained in `webpage.h` and loaded into browser memory without external assets:

1. **Live Header & Connection Status**:
   - Real-time indicator dot: **Green** for connected Wi-Fi, **Amber** for fallback hotspot mode.
   - Shows connection mode and live RSSI signal strength in dBm.
   - Quick settings button to open the configuration dialog.
2. **Hero & Room Status Summary**:
   - Displays device IP address or mDNS host.
   - Live counter (`X / 4 on`) and progress bar showing room load percentage.
   - Human-readable status caption (*"All appliances are off"*, *"2 appliances running"*, etc.).
3. **Appliance Control Cards**:
   - Interactive toggle button with smooth slide animation.
   - Dynamic glow border and background color transition when energized.
   - Real-time active status label (`ON` / `OFF`) with status LED dot.
   - Auto-off timer readout showing remaining time (`Auto-off in 42 min`).
   - Per-appliance auto-off duration selector (15 min, 30 min, 1 hr, 2 hr, 4 hr, 8 hr) with **Set timer** and **Cancel timer** buttons.
   - Inline **Rename** button to customize channel names on the fly.
4. **Quick Scenes & Mood Presets**:
   - One-tap buttons for instant multi-relay automation:
     - **Night**: Fans on, lights off.
     - **Movie**: Ambient lighting on, main lights off.
     - **Reading**: Bedside lamp on, fan on.
     - **Bright**: All appliances energized.
     - **All off**: All appliances turned off.
5. **Master Controls**:
   - **Turn all on** / **Turn all off** buttons in the appliances section header.
6. **Room Settings Dialog**:
   - **Power-Up Startup Behavior**:
     - *Restore each appliance's last state* (Mode 0, Default): Remembers every channel separately across reboots.
     - *Always start all appliances OFF* (Mode 1): Safe mode after power cuts.
     - *Always start all appliances ON* (Mode 2): Turns all channels ON after reboot.
   - **Wi-Fi Setup Portal**:
     - Enter 2.4 GHz network SSID and password directly from your browser.
     - Automatically saves to NVS and restarts the controller to connect to the new router.
7. **Live Syncing & Toast Feedback**:
   - Automatically polls `/api/status` every 2.5 seconds to synchronize changes made by physical switches or other connected phones.
   - Toast popup notifications confirm every action and alert on connection failures.

---

## 🔌 REST API Documentation

The ESP32 runs a local HTTP REST API on port `80`:

### `GET /api/status`
Returns real-time controller state, network status, and channel details.
- **Response** `200 OK` (JSON):
```json
{
  "ip": "192.168.1.150",
  "mode": "wifi",
  "rssi": -58,
  "bootMode": 0,
  "relays": [
    { "id": 1, "state": true, "name": "Main Light", "timerSeconds": 0 },
    { "id": 2, "state": false, "name": "Ambient Light", "timerSeconds": 0 },
    { "id": 3, "state": false, "name": "Bedside Light", "timerSeconds": 0 },
    { "id": 4, "state": true, "name": "Ceiling Fan", "timerSeconds": 1800 }
  ]
}
```

### `POST /api/relay`
Toggles or sets an individual relay state.
- **Parameters** (Form URL-Encoded):
  - `id`: Relay index (`1` to `4`)
  - `state`: `1` (ON) or `0` (OFF)
- **Response**: `200 OK` (`OK`)

### `POST /api/master`
Applies room scenes or master switches.
- **Parameters**:
  - `action`: `all_on`, `all_off`, `night`, `movie`, or `reading`
- **Response**: `200 OK` (`OK`)

### `POST /api/timer`
Configures or cancels an auto-off timer.
- **Parameters**:
  - `id`: Relay index (`1` to `4`)
  - `minutes`: Timer duration in minutes (`1` to `1440`). Pass `0` to cancel an active timer.
- **Response**: `200 OK` (`Timer configured`)

### `POST /api/rename`
Renames an appliance channel.
- **Parameters**:
  - `id`: Relay index (`1` to `4`)
  - `name`: New name (string up to 24 characters)
- **Response**: `200 OK` (`Renamed`)

### `POST /api/bootmode`
Sets the startup behavior following power-on / restart.
- **Parameters**:
  - `mode`: `0` (Restore Last State), `1` (Always OFF), or `2` (Always ON)
- **Response**: `200 OK` (`Boot mode saved`)

### `POST /api/wifi`
Updates Wi-Fi credentials and triggers a restart.
- **Parameters**:
  - `ssid`: Network SSID (1–32 characters)
  - `pass`: Wi-Fi password (8–63 characters, or blank for open)
- **Response**: `200 OK` (`Saved. Restarting...`)

---

## 🚀 Installation & Getting Started

### 1. Requirements
- **Hardware**: ESP32 Dev Module (ESP-WROOM-32 or compatible), 4-Channel 5V Relay Board (Active LOW), 5V 2A DC power supply.
- **Software**: [Arduino IDE](https://www.arduino.cc/en/software) (2.x recommended) with ESP32 board support package installed.

### 2. Sketch Setup
1. Keep `AutoRoom.ino` and `webpage.h` inside a sketch folder named **`AutoRoom`**:
   ```
   AutoRoom/
   ├── AutoRoom.ino
   ├── webpage.h
   └── README.md
   ```
2. Open `AutoRoom.ino` in Arduino IDE.
3. Select **Board**: `ESP32 Dev Module`.
4. (Optional) In `AutoRoom.ino`, configure your default Wi-Fi name and password:
   ```cpp
   const char* DEFAULT_WIFI_SSID = "YOUR_WIFI_NAME";
   const char* DEFAULT_WIFI_PASS = "YOUR_WIFI_PASSWORD";
   ```
   *(Alternatively, leave placeholders and configure via the setup hotspot).*
5. (Optional) Set a web password to secure your dashboard:
   ```cpp
   const char* WEB_PASSWORD = "your_secure_password";
   ```
6. Click **Upload**.

### 3. First Connection & Usage
1. Open the Serial Monitor at **115200 baud**.
2. If router credentials are not set or unreachable, the ESP32 starts its fallback hotspot:
   - **SSID**: `AutoRoom-Setup`
   - **Password**: `12345678`
   - **IP**: `http://192.168.4.1`
3. Connect your phone or laptop to `AutoRoom-Setup`, open `http://192.168.4.1`, open **Settings → Wi-Fi setup**, enter your 2.4 GHz home network credentials, and click **Save Wi-Fi & restart**.
4. Once connected to your home Wi-Fi, open the IP address shown in the Serial Monitor or access:
   ```
   http://autoroom.local
   ```
5. Bookmark the address or use "Add to Home Screen" on iOS/Android for a native app experience!

---

## 🛠️ Configuration Reference

| Setting | Location in `AutoRoom.ino` | Default | Description |
|---|---|---|---|
| `RELAY_ACTIVE_LOW` | Line 40 | `true` | `true` for standard active-low relay boards, `false` for active-high |
| `ENABLE_PHYSICAL_SWITCHES` | Line 69 | `true` | Set `false` if no wall switches are wired to prevent floating pins |
| `DEFAULT_BOOT_MODE` | Line 50 | `BOOT_MODE_RESTORE` (`0`) | Startup mode: `0` = Restore, `1` = All OFF, `2` = All ON |
| `WEB_PASSWORD` | Line 65 | `""` (Disabled) | HTTP Basic Authentication password (username: `admin`) |
| `DISABLE_BROWNOUT` | Line 72 | `false` | Set `true` only if weak USB cables trigger brownout reset loops |
| `AP_SSID` / `AP_PASS` | Line 57-58 | `AutoRoom-Setup` / `12345678` | Fallback hotspot SSID and password |
| `MDNS_HOST` | Line 60 | `"autoroom"` | Local mDNS hostname (`http://autoroom.local`) |

---

## 📄 License

This project is licensed under the MIT License — free for personal and commercial use.
