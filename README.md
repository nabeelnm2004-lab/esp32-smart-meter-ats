# ESP8266 Smart Meter ATS

Smart automatic energy meter switching system for ESP8266 NodeMCU (ESP-12E).

This project reads live electrical values from a PZEM-004T V3 module, controls up to 3 relay-driven meter/load lines, and provides a built-in web dashboard over WiFi Access Point mode.

## Main Features

- 3-channel relay switching (active-LOW outputs)
- Automatic meter switching based on per-meter kWh limit
- Live monitoring from PZEM-004T V3:
  - Voltage (V)
  - Current (A)
  - Power (W)
  - Energy (kWh)
- Web dashboard at `http://192.168.4.1/`
- EEPROM persistence for key settings
- Daily and monthly usage tracking
- Protection logic:
  - Over-voltage trip
  - Under-voltage trip
  - Over-current trip
- Emergency OFF mode

## Hardware

- NodeMCU 1.0 (ESP-12E)
- PZEM-004T V3 energy meter module
- DS3231 RTC module
- 3-channel relay module (active-LOW)
- Emergency push button

## Pin Mapping

- D5 (GPIO14) -> Relay 1
- D6 (GPIO12) -> Relay 2
- D7 (GPIO13) -> Relay 3
- D3 (GPIO0)  -> PZEM TX to ESP RX
- D4 (GPIO2)  -> ESP TX to PZEM RX
- D2 (GPIO4)  -> RTC SDA
- D1 (GPIO5)  -> RTC SCL
- D8 (GPIO15) -> Emergency button

## WiFi Access Point

- SSID: `SmartMeterATS`
- Password: `12345678`
- Dashboard URL: `http://192.168.4.1/`

## Required Libraries

Install from Arduino Library Manager:

- PZEM004Tv30 (Jakub Mandula)
- RTClib (Adafruit)
- ArduinoJson (v6.x)

Built into ESP8266 core:

- ESP8266WiFi
- ESP8266WebServer
- SoftwareSerial
- Wire
- EEPROM

## Arduino IDE Setup

1. Install ESP8266 board package.
2. Select board: `NodeMCU 1.0 (ESP-12E Module)`.
3. Recommended settings:
   - Upload Speed: 115200
   - CPU Frequency: 80 MHz
   - Flash Size: 4MB (FS:2MB OTA:~1019KB)
4. Select correct COM port.
5. Upload `SmartMeterATS.ino`.

## Project Files

- `SmartMeterATS.ino` - main firmware
- `PROJECT_GUIDE.txt` - full implementation and wiring guide
- `prompt.txt` - original build prompt

## Safety Note

This project interacts with mains-connected equipment through relays and energy meter hardware. Use proper electrical isolation and follow qualified electrical safety practices.
