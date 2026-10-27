# AutoRoom ESP32 controller

This folder contains the Arduino sketch and its embedded, mobile-friendly dashboard:

- `AutoRoom.ino` — ESP32 relay, wall-switch, Wi-Fi, timer, and API firmware.
- `webpage.h` — self-contained responsive web interface (no external assets or libraries).

## State restoration

In the dashboard, open **Settings → After ESP32 power-up** and select **Restore each appliance's last state**. In this mode, each relay restores independently: a channel that was ON returns ON, and a channel that was OFF remains OFF. Relay changes are written to a compact NVS state mask immediately. The first boot after updating from the earlier version migrates its `state_0` through `state_3` values. Timers themselves are not restored after a reboot.

A full flash erase or clearing the ESP32's NVS storage removes saved relay states. The other startup choices—always OFF or always ON—intentionally override the saved mask at startup.

If a relay still energizes unexpectedly, open the Serial Monitor at **115200 baud** and restart the ESP32. The log prints the saved relay mask and each channel's initialized ON/OFF state. If the GUI and log say OFF while the physical relay is ON, check whether your relay board is active-high; set `RELAY_ACTIVE_LOW` to `false` only for an active-high board. If the log itself says ON, confirm the saved channel states and choose Restore mode in the dashboard before restarting.

## Build and first connection

1. Open `AutoRoom.ino` in Arduino IDE with the ESP32 Arduino core installed. Keep `webpage.h` in the same `AutoRoom` sketch folder.
2. Select the board and port, then upload. The supplied network settings are placeholders; alternatively, leave them as-is and use the setup hotspot.
3. If the configured router is not available, join **AutoRoom-Setup** (password `12345678`) and open `http://192.168.4.1`. In **Settings → Wi-Fi setup**, enter a 2.4 GHz network and save; the ESP32 restarts after saving.
4. Once connected to the router, open the IP shown in the Serial Monitor at 115200 baud, or try `http://autoroom.local` if mDNS is supported on the client network.

Before uploading, check `RELAY_ACTIVE_LOW` and the GPIO assignments near the top of the sketch for your hardware. Firmware sets relay GPIOs to the configured OFF level as the first setup action to reduce startup glitches; some relay boards may still need suitable hardware biasing to keep inputs inactive while the ESP32 is unpowered or resetting. Change the empty `WEB_PASSWORD` to a strong password before exposing the controller to a shared network. Disconnect mains power before changing relay wiring; use properly rated hardware and enclosures.
