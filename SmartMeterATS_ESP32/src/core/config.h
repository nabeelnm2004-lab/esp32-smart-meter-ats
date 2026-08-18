/*
 * config.h — every compile-time constant for the Smart Meter ATS.
 *
 * Single source of truth for GPIO assignments, timing, thresholds and
 * NVS key names. Nothing outside this file may hardcode a pin number
 * or a magic timing value.
 *
 * The firmware is universal: one binary serves 1..MAX_METERS meters.
 * The only per-installation difference is the persisted "Active
 * Meters" count, so no constant here is ever customer-specific.
 */
#ifndef CORE_CONFIG_H
#define CORE_CONFIG_H

#include <Arduino.h>
#include <IPAddress.h>

namespace core {
namespace config {

// ------------------------------------------------------------
//  Firmware identity
// ------------------------------------------------------------
constexpr const char* FW_VERSION = "2.2.0";
#define FW_BUILD  __DATE__ " " __TIME__

// ------------------------------------------------------------
//  Meter capacity
//  The PCB carries footprints for MAX_METERS relay positions; only
//  the purchased number is populated. Unpopulated positions are
//  driven OFF forever.
// ------------------------------------------------------------
constexpr uint8_t MAX_METERS            = 10;
constexpr uint8_t DEFAULT_ACTIVE_METERS = 3;

// ------------------------------------------------------------
//  GPIO assignments — FIXED for the life of the product.
//  RELAY_PINS is the ONLY place a meter index maps to a GPIO.
//  The dashboard and API never see GPIO numbers, only indices.
//
//  Relay1  -> GPIO25      Relay6  -> GPIO13
//  Relay2  -> GPIO26      Relay7  -> GPIO4
//  Relay3  -> GPIO27      Relay8  -> GPIO18
//  Relay4  -> GPIO33      Relay9  -> GPIO19
//  Relay5  -> GPIO32      Relay10 -> GPIO23
//
//  All are 100% safe output pins: output-capable, non-strapping, and
//  free of any boot-time signal, avoiding UART0, UART2, I2C and flash.
// ------------------------------------------------------------
extern const uint8_t RELAY_PINS[MAX_METERS];

constexpr uint8_t PZEM_RX_PIN   = 16;  // UART2 RX  (PZEM TX -> ESP GPIO16)
constexpr uint8_t PZEM_TX_PIN   = 17;  // UART2 TX  (ESP GPIO17 -> PZEM RX)
constexpr uint8_t I2C_SDA_PIN   = 21;  // DS3231 SDA
constexpr uint8_t I2C_SCL_PIN   = 22;  // DS3231 SCL
// Input-only pin (no internal pull-up), so the button needs an
// external ~10k pull-up to 3V3; button to GND, LOW means pressed.
constexpr uint8_t BTN_EMERGENCY = 35;

// Most relay modules are ACTIVE LOW.
constexpr uint8_t RELAY_ON  = LOW;
constexpr uint8_t RELAY_OFF = HIGH;

// ------------------------------------------------------------
//  Timing (milliseconds unless noted)
// ------------------------------------------------------------
constexpr unsigned long RELAY_SWITCH_DELAY    = 500;
constexpr unsigned long PZEM_READ_INTERVAL    = 2000;
constexpr unsigned long SERIAL_PRINT_INTERVAL = 3000;
constexpr unsigned long BTN_DEBOUNCE_MS       = 50;
constexpr unsigned long RESET_CHECK_INTERVAL  = 60000;   // rollover check cadence
constexpr unsigned long RESTART_GRACE_MS      = 1000;    // let HTTP response flush
constexpr unsigned long WIFI_APPLY_DELAY_MS   = 1000;    // defer radio reconfigure
constexpr unsigned long WIFI_POLL_INTERVAL_MS = 1000;    // STA link poll cadence
constexpr unsigned long NTP_POLL_INTERVAL_MS  = 1000;    // NTP adopt poll cadence
constexpr uint8_t       WDT_TIMEOUT_S         = 10;      // task watchdog, seconds

// FreeRTOS sampling task (Core 0)
constexpr uint32_t PZEM_TASK_STACK    = 4096;
constexpr UBaseType_t PZEM_TASK_PRIO  = 1;
constexpr BaseType_t PZEM_TASK_CORE   = 0;

// ------------------------------------------------------------
//  Relay test mode — the scheduler is locked out while active, so
//  a forgotten browser tab must never leave loads in a test state.
// ------------------------------------------------------------
constexpr unsigned long TEST_MODE_TIMEOUT_MS = 300000UL;  // 5 minutes

// ------------------------------------------------------------
//  Event log — RAM only, zero flash wear. A reboot naturally shows
//  up as a fresh "Boot" entry, which makes power loss visible.
// ------------------------------------------------------------
constexpr uint8_t EVENT_LOG_SIZE = 100;
constexpr uint8_t EVENT_MSG_LEN  = 48;

// ------------------------------------------------------------
//  NVS write protection (flash wear)
//  Routine changes are batched: a flush needs BOTH a settled change
//  burst (debounce) and a minimum gap since the last real write
//  (throttle). MAX_DEFER is the starvation guard for state that is
//  dirtied continuously.
// ------------------------------------------------------------
constexpr unsigned long NVS_MIN_WRITE_INTERVAL_MS = 30000UL;
constexpr unsigned long NVS_DEBOUNCE_MS           = 3000UL;
constexpr unsigned long NVS_MAX_DEFER_MS          = 60000UL;

constexpr const char* NVS_NAMESPACE = "smartats";

// ------------------------------------------------------------
//  Statistics
// ------------------------------------------------------------
constexpr uint8_t DAILY_HISTORY_DAYS = 30;

// ------------------------------------------------------------
//  Value ranges — shared by loadSettings(), the REST API and the
//  config-restore path so every entry point clamps identically.
// ------------------------------------------------------------
constexpr float ENERGY_LIMIT_MIN     = 0.1f;
constexpr float ENERGY_LIMIT_MAX     = 9999.0f;
constexpr float ENERGY_LIMIT_DEFAULT = 5.0f;

constexpr float OVER_VOLTAGE_MIN     = 220.0f;
constexpr float OVER_VOLTAGE_MAX     = 300.0f;
constexpr float OVER_VOLTAGE_DEFAULT = 250.0f;

constexpr float UNDER_VOLTAGE_MIN     = 100.0f;
constexpr float UNDER_VOLTAGE_MAX     = 220.0f;
constexpr float UNDER_VOLTAGE_DEFAULT = 180.0f;

constexpr float OVER_CURRENT_MIN     = 0.1f;
constexpr float OVER_CURRENT_MAX     = 200.0f;
constexpr float OVER_CURRENT_DEFAULT = 16.0f;

// Below this the line is treated as de-energised, so under-voltage
// protection must not trip on a simply dead line.
constexpr float LINE_LIVE_VOLTAGE = 10.0f;

// ------------------------------------------------------------
//  Auto-recovery stability windows (milliseconds)
//  After an OV/UV/OC trip, the condition must stay CONTINUOUSLY normal
//  for the fault's window before the trip auto-clears and the load is
//  restored. A single abnormal sample restarts the window, which is
//  what prevents relay chatter on an unstable line. Runtime-configurable
//  and persisted; MIN is an anti-chatter floor.
// ------------------------------------------------------------
constexpr unsigned long OV_RECOVERY_MS_DEFAULT = 5000UL;
constexpr unsigned long UV_RECOVERY_MS_DEFAULT = 5000UL;
constexpr unsigned long OC_RECOVERY_MS_DEFAULT = 10000UL;
constexpr unsigned long RECOVERY_MS_MIN        = 1000UL;     // never chatter faster than 1s
constexpr unsigned long RECOVERY_MS_MAX        = 300000UL;   // 5 min ceiling

constexpr uint8_t RESET_DAY_MIN     = 1;
constexpr uint8_t RESET_DAY_MAX     = 28;   // valid in every month
constexpr uint8_t RESET_DAY_DEFAULT = 1;

// A PZEM energy reading below this fraction of the previous one means
// the module lost power and zeroed its register; a smaller dip is
// just read jitter.
constexpr float PZEM_RESET_RATIO = 0.9f;
// Sanity guard against a counter wrap being booked as consumption.
constexpr float PZEM_MAX_SANE_DELTA_KWH = 100.0f;
// Consecutive failed Modbus read cycles (each PZEM_READ_INTERVAL) before
// the PZEM is declared disconnected. A single failed frame is treated as
// line noise; only a sustained run flips the status to Disconnected.
constexpr uint8_t PZEM_OFFLINE_FAILURES = 3;
// Longest outage for which lost energy is still estimated from the
// last known power; beyond this the estimate is meaningless.
constexpr float PZEM_MAX_ESTIMATE_HOURS = 24.0f;

// Earliest epoch accepted from NTP or a browser sync (2020-01-01).
constexpr uint32_t MIN_VALID_EPOCH = 1577836800UL;
constexpr uint16_t MIN_VALID_YEAR  = 2020;
constexpr uint16_t MAX_VALID_YEAR  = 2099;

// ------------------------------------------------------------
//  WiFi
// ------------------------------------------------------------
constexpr const char* AP_SSID     = "SmartMeterATS";
constexpr const char* AP_PASSWORD = "12345678";
constexpr const char* MDNS_HOSTNAME = "smartats";
constexpr const char* OTA_HOSTNAME  = "SmartMeterATS";
constexpr const char* DEFAULT_OTA_PASSWORD = "smartats123";
// HTTP Basic auth user for the mutating endpoints. The password is
// the OTA password (NVS "otapass"), so there is one device secret.
constexpr const char* AUTH_USER = "admin";

// Read-only "Viewer" role. Accepted on telemetry endpoints, refused on
// every mutating route. Distinct from the admin device secret so it can
// be shared without granting control. Password is a fixed build-time
// constant (no control capability, so it is not stored in NVS).
constexpr const char* AUTH_VIEWER_USER = "viewer";
constexpr const char* VIEWER_PASSWORD  = "viewer123";

extern const IPAddress AP_IP;
extern const IPAddress AP_SUBNET;

constexpr unsigned long STA_CONNECT_TIMEOUT_MS = 30000UL;
// Station Only mode keeps its SoftAP alive after the station link is
// proven so a browser connected to the AP can be handed off to the router
// (mDNS / STA IP) before the AP disappears. The AP is retired only after
// this grace period AND once no client is associated to it any more — it
// must never strand a connected dashboard.
constexpr unsigned long STA_ONLY_AP_GRACE_MS = 15000UL;
// Named SSID_MAX_LENGTH/PASS_MAX_LENGTH (not MAX_SSID_LEN) because the
// ESP32 core >= 3.3 defines MAX_SSID_LEN as a macro in
// esp_wifi_types_generic.h, which would replace the constexpr identifier.
constexpr uint8_t  SSID_MAX_LENGTH = 32;
constexpr uint8_t  PASS_MAX_LENGTH = 64;
constexpr uint8_t  MAX_SCAN_RESULTS = 20;
constexpr uint16_t HTTP_PORT = 80;

enum WifiMode : uint8_t {
  WIFI_MODE_AP_ONLY  = 0,
  WIFI_MODE_STA_ONLY = 1,
  WIFI_MODE_AP_STA   = 2,
};

// ------------------------------------------------------------
//  JSON document capacities
// ------------------------------------------------------------
constexpr size_t JSON_STATUS_SIZE      = 4096;
constexpr size_t JSON_SYSINFO_SIZE     = 768;
constexpr size_t JSON_BACKUP_SIZE      = 1536;
constexpr size_t JSON_WIFI_STATUS_SIZE = 512;

// Config backup schema version, for future migration.
constexpr uint8_t BACKUP_SCHEMA_VERSION = 1;

}  // namespace config
}  // namespace core

#endif  // CORE_CONFIG_H
