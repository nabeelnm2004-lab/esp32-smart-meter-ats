# SmartMeterATS ESP32 — Complete REST API Reference

**Firmware version:** 2.2.0
**HTTP port:** 80 (configurable via `config::HTTP_PORT`)
**Base URL:** `http://<device-ip>/` or `http://smartmeterats.local/`
**Audit date:** 2026-08-13
**Purpose:** Full endpoint reference for the existing web dashboard and the future Android/iOS mobile app.

---

## Table of Contents

1. [Authentication & Roles](#1-authentication--roles)
2. [Common Response Envelope](#2-common-response-envelope)
3. [HTTP Status Codes Used](#3-http-status-codes-used)
4. [Dashboard / System Status](#4-dashboard--system-status)
5. [System Information](#5-system-information)
6. [Meters / PZEM](#6-meters--pzem)
7. [Relay Control](#7-relay-control)
8. [Scheduler](#8-scheduler)
9. [Protection / Faults](#9-protection--faults)
10. [Wi-Fi](#10-wi-fi)
11. [Security](#11-security)
12. [Event Log](#12-event-log)
13. [OTA Firmware Update](#13-ota-firmware-update)
14. [Device Control](#14-device-control)
15. [Configuration — Backup & Restore](#15-configuration--backup--restore)
16. [Mobile-App Readiness Verification](#16-mobile-app-readiness-verification)
17. [Authorization Audit](#17-authorization-audit)
18. [API Reliability & Error Handling](#18-api-reliability--error-handling)
19. [Known Limitations for Mobile App](#19-known-limitations-for-mobile-app)

---

## 1. Authentication & Roles

The firmware implements **HTTP Basic Authentication** with two roles.

| Role | Username | Password | Scope |
|------|----------|----------|-------|
| **Admin** | `admin` | OTA password (NVS key `otapass`, default `smartats123`) | Full read + write access |
| **Viewer** | `viewer` | `viewer123` (compile-time constant) | Read-only telemetry only |
| **None** | — | — | Rejected with `401` on all protected routes |

**Important notes for mobile apps:**
- The admin password is the **same credential** used for the web dashboard login and Arduino OTA.
- The viewer password is a fixed build-time constant; it **cannot** be changed without rebuilding firmware.
- Authorization is **enforced server-side** in `core::auth::requireRole()`. The UI hiding roles in the dashboard is cosmetic only — unauthenticated curl requests to mutating routes receive `401`.
- The admin password is stored in NVS and **survives reboots**. A factory reset reverts it to the default.

**Auth header format:**
```
Authorization: Basic base64(username:password)
```

Example for admin with password `smartats123`:
```
Authorization: Basic YWRtaW46c21hcnRhdHMxMjM=
```

---

## 2. Common Response Envelope

Every JSON response follows one of two shapes:

**Success:**
```json
{ "status": "ok" }
```
Some endpoints add extra fields alongside `"status":"ok"` (see individual entries).

**Error:**
```json
{ "status": "error", "msg": "human readable reason" }
```

All responses include the header:
```
X-Content-Type-Options: nosniff
Content-Type: application/json
```

> **Note for mobile app:** Always check `status` before reading other fields. The `msg` field is safe to display to the user.

---

## 3. HTTP Status Codes Used

| Code | Meaning |
|------|---------|
| `200` | Request accepted and applied |
| `400` | Malformed or out-of-range argument |
| `401` | Authentication required or wrong password |
| `409` | Well-formed request refused by current device state (trip, test mode, last meter, etc.) |
| `500` | Device failed to carry out the request (e.g. flash write error) |

---

## 4. Dashboard / System Status

### `GET /api/status`

**Auth required:** None (public, read-only telemetry)
**Poll interval:** Dashboard polls every **3 seconds**

The primary endpoint. Returns the complete live state of the device — PZEM readings, meter configuration, protection state, Wi-Fi, scheduler, and time. A mobile app can drive its entire UI from this single endpoint.

**Response (200):**
```json
{
  "voltage":          230.5,
  "current":          1.25,
  "power":            287.5,
  "energy":           1.234,
  "meterCount":       3,
  "maxMeters":        10,
  "activeMeter":      0,
  "emergency":        false,
  "bypass":           false,
  "pzemOK":           true,
  "todayUsed":        0.456,
  "thisMonth":        3.210,
  "lastMonth":        12.050,
  "protTrip":         false,
  "protReason":       "",
  "faultName":        "",
  "faultStatus":      "cleared",
  "recoverCountdown": 0,
  "recoverDelay":     0,
  "lastFaultEpoch":   0,
  "ovRec":            5,
  "uvRec":            5,
  "ocRec":            10,
  "resetDay":         1,
  "ovVolt":           250.0,
  "uvVolt":           180.0,
  "ocCurr":           16.0,
  "pzemWasReset":     false,
  "pzemResetCount":   0,
  "wifiMode":         2,
  "staSsid":          "HomeNetwork",
  "staOK":            true,
  "staIP":            "192.168.1.100",
  "apIP":             "192.168.4.1",
  "staRssi":          -62,
  "staFallback":      false,
  "otaReady":         true,
  "testMode":         false,
  "testLeft":         0,
  "uptime":           3600,
  "rtcDay":           13,
  "rtcMonth":         8,
  "rtcYear":          2026,
  "rtcHour":          18,
  "rtcMin":           30,
  "limits":           [5.0, 5.0, 5.0],
  "enabled":          [true, true, false],
  "used":             [1.234, 0.567, 0.0],
  "daily":            [0.1, 0.2, 0.3]
}
```

**Field reference:**

| Field | Type | Description |
|-------|------|-------------|
| `voltage` | float | Line voltage in Volts. `0.0` when `pzemOK` is false |
| `current` | float | Load current in Amperes. `0.0` when `pzemOK` is false |
| `power` | float | Active power in Watts. `0.0` when `pzemOK` is false |
| `energy` | float | PZEM cumulative energy register in kWh. `0.0` when `pzemOK` is false |
| `meterCount` | int | Number of active meter slots (1–10) |
| `maxMeters` | int | Maximum meter slots supported by hardware (always 10) |
| `activeMeter` | int | 0-based index of the currently energised meter relay |
| `emergency` | bool | `true` = emergency OFF latch is set; all relays are de-energised |
| `bypass` | bool | `true` = automatic limit-based switching is suspended |
| `pzemOK` | bool | `true` = PZEM sensor is responding and returning valid data |
| `todayUsed` | float | Energy used today (kWh), reset each day |
| `thisMonth` | float | Energy used in the current billing period (kWh) |
| `lastMonth` | float | Energy used in the previous billing period (kWh) |
| `protTrip` | bool | `true` = a protection fault has tripped; all relays are OFF |
| `protReason` | string | Human-readable fault description (e.g. `"Over Voltage: 255V"`) |
| `faultName` | string | Fault type: `"Over Voltage"`, `"Under Voltage"`, `"Over Current"`, or `""` |
| `faultStatus` | string | `"cleared"` / `"active"` / `"recovering"` |
| `recoverCountdown` | int | Seconds remaining in the auto-recovery stability window |
| `recoverDelay` | int | Configured stability window for this fault type (seconds) |
| `lastFaultEpoch` | uint32 | Unix timestamp of the last protection trip (0 = never) |
| `ovRec` | int | Over-voltage recovery window (seconds) |
| `uvRec` | int | Under-voltage recovery window (seconds) |
| `ocRec` | int | Over-current recovery window (seconds) |
| `resetDay` | int | Day-of-month (1–28) on which monthly energy counters reset |
| `ovVolt` | float | Over-voltage protection threshold (V) |
| `uvVolt` | float | Under-voltage protection threshold (V) |
| `ocCurr` | float | Over-current protection threshold (A) |
| `pzemWasReset` | bool | `true` = PZEM energy register reset detected since last poll (auto-clears on read) |
| `pzemResetCount` | uint16 | Number of PZEM resets detected since boot |
| `wifiMode` | int | `0` = AP only, `1` = STA only, `2` = AP+STA |
| `staSsid` | string | Configured station SSID (empty if none) |
| `staOK` | bool | `true` = station link is up and associated |
| `staIP` | string | Station IP address (empty if not connected) |
| `apIP` | string | Access-point IP address (always present) |
| `staRssi` | int | Station signal strength in dBm (0 if not connected) |
| `staFallback` | bool | `true` = device fell back to AP-only after STA connect timeout |
| `otaReady` | bool | `true` = ArduinoOTA responder is listening |
| `testMode` | bool | `true` = relay test mode is active |
| `testLeft` | int | Seconds remaining in test mode (0 if not active) |
| `uptime` | uint32 | Device uptime in seconds (64-bit hardware timer, never wraps) |
| `rtcDay/Month/Year/Hour/Min` | int | Current RTC date/time (only present when RTC is online) |
| `limits[]` | float[] | Per-meter energy limits in kWh (length = `meterCount`) |
| `enabled[]` | bool[] | Per-meter enabled flags (length = `meterCount`) |
| `used[]` | float[] | Per-meter energy used in kWh since last reset (length = `meterCount`) |
| `daily[]` | float[] | 30-day daily usage ring buffer in kWh, oldest-first, current day last |

**Error responses:**
- None under normal operation. Connection timeout = device offline.

---

### `GET /api/wifiStatus`

**Auth required:** None
**Poll interval:** Dashboard polls during a Wi-Fi connection attempt (1-second loop)

Returns Wi-Fi link state. Mobile apps should poll this after submitting a connect request.

**Response (200):**
```json
{
  "mode":     2,
  "staOK":    true,
  "ssid":     "HomeNetwork",
  "staIP":    "192.168.1.100",
  "apIP":     "192.168.4.1",
  "rssi":     -62,
  "fallback": false,
  "err":      "WPA auth failed"
}
```

> `"err"` is only present when the last station attempt failed.

---

## 5. System Information

### `GET /api/sysinfo`

**Auth required:** None
**Poll interval:** Dashboard polls every **10 seconds**

Firmware identity, chip stats, network and RTC health.

**Response (200):**
```json
{
  "fw":           "2.2.0",
  "build":        "Aug 13 2026 18:00:00",
  "chip":         "ESP32-D0WD-V3",
  "flash":        4194304,
  "heap":         145000,
  "cpu":          240,
  "uptime":       3600,
  "rstReason":    "Power-on",
  "rssi":         -62,
  "hostname":     "smartmeterats.local",
  "ip":           "192.168.1.100",
  "timeSrc":      "RTC (DS3231)",
  "meterCount":   3,
  "rtcOK":        true,
  "rtcLostPower": false,
  "rtcTime":      "2026-08-13 18:30:00",
  "lastSync":     "08-13 17:00"
}
```

| Field | Type | Description |
|-------|------|-------------|
| `fw` | string | Firmware version string |
| `build` | string | Compiler date/time stamp |
| `chip` | string | ESP32 chip model |
| `flash` | uint32 | Total flash size in bytes |
| `heap` | uint32 | Free heap memory in bytes |
| `cpu` | int | CPU frequency in MHz |
| `uptime` | uint32 | Uptime in seconds |
| `rstReason` | string | `"Power-on"`, `"SW restart"`, `"Panic"`, `"Watchdog"`, `"Brownout"`, `"Other"` |
| `rssi` | int | Wi-Fi RSSI in dBm (0 if not connected) |
| `hostname` | string | mDNS hostname |
| `ip` | string | Current reachable IP address |
| `timeSrc` | string | `"RTC (DS3231)"`, `"NTP"`, `"Browser sync"`, or `"None"` |
| `meterCount` | int | Active meter count |
| `rtcOK` | bool | `true` = DS3231 found and responding |
| `rtcLostPower` | bool | `true` = RTC backup battery dead or missing |
| `rtcTime` | string | Current RTC time as `"YYYY-MM-DD HH:MM:SS"` (only when `rtcOK`) |
| `lastSync` | string | Last time sync as `"MM-DD HH:MM"` (only when synced) |

---

## 6. Meters / PZEM

All meter-mutating endpoints require **Admin** authentication.

### `GET /api/setLimits`

**Auth required:** Admin

Set per-meter energy limits for the active window.

**Query parameters:**

| Parameter | Type | Range | Description |
|-----------|------|-------|-------------|
| `l0`–`lN` | float | 0.1–9999.0 | Energy limit for meter N in kWh |

Values outside the valid range are clamped to the default (5.0 kWh).

**Example:** `GET /api/setLimits?l0=10.0&l1=8.5&l2=15.0`

**Response (200):** `{"status":"ok"}`

**Errors:** `401` unauthenticated

---

### `GET /api/setEnabled`

**Auth required:** Admin

**Query parameters:**

| Parameter | Type | Description |
|-----------|------|-------------|
| `idx` | int | Meter index (0-based, must be < `meterCount`) |
| `val` | int | `1` = enable, `0` = disable |

**Response (200):** `{"status":"ok"}`

**Errors:**
- `400` `{"status":"error","msg":"idx and val required"}`
- `400` `{"status":"error","msg":"idx out of range"}`
- `401` unauthenticated

---

### `GET /api/switchMeter`

**Auth required:** Admin

Manually switch to a specific meter. Clears the emergency latch on success.

**Query parameters:**

| Parameter | Type | Description |
|-----------|------|-------------|
| `m` | int | Meter index (0-based) |

**Response (200):** `{"status":"ok"}`

**Errors:**
- `400` `{"status":"error","msg":"m required"}`
- `401` unauthenticated
- `409` `{"status":"error","msg":"test mode active"}` / `"protection trip active"` / `"meter disabled"` / `"energy limit reached"` / `"out of range"`

---

### `GET /api/resetEnergy`

**Auth required:** Admin

Zero all per-meter energy counters and re-baseline the PZEM register.

**Response (200):** `{"status":"ok"}`

**Errors:** `401`, `409`

---

### `GET /api/setActiveMeters`

**Auth required:** Admin

Set the number of active meter slots (1–10).

**Query parameters:**

| Parameter | Type | Range | Description |
|-----------|------|-------|-------------|
| `n` | int | 1–10 | New active meter count |

**Response (200):** `{"status":"ok"}`

**Errors:** `400`, `401`

---

### `GET /api/addMeter`

**Auth required:** Admin

Append one meter slot with factory defaults (limit = 5.0 kWh, enabled = true).

**Response (200):**
```json
{"status":"ok","idx":3,"meterCount":4}
```

**Errors:**
- `400` `{"status":"error","msg":"already at max meters"}`
- `401` unauthenticated

---

### `GET /api/removeMeter`

**Auth required:** Admin

Remove a meter slot; slots above it shift down. Cannot remove the active meter or the last remaining slot.

**Query parameters:**

| Parameter | Type | Description |
|-----------|------|-------------|
| `idx` | int | 0-based meter index to remove |

**Response (200):**
```json
{"status":"ok","meterCount":2}
```

**Errors:**
- `400` `{"status":"error","msg":"idx required"}`
- `400` cannot remove active or last meter
- `401` unauthenticated

---

## 7. Relay Control

### `GET /api/emergency`

**Auth required:** Admin

Latch emergency OFF: de-energise all relays immediately and persist the latch so it survives a reboot. Cleared only by a successful `switchMeter`.

**Response (200):** `{"status":"ok"}`

**Errors:** `401`, `409`

---

### `GET /api/setBypass`

**Auth required:** Admin

Enable or disable bypass mode (suspends automatic limit-based switching; protection and emergency remain active).

**Query parameters:**

| Parameter | Type | Description |
|-----------|------|-------------|
| `val` | int | `1` = enable bypass, `0` = disable bypass |

**Response (200):** `{"status":"ok"}`

**Errors:**
- `400` `{"status":"error","msg":"val required"}`
- `401`, `409`

---

### `GET /api/testMode`

**Auth required:** Admin

Enter or exit relay test mode. Entering drives all relays OFF. Auto-exits after 5 minutes.

**Query parameters:**

| Parameter | Type | Description |
|-----------|------|-------------|
| `on` | int | `1` = enter test mode, `0` = exit |

**Response (200):** `{"status":"ok"}`

**Errors:** `400`, `401`, `409`

---

### `GET /api/testRelay`

**Auth required:** Admin

Toggle one relay by index. **Only valid while test mode is active.**

**Query parameters:**

| Parameter | Type | Description |
|-----------|------|-------------|
| `idx` | int | 0-based relay index |

**Response (200):**
```json
{"status":"ok","on":true}
```

**Errors:**
- `400` `{"status":"error","msg":"idx required"}`
- `401` unauthenticated
- `409` test mode not active / emergency set / protection trip

---

## 8. Scheduler

The auto-switch scheduler runs automatically. There is no direct start/stop API; it is suspended implicitly via bypass or emergency.

### `GET /api/setResetDay`

**Auth required:** Admin

Set the day of month (1–28) on which monthly energy counters reset.

**Query parameters:**

| Parameter | Type | Range | Description |
|-----------|------|-------|-------------|
| `d` | int | 1–28 | Day of month |

**Response (200):** `{"status":"ok"}`

**Errors:**
- `400` `{"status":"error","msg":"d must be 1-28"}`
- `401` unauthenticated

---

## 9. Protection / Faults

### `GET /api/setProtection`

**Auth required:** Admin

Set OV/UV/OC thresholds and per-fault auto-recovery windows. Absent parameters are unchanged; out-of-range values are silently ignored.

**Query parameters:**

| Parameter | Type | Range | Description |
|-----------|------|-------|-------------|
| `ov` | float | 220–300 | Over-voltage threshold (V) |
| `uv` | float | 100–220 | Under-voltage threshold (V) |
| `oc` | float | 0.1–200 | Over-current threshold (A) |
| `ovrec` | int (ms) | 1000–300000 | OV recovery stability window (ms) |
| `uvrec` | int (ms) | 1000–300000 | UV recovery stability window (ms) |
| `ocrec` | int (ms) | 1000–300000 | OC recovery stability window (ms) |

**Example:** `GET /api/setProtection?ov=245&uv=185&oc=20&ovrec=5000`

**Response (200):** `{"status":"ok"}`

**Errors:** `401` unauthenticated

---

### `GET /api/clearFault`

**Auth required:** Admin

Clear a latched protection trip and restore the active meter unless the emergency latch is still set.

**Response (200):** `{"status":"ok"}`

**Errors:** `401`, `409`

---

## 10. Wi-Fi

### `GET /api/scanWiFi/start`

**Auth required:** None (no secrets exposed)

Kick off an asynchronous Wi-Fi scan. Returns immediately; the scan runs in
the WiFi driver's own task so the web server and dashboard `/api/status`
poll keep working throughout. If a scan is already in flight this is a no-op.

**Response (200):** `{"status":"ok"}`

**Errors:** `500` (`{"status":"error","msg":"scan start failed"}`)

### `GET /api/scanWiFi`

**Auth required:** None (no secrets exposed)

Poll the in-flight scan started by `/api/scanWiFi/start`. Never blocks.
Returns `{"status":"running"}` while the scan is still in the air and the
finished payload once it has completed. Returns up to 20 visible networks.

**Response (200) while scanning:**
```json
{"status": "running"}
```

**Response (200) when done:**
```json
{
  "status": "ok",
  "networks": [
    {"ssid": "HomeNetwork", "rssi": -52, "encrypted": true,  "channel": 6},
    {"ssid": "OpenWifi",    "rssi": -70, "encrypted": false, "channel": 11}
  ]
}
```

**Response (200) on driver failure:**
```json
{"status": "error", "msg": "scan failed"}
```

**Client flow:** call `/api/scanWiFi/start`, then poll `/api/scanWiFi` every
~500 ms until `status` is `"ok"` or `"error"` (give up after ~10 s). The
result buffer is freed by the device on the first done response.

---

### `GET /api/setWiFi`

**Auth required:** Admin

Persist and apply a new Wi-Fi mode and credentials. Radio reconfigures ~1 second after the response.

**Query parameters:**

| Parameter | Type | Description |
|-----------|------|-------------|
| `mode` | int | `0` = AP only, `1` = STA only, `2` = AP+STA |
| `ssid` | string | Station SSID (required for modes 1 and 2) |
| `pass` | string | Station password (min 8 chars for WPA; empty for open network) |

**Response (200):** `{"status":"ok"}`

**Errors:** `400`, `401`

---

### `GET /api/connectWiFi`

**Auth required:** Admin

Initiate an immediate station association attempt while keeping the AP running. Poll `/api/wifiStatus` for result.

**Query parameters:**

| Parameter | Type | Description |
|-----------|------|-------------|
| `ssid` | string | Target SSID (required) |
| `pass` | string | Password (optional) |
| `mode` | int | Optional Wi-Fi mode override |

**Response (200):** `{"status":"ok"}`

**Errors:** `400`, `401`

---

### `GET /api/disconnectWiFi`

**Auth required:** Admin

Drop the station link. AP stays up; saved credentials are preserved.

**Response (200):** `{"status":"ok"}`

**Errors:** `401`

---

### `GET /api/forgetWiFi`

**Auth required:** Admin

Erase saved station credentials from NVS and revert to AP-only mode.

**Response (200):** `{"status":"ok"}`

**Errors:** `401`

---

## 11. Security

### `GET /api/backup`

**Auth required:** Admin

Export all persisted configuration. **Contains Wi-Fi password and OTA password.**

**Response (200):**
```
Content-Disposition: attachment; filename=smartats-config.json
Content-Type: application/json
```
```json
{
  "ver":     1,
  "mcount":  3,
  "active":  0,
  "bypass":  false,
  "rday":    1,
  "ov":      250.0,
  "uv":      180.0,
  "oc":      16.0,
  "wmode":   2,
  "wssid":   "HomeNetwork",
  "wpass":   "mypassword",
  "otapass": "smartats123",
  "lastmon": 12.050,
  "lmon":    7,
  "lyear":   2026,
  "limits":  [5.0, 5.0, 5.0, 5.0, 5.0, 5.0, 5.0, 5.0, 5.0, 5.0],
  "enabled": [true, true, false, true, true, true, true, true, true, true]
}
```

> **Security:** Do NOT cache or log this response. Store only in user-controlled, encrypted app storage. Never transmit over the internet.

**Errors:** `401`

---

### `GET /api/setTime`

**Auth required:** Admin

Sync the RTC and internal epoch from the client clock.

**Query parameters:**

| Parameter | Type | Description |
|-----------|------|-------------|
| `y` | int | Year (2020–2099) |
| `mo` | int | Month (1–12) |
| `d` | int | Day (1–31) |
| `h` | int | Hour (0–23) |
| `mi` | int | Minute (0–59) |
| `s` | int | Second (0–59) |

**Example:** `GET /api/setTime?y=2026&mo=8&d=13&h=18&mi=30&s=00`

**Response (200):** `{"status":"ok"}`

**Errors:**
- `400` `{"status":"error","msg":"invalid date/time"}`
- `401` unauthenticated

---

## 12. Event Log

### `GET /api/events`

**Auth required:** None
**Poll interval:** Dashboard polls every **20 seconds** (System tab visible only)

Return the in-memory circular event log (up to 100 entries, oldest-first).

**Response (200):**
```json
{
  "events": [
    {"t": "08-13 17:30:00", "m": "Boot (power-on)"},
    {"t": "08-13 17:30:01", "m": "WiFi connect: HomeNetwork"},
    {"t": "up+10s",         "m": "PZEM sensor connected"}
  ]
}
```

| Field | Description |
|-------|-------------|
| `t` | Timestamp: `"MM-DD HH:MM:SS"` when RTC/NTP is available; `"up+Xs"` otherwise |
| `m` | Event message (max 47 chars) |

> The event log is RAM-only. It clears on every reboot. A `"Boot"` entry is always the first record.

---

### `GET /api/clearEvents`

**Auth required:** Admin

Wipe the in-memory event log.

**Response (200):** `{"status":"ok"}`

**Errors:** `401`

---

## 13. OTA Firmware Update

### `POST /api/update`

**Auth required:** Admin (checked **on every chunk** during streaming upload)

Upload a compiled `.bin` firmware image via multipart/form-data.

**Request:**
```
POST /api/update HTTP/1.1
Content-Type: multipart/form-data; boundary=...
Authorization: Basic ...

[binary firmware image in "firmware" field]
```

**Response (200):** `{"status":"ok"}` — device reboots in ~1 second.

**Error responses:**
- `400` upload incomplete: `{"status":"error","msg":"upload incomplete or never started"}`
- `401` `{"status":"error","msg":"auth required"}`
- `500` flash write error: `{"status":"error","msg":"<Update error string>"}`

**Safety guards:**
- First chunk validated for ESP32 magic byte (`0xE9`); non-firmware files rejected immediately.
- All relays driven OFF before flashing.
- PZEM sampling task paused during upload.
- 60-second stall timeout restores normal operation if upload hangs.

> **Mobile app:** Use a 180-second timeout. On success, wait ~5 seconds before reconnecting.

---

## 14. Device Control

### `GET /api/restart`

**Auth required:** Admin

Soft reboot; all settings preserved. Response sent before the deferred reboot (~1 second).

**Response (200):** `{"status":"ok"}`

**Errors:** `401`

---

### `GET /api/factoryReset`

**Auth required:** Admin

De-energise all relays, erase the entire NVS namespace, reboot to factory defaults.

> **Destructive. Cannot be undone.**

**Response (200):** `{"status":"ok"}` — device reboots in ~1 second.

**Errors:** `401`

---

## 15. Configuration — Backup & Restore

### `POST /api/restore`

**Auth required:** Admin
**Content-Type:** `application/json`

Import a full configuration JSON. Every field is optional and validated with the same clamps used at boot; an invalid file changes nothing.

**Request body:** JSON object with the same schema as `/api/backup` output.

**Response (200):** `{"status":"ok"}`

**Errors:**
- `400` `{"status":"error","msg":"invalid JSON"}`
- `401` unauthenticated

---

## 16. Mobile-App Readiness Verification

All required mobile-app data points are available from the existing API. No new endpoints are needed.

| App Requirement | Endpoint | Field(s) |
|----------------|----------|----------|
| ESP32 online/offline | `/api/status` poll | HTTP response vs. timeout |
| Wi-Fi status | `/api/status` | `staOK`, `wifiMode`, `staFallback` |
| Wi-Fi SSID | `/api/status` | `staSsid` |
| Station IP | `/api/status` | `staIP` |
| AP IP | `/api/status` | `apIP` |
| RSSI | `/api/status` | `staRssi` |
| PZEM connection | `/api/status` | `pzemOK` |
| Voltage | `/api/status` | `voltage` (0 when `pzemOK=false`) |
| Current | `/api/status` | `current` (0 when `pzemOK=false`) |
| Power | `/api/status` | `power` (0 when `pzemOK=false`) |
| Energy | `/api/status` | `energy` (0 when `pzemOK=false`) |
| Active relay | `/api/status` | `activeMeter` |
| All relay states | `/api/status` | `meterCount`, `activeMeter`, `enabled[]`, `emergency`, `protTrip` |
| Scheduler state | `/api/status` | `bypass` |
| Protection state | `/api/status` | `protTrip`, `protReason`, `faultName`, `faultStatus` |
| Fault recovery | `/api/status` | `faultStatus`, `recoverCountdown`, `recoverDelay` |
| Bypass mode | `/api/status` | `bypass` |
| Emergency state | `/api/status` | `emergency` |
| Firmware version | `/api/sysinfo` | `fw`, `build` |
| Chip info | `/api/sysinfo` | `chip`, `flash`, `heap`, `cpu` |
| Uptime | `/api/status` or `/api/sysinfo` | `uptime` |
| Reset reason | `/api/sysinfo` | `rstReason` |
| RTC health | `/api/sysinfo` | `rtcOK`, `rtcLostPower`, `rtcTime` |
| Time source | `/api/sysinfo` | `timeSrc` |
| Energy stats | `/api/status` | `todayUsed`, `thisMonth`, `lastMonth`, `daily[]` |
| Monthly reset day | `/api/status` | `resetDay` |
| Protection thresholds | `/api/status` | `ovVolt`, `uvVolt`, `ocCurr` |
| Event log | `/api/events` | `events[]` |
| OTA readiness | `/api/status` | `otaReady` |
| Test mode | `/api/status` | `testMode`, `testLeft` |

---

## 17. Authorization Audit

Authorization is enforced **server-side** via `core::auth::requireRole()`. Dashboard UI hiding is cosmetic only.

| Endpoint | Method | Auth Enforcement | Access |
|----------|--------|-----------------|--------|
| `GET /` | GET | None | Public |
| `GET /api/status` | GET | None | Public |
| `GET /api/wifiStatus` | GET | None | Public |
| `GET /api/scanWiFi/start` | GET | None | Public |
| `GET /api/scanWiFi` | GET | None | Public |
| `GET /api/events` | GET | None | Public |
| `GET /api/sysinfo` | GET | None | Public |
| `GET /api/backup` | GET | `authed()` → Admin | Admin only |
| `GET /api/setLimits` | GET | `authed()` → Admin | Admin only |
| `GET /api/setEnabled` | GET | `authed()` → Admin | Admin only |
| `GET /api/switchMeter` | GET | `authed()` → Admin | Admin only |
| `GET /api/resetEnergy` | GET | `authed()` → Admin | Admin only |
| `GET /api/emergency` | GET | `authed()` → Admin | Admin only |
| `GET /api/setProtection` | GET | `authed()` → Admin | Admin only |
| `GET /api/clearFault` | GET | `authed()` → Admin | Admin only |
| `GET /api/setActiveMeters` | GET | `authed()` → Admin | Admin only |
| `GET /api/addMeter` | GET | `authed()` → Admin | Admin only |
| `GET /api/removeMeter` | GET | `authed()` → Admin | Admin only |
| `GET /api/setBypass` | GET | `authed()` → Admin | Admin only |
| `GET /api/setWiFi` | GET | `authed()` → Admin | Admin only |
| `GET /api/setResetDay` | GET | `authed()` → Admin | Admin only |
| `GET /api/connectWiFi` | GET | `authed()` → Admin | Admin only |
| `GET /api/disconnectWiFi` | GET | `authed()` → Admin | Admin only |
| `GET /api/forgetWiFi` | GET | `authed()` → Admin | Admin only |
| `GET /api/clearEvents` | GET | `authed()` → Admin | Admin only |
| `POST /api/restore` | POST | `authed()` → Admin | Admin only |
| `GET /api/factoryReset` | GET | `authed()` → Admin | Admin only |
| `GET /api/restart` | GET | `authed()` → Admin | Admin only |
| `GET /api/testMode` | GET | `authed()` → Admin | Admin only |
| `GET /api/testRelay` | GET | `authed()` → Admin | Admin only |
| `GET /api/setTime` | GET | `authed()` → Admin | Admin only |
| `POST /api/update` | POST | Per-chunk `authenticate()` | Admin only |

**Secrets never returned by any API (except `/api/backup` for Admin restore purposes):**
- Admin password — only in `/api/backup` (Admin only)
- Wi-Fi station password — only in `/api/backup` (Admin only)
- AP password — never returned
- Viewer password — never returned

---

## 18. API Reliability & Error Handling

| Condition | Behaviour |
|-----------|-----------|
| **Unauthorized request** | `401 {"status":"error","msg":"auth required"}` on all mutating routes |
| **Missing required parameter** | `400 {"status":"error","msg":"<param> required"}` |
| **Out-of-range parameter** | Clamped silently (thresholds) or `400` with message (meter index) |
| **State conflict** | `409 {"status":"error","msg":"<reason>"}` (test mode, trip, last meter, etc.) |
| **PZEM unavailable** | `pzemOK=false` in `/api/status`; voltage/current/power/energy = 0 |
| **Wi-Fi failure** | `staOK=false`, `err` field in `/api/wifiStatus`, `staFallback=true` |
| **OTA stall timeout** | Aborted after 60 s; normal operation restored |
| **Wi-Fi scan in progress** | `/api/scanWiFi` returns `{"status":"running"}` until the async scan finishes |
| **Flash write error** | `500 {"status":"error","msg":"<Update.errorString()>"}` |
| **Device offline** | HTTP timeout / connection refused |

---

## 19. Known Limitations for Mobile App

| Limitation | Details | Workaround |
|-----------|---------|------------|
| **All mutating endpoints use GET** | Parameters passed as query strings; only `/api/restore` and `/api/update` use POST | Encode parameters as URL query strings; never cache GET mutating calls |
| **No CORS headers** | Deliberately omitted to prevent cross-site scripting | Use native HTTP client, not a browser WebView |
| **HTTP only (no HTTPS)** | Plain HTTP on port 80 | Use only on trusted local networks; never relay credentials over the internet |
| **No push / WebSocket** | No server push capability | Poll `/api/status` every 3–5 seconds |
| **Single-threaded web server** | One request handled at a time | Serialise requests; do not send concurrent API calls |
| **Wi-Fi scan is asynchronous** | Start via `/api/scanWiFi/start`, then poll `/api/scanWiFi` (returns `{"status":"running"}` until done); never blocks other requests | Use the start/poll flow; treat `"running"` as "keep waiting" up to ~10 s |
| **OTA via POST/multipart only** | No URL-based firmware update | Implement multipart file upload with per-chunk auth |
| **Viewer role limited** | `viewer`/`viewer123` accepted only on 5 public endpoints; all mutating routes return `401` | Do not attempt Admin-only endpoints with Viewer credentials |
| **Viewer password is compile-time constant** | Cannot be changed without rebuilding firmware | Do not rely on Viewer for security |
| **No `/api/setPassword` endpoint** | Password changed only via `/api/restore` with a modified `otapass` field | Document password change procedure for users |
| **`pzemWasReset` auto-clears on poll** | Latches `true` until `/api/status` is polled, then resets automatically | Check this field on every successful poll and notify the user if `true` |
| **No relay state array** | Individual relay ON/OFF not returned directly; derive from `activeMeter`, `emergency`, `protTrip` | Relay N is ON when `activeMeter==N && !emergency && !protTrip && !testMode` |
| **Event log is RAM-only** | Cleared on every reboot; no persistent log storage | Use `"Boot"` entry as a natural separator; offer local export from the app |
| **`/api/backup` exposes credentials** | Wi-Fi password and OTA password in plaintext JSON | Never log or cache backup response; store only in encrypted user storage |
| **mDNS requires local network** | `smartmeterats.local` resolves only on the same LAN subnet | Fall back to IP address if mDNS unavailable |
| **RTC date/time fields are optional** | `rtcDay/Month/Year/Hour/Min` in `/api/status` are omitted when RTC is offline | Check for presence before using; fall back to device uptime or mobile clock |

---

*Document generated from firmware source audit of SmartMeterATS\_ESP32 v2.2.0.*
*Source files audited: `web_server.cpp`, `auth_manager.cpp/.h`, `config.h`, `system_state.h`,
`ota_manager.cpp`, `wifi_manager.cpp/.h`, `meter_controller.h`, `json_response.cpp/.h`,
`event_log.h`, `relay_manager.h`, `protection_monitor.h`, `switch_scheduler.h`,
`settings_storage.h`, `pzem_sensor.h`, `dashboard.h`, `SmartMeterATS_ESP32.ino`.*
