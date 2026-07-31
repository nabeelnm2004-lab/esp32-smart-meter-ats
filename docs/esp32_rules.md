# ESP32 Rules

This document is the single source of truth for all ESP32 hardware assignments and development rules.

---

# GPIO Assignment Rules

## Fixed Hardware Assignment

These GPIO assignments are permanent and must not be changed without updating this document.

| Peripheral | GPIO |
|------------|------|
| Relay 1 | GPIO25 |
| Relay 2 | GPIO26 |
| Relay 3 | GPIO27 |
| Relay 4 | GPIO33 |
| Relay 5 | GPIO32 |
| Relay 6 | GPIO13 |
| Relay 7 | GPIO4 |
| Relay 8 | GPIO18 |
| Relay 9 | GPIO19 |
| Relay 10 | GPIO23 |
| PZEM TX (UART2 TX) | GPIO17 |
| PZEM RX (UART2 RX) | GPIO16 |
| DS3231 SDA | GPIO21 |
| DS3231 SCL | GPIO22 |
| Emergency Button | GPIO35 (input-only; external ~10kΩ pull-up to 3V3) |

---

## Reserved GPIOs

These GPIOs must NOT be used for relay outputs or general-purpose outputs.

| GPIO | Reason |
|------|--------|
| GPIO0 | Boot strap pin |
| GPIO2 | Boot strap pin |
| GPIO5 | Boot strap pin (avoid unless absolutely necessary) |
| GPIO12 | Boot strap pin |
| GPIO15 | Boot strap pin |
| GPIO34 | Input only |
| GPIO35 | Input only — now used for the Emergency Button (external pull-up) |
| GPIO36 | Input only |
| GPIO39 | Input only |
| GPIO1 | UART0 TX (Programming) |
| GPIO3 | UART0 RX (Programming) |
| GPIO6–GPIO11 | Connected to onboard SPI Flash |

---

## Safe GPIO Selection

- Only use GPIOs that are 100% safe for their role. For any new or
  reassigned pin, this rule takes priority over convenience.
- A "100% safe" output GPIO is one that is NOT a strapping/boot pin,
  NOT input-only, NOT wired to SPI flash, NOT a UART0 programming pin,
  and does NOT drive a signal or glitch during boot.
- 100% safe output GPIOs on the WROOM-32: **GPIO4, 13, 16, 17, 18, 19,
  21, 22, 23, 25, 26, 27, 32, 33**. Prefer these for every output.
- Avoid GPIO14 (and GPIO2/GPIO12/GPIO15) for relay outputs: even though
  GPIO14 is not input-only, it emits a PWM/clock signal at boot that can
  momentarily pulse a relay before firmware drives it inactive. Not 100%
  safe for a load-switching output.
- Input-only GPIOs (34, 35, 36, 39) may be used ONLY for inputs, and
  only when the signal has its own external pull — they have no internal
  pull-up/down.

## Relay Rules

- Relay GPIO assignments are fixed.
- GPIO numbers must never be configurable from the dashboard.
- Never assign relay outputs to boot pins.
- Never assign relay outputs to input-only pins.
- All relay outputs must remain OFF during ESP32 boot.
- Configure relay GPIOs before enabling relay control logic.
- Initialize relay pins to their inactive state immediately after boot.

---

## UART Rules

Use Hardware UART2 only.

| Function | GPIO |
|----------|------|
| TX | GPIO17 |
| RX | GPIO16 |

Rules:

- Never use SoftwareSerial.
- Never reassign UART2 pins.
- UART0 (GPIO1/GPIO3) is reserved for programming and debugging.

---

## I2C Rules

Use the default ESP32 I2C pins.

| Function | GPIO |
|----------|------|
| SDA | GPIO21 |
| SCL | GPIO22 |

Rules:

- Never use these pins for relays.
- Validate RTC communication before using RTC data.

---

# ESP32 Development Rules

## Timing

- Never use `delay()` unless explicitly required.
- Use `millis()` or FreeRTOS timers.
- Keep code non-blocking.

---

## Storage

- Use Preferences (NVS) instead of EEPROM.
- Write only when data changes.
- Debounce flash writes.
- Minimize flash wear.

---

## Wi-Fi

- Keep Wi-Fi logic separate from application logic.
- Support automatic reconnection.
- Handle disconnects gracefully.
- Never block the main application while reconnecting.

---

## OTA

- Keep OTA logic isolated from application logic.
- OTA updates must not interfere with relay safety.

---

## GPIO

- Define every GPIO in `config.h`.
- Never hardcode GPIO numbers throughout the project.
- Every GPIO must have only one dedicated purpose.

---

## FreeRTOS

- Protect shared resources with mutexes when required.
- Keep tasks lightweight.
- Avoid blocking operations inside tasks.

---

## Power Management

- Handle brownout events safely.
- Restore the previous safe state after unexpected resets.
- Detect and report reset reasons when possible.

---

## RTC

- Synchronize RTC using NTP when available.
- Validate RTC time before scheduling.
- Use RTC as fallback when Wi-Fi is unavailable.

---

## Filesystem

- Use LittleFS when persistent files are required.
- Handle mount failures gracefully.
- Never assume filesystem availability.

---

## General Coding Rules

- Keep `loop()` lightweight.
- Prevent watchdog resets.
- Validate all user inputs.
- Check return values from hardware drivers.
- Handle communication failures gracefully.
- Keep hardware drivers independent from the web server.
- Separate application logic from hardware control using a controller layer.
- Never expose GPIO numbers through the dashboard or REST API.

---

## Future Expansion

Reserve new peripherals before assigning GPIOs.

Future features may include:

- OTA Updates
- MQTT
- Firebase
- Telegram Notifications
- SD Card
- Ethernet
- LCD/OLED Display
- Additional Sensors

Update this document before changing any GPIO assignments.