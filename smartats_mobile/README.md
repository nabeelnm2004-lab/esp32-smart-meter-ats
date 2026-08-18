# SmartATS Mobile - Flutter Android App

This is the production-ready Flutter app codebase for **SmartATS Mobile**, matching the UI, theme, and real REST API endpoints of the ESP32 Smart Meter Automatic Transfer Switch.

## 📱 Features Included
1. **Dashboard Screen**: Real-time Line Voltage, Load Current, Active Power (kW), Cumulative Energy (kWh), dynamic Power Flow Topology, and E-Stop Cutoff.
2. **Meters & Loads Screen**: Dynamic multi-source monitoring (Grid, Generator, Solar, Custom) with real-time monthly quota usage bars and 1-tap bus transfer.
3. **Protection Matrix Screen**: Interactive Over-Voltage, Under-Voltage, and Over-Current sliders with direct write to hardware EEPROM/NVS.
4. **Wi-Fi & Provisioning Screen**: Station/AP status, live 2.4GHz network scanner with RSSI meters, and one-tap network provisioning.
5. **System & Health Screen**: ESP32 chip specs, free heap, uptime, and DS3231 RTC synchronization.

## 📦 Pre-built APK

A release APK is included in the repo at:
`smartats_mobile/build/app/outputs/flutter-apk/app-release.apk`

Copy it to your Android phone and install it (enable "Install unknown apps" on Android 8+).

## 🚀 Quick Start (Building APK)

1. Make sure Flutter 3.x is installed on your machine.
2. Open terminal in the `smartats_mobile` directory:
```bash
flutter pub get
flutter run
```
3. To build the release APK for your Android device:
```bash
flutter build apk --release
```
The output APK will be located at:
`build/app/outputs/flutter-apk/app-release.apk`

## 🔌 Connecting to Your ESP32
- In AP Mode (Direct Hotspot): Default IP is `http://192.168.4.1`
- In Station Mode (Home Router): Tap the antenna icon in the top right corner of the app to enter your ESP32's local network IP (e.g. `http://192.168.1.105`).
