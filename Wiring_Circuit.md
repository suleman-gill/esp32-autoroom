# ESP32 Smart Auto-Room Automation Controller

An ESP32-based 4-channel smart room automation system controllable via any smartphone browser with **zero cloud dependencies**, **instant offline local control**, **wall switch integration**, **auto-restore memory**, and **built-in countdown timers**.

---

## 📑 Hardware Pinout Summary

| ESP32 Pin | Function | Connected To | Notes |
|---|---|---|---|
| **GPIO 26** | Relay 1 Control | Relay Module IN1 (Light 1 - Main Light) | Active LOW |
| **GPIO 27** | Relay 2 Control | Relay Module IN2 (Light 2 - Ambient Light) | Active LOW |
| **GPIO 32** | Relay 3 Control | Relay Module IN3 (Light 3 - Bedside Light) | Active LOW |
| **GPIO 33** | Relay 4 Control | Relay Module IN4 (Ceiling Fan) | Active LOW |
| **GPIO 18** | Physical Switch 1 | Wall Switch 1 (Light 1) to GND | Internal Pull-Up (`INPUT_PULLUP`) |
| **GPIO 19** | Physical Switch 2 | Wall Switch 2 (Light 2) to GND | Internal Pull-Up (`INPUT_PULLUP`) |
| **GPIO 21** | Physical Switch 3 | Wall Switch 3 (Light 3) to GND | Internal Pull-Up (`INPUT_PULLUP`) |
| **GPIO 22** | Physical Switch 4 | Wall Switch 4 (Fan) to GND | Internal Pull-Up (`INPUT_PULLUP`) |
| **5V / VIN** | Power Input | 5V 2A DC Power Supply | Shared with Relay VCC |
| **GND** | Ground | Common Ground | Connect ESP32 GND and Relay GND |

---

## ⚡ Complete Circuit & Wiring Diagram

### 1. Low-Voltage DC Wiring (ESP32, Relay Module & Physical Wall Switches)

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
    |  [ GPIO 26 ] ------> (IN1: Light 1) ----> |  [ IN1 ]          |
    |  [ GPIO 27 ] ------> (IN2: Light 2) ----> |  [ IN2 ]          |
    |  [ GPIO 32 ] ------> (IN3: Light 3) ----> |  [ IN3 ]          |
    |  [ GPIO 33 ] ------> (IN4: Ceiling Fan)-> |  [ IN4 ]          |
    |                   |                       +-------------------+
    |                   |
    |  [ GPIO 18 ] -----+---> [ Wall Switch 1 (Light 1) ] ---+
    |  [ GPIO 19 ] -----+---> [ Wall Switch 2 (Light 2) ] ---+
    |  [ GPIO 21 ] -----+---> [ Wall Switch 3 (Light 3) ] ---+
    |  [ GPIO 22 ] -----+---> [ Wall Switch 4 (Fan) ] -------+
    |                   |                          |
    |  [ GND ] <--------+--------------------------+ (Switches connect to GND)
    +-------------------+
```

> **Why Switch to GND?**  
> We use ESP32's built-in `INPUT_PULLUP` resistors. The wall switch only carries 3.3V at microamps of current to ground. It is completely safe to touch and does not require external resistors!

---

### 2. High-Voltage AC Mains Wiring (220V / 110V Appliances)

> ⚠️ **SAFETY WARNING**:  
> Always turn off your main room circuit breaker / MCB before touching 220V/110V wires. Ensure all relay terminal screws are insulated and mounted in a protective plastic enclosure box.

Each relay on the module has 3 screw terminals:
- **COM (Common)**: Connects to the AC Live / Phase wire from mains power.
- **NO (Normally Open)**: Connects to the Appliance Live input wire.
- **NC (Normally Closed)**: Left unconnected for standard light/fan switches.

```
       [ AC Mains 220V / 110V ]
         |
         +--- LIVE (L) ----------------+
         |                             |
         +--- NEUTRAL (N) -------+     |
                                 |     |
                                 |     v
                      +----------|-----------------------+
                      |          |     Relay 1 (NO)      |
                      |          |     [ COM ] <---------+ (Mains Live)
                      |          |     [ NO  ] ----+
                      |          |     [ NC  ]     |
                      |          +-----------------|-----+
                      |                            |
                      |                            v
                      |                    (Light 1 / Bulb)
                      |                            |
                      +----------------------------+ (Neutral return)
```

Repeat the same for Relay 2 (Light 2), Relay 3 (Ceiling Fan), and Relay 4 (Socket/Appliance).

---

## 📦 Required Components Checklist

1. **ESP32 Development Board** (ESP-WROOM-32, 30-pin or 38-pin).
2. **4-Channel 5V Relay Module** (Optocoupler-isolated, active LOW).
3. **5V 2A DC Power Supply / Adapter** (Hi-Link HLK-PM01 or standard 5V 2A USB power adapter).
4. **4x Standard Wall Switches or Push Buttons** (existing home switches can be reused!).
5. **Jumper Wires & Connecting Wires** (22-26 AWG for DC, 1.5mm² / 16 AWG for 220V AC).
6. **Plastic Junction / Enclosure Box** (for safe mounting).
