# Smart Meter ATS — Universal Firmware (ESP32)

Smart automatic energy meter switching system with **one universal firmware binary** that supports **1–10 meters** without recompilation.

The system reads live electrical values from a PZEM-004T V3 module, controls up to 10 relay-driven meter/load lines, and provides a built-in web dashboard over WiFi Access Point mode.

## Universal Firmware Architecture

Every customer receives **exactly the same firmware binary**. The differences between installations are only:

1. The number of relay modules physically installed on the PCB (the PCB carries 10 relay positions; unused positions stay empty).
2. The **Active Meters** value (1–10) selected once on the dashboard and stored permanently in NVS (Preferences).

Rules enforced by the design:

- Relay GPIOs are **permanently fixed in firmware** (single `RELAY_PINS[]` table) and never configurable from the dashboard.
- The dashboard and REST API work purely with meter indices — **GPIO numbers are never exposed** to the browser.
- All meter cards, limit rows, schedule logic, JSON arrays, and dropdown options are **generated dynamically** from the Active Meters value. Meters above the count are completely hidden and ignored.
- All per-meter state lives in arrays sized to `MAX_METERS` (10); every loop runs over the runtime `activeMeterCount`, never over literals.
- Settings for all 10 slots are always persisted, so increasing Active Meters later restores prior limits.

## Project Status

> Work in progress: not fully tested yet on all real-world conditions and hardware scenarios.

## Main Features

- Up to 10-channel relay switching (active-LOW outputs), count set at runtime
- Automatic meter switching based on per-meter kWh limit
- Live monitoring from PZEM-004T V3: Voltage, Current, Power, Energy
- Web dashboard at `http://192.168.4.1/` (fully dynamic UI)
- NVS (Preferences) persistence for all settings
- Daily and monthly usage tracking with 30-day graph
- Protection logic: over-voltage, under-voltage, over-current trip (cuts all relays)
- Emergency OFF (dashboard button and physical button)
- Dual-core design: PZEM polling on Core 0, web server/relay logic on Core 1
- Task watchdog

## Hardware

- ESP32 DevKit (WROOM-32)
- PZEM-004T V3 energy meter module
- DS3231 RTC module
- 1–10 relay modules (active-LOW), installed per customer order
- Emergency push button

## Pin Mapping (fixed — never changes between customers)

| Meter | GPIO | Meter | GPIO |
|-------|------|-------|------|
| 1 | 25 | 6  | 13 |
| 2 | 26 | 7  | 4  |
| 3 | 27 | 8  | 18 |
| 4 | 33 | 9  | 19 |
| 5 | 32 | 10 | 23 |

Other connections:

- GPIO16 — PZEM TX → ESP RX (UART2)
- GPIO17 — ESP TX → PZEM RX (UART2)
- GPIO21 — RTC SDA
- GPIO22 — RTC SCL
- GPIO35 — Emergency button (to GND, external ~10kΩ pull-up; input-only pin)

## WiFi Access Point

- SSID: `SmartMeterATS`
- Password: `12345678`
- Dashboard URL: `http://192.168.4.1/`

## REST API

All endpoints are HTTP GET at `192.168.4.1`. `N` = Active Meters.

| Endpoint | Parameters | Description |
|----------|------------|-------------|
| `/api/status` | — | Full system JSON (`meterCount`, arrays sized to N) |
| `/api/setActiveMeters` | `n` (1–10) | Set Active Meters, saved permanently |
| `/api/setLimits` | `l0`…`l{N-1}` (kWh) | Set per-meter limits |
| `/api/setEnabled` | `idx` (0–N-1), `val` (0/1) | Enable/disable a meter |
| `/api/switchMeter` | `m` (0–N-1) | Manual meter switch |
| `/api/resetEnergy` | — | Reset kWh counters |
| `/api/emergency` | — | All relays OFF |
| `/api/setProtection` | `ov`, `uv`, `oc` | Set protection thresholds |
| `/api/clearFault` | — | Clear protection trip |

## Commissioning a New Installation

1. Install the ordered number of relay modules on the PCB (positions 1..N in order).
2. Flash the standard firmware binary (same for every customer) — or ship pre-flashed.
3. Connect to the `SmartMeterATS` WiFi AP and open the dashboard.
4. In **System Configuration → Active Meters**, select the installed count and press **Apply**.
5. Done. The dashboard, scheduler, and protection logic adapt automatically.

## Required Libraries

Install from Arduino Library Manager:

- PZEM004Tv30 (Jakub Mandula, v1.1.2+)
- RTClib (Adafruit)
- ArduinoJson (Benoit Blanchon)

Bundled with the ESP32 Arduino core: WiFi, WebServer, Preferences, Wire.

## Arduino IDE Setup

1. Install the ESP32 board package (Boards Manager → "esp32" by Espressif).
2. Select board: `ESP32 Dev Module`.
3. Select the correct COM port.
4. Upload `SmartMeterATS_ESP32/SmartMeterATS_ESP32.ino`.

## Project Files

- `SmartMeterATS_ESP32/SmartMeterATS_ESP32.ino` — universal ESP32 firmware
- `PROJECT_GUIDE.txt` — legacy ESP8266 wiring/implementation guide (kept for reference)

## Safety Note

This project interacts with mains-connected equipment through relays and energy meter hardware. Use proper electrical isolation and follow qualified electrical safety practices.
