#include "web_server.h"

#include <ArduinoJson.h>
#include <WebServer.h>
#include <WiFi.h>

#include "../core/config.h"
#include "../core/event_log.h"
#include "../core/meter_controller.h"
#include "../core/system_state.h"
#include "../hardware/rtc_manager.h"
#include "../network/ota_manager.h"
#include "../network/wifi_manager.h"
#include "../scheduler/switch_scheduler.h"
#include "../storage/settings_storage.h"
#include "../utilities/json_response.h"
#include "dashboard.h"

namespace ui {
namespace web {

namespace config     = core::config;
namespace state      = core::state;
namespace controller = core::controller;
namespace eventlog   = core::eventlog;
namespace autoswitch = scheduler::autoswitch;
namespace wifi       = network::wifi;
namespace ota        = network::ota;
namespace nvs        = storage::settings;
namespace json       = utilities::json;

using config::MAX_METERS;

namespace {

WebServer server(config::HTTP_PORT);

// ------------------------------------------------------------
//  Helpers
// ------------------------------------------------------------

// Gate for the mutating routes. Reuses the OTA password (NVS "otapass")
// with HTTP Basic auth. On failure it sends a plain 401 JSON (no native
// WWW-Authenticate dialog — the dashboard attaches the header itself).
bool requireAuth() {
  if (server.authenticate(config::AUTH_USER, state::otaPassword.c_str())) return true;
  json::sendError(server, 401, F("auth required"));
  return false;
}

// Translate a rejected controller Result into a documented HTTP code.
// failCode is 400 for a malformed/out-of-range argument or 409 for a
// well-formed request refused by the current state.
void sendResult(const controller::Result& r, int failCode) {
  if (r.ok) json::sendOk(server);
  else      json::sendError(server, failCode, r.msg);
}

// ------------------------------------------------------------
//  Read-only endpoints — serialise state for the dashboard. These
//  belong in the presentation layer: they format, they never mutate.
// ------------------------------------------------------------

// GET /api/status — the poll the dashboard runs every few seconds.
void handleStatus() {
  StaticJsonDocument<config::JSON_STATUS_SIZE> doc;

  STATE_LOCK();
  doc["voltage"]     = state::pzemOK ? state::liveVoltage : 0.0f;
  doc["current"]     = state::pzemOK ? state::liveCurrent : 0.0f;
  doc["power"]       = state::pzemOK ? state::livePower   : 0.0f;
  doc["energy"]      = state::liveEnergy;
  doc["meterCount"]  = state::activeMeterCount;
  doc["maxMeters"]   = MAX_METERS;
  doc["activeMeter"] = state::activeMeter;
  doc["emergency"]   = state::emergencyOff;
  doc["bypass"]      = state::bypassMode;
  doc["pzemOK"]      = state::pzemOK;
  doc["todayUsed"]   = state::todayUsed;
  doc["thisMonth"]   = state::currentMonthUsed;
  doc["lastMonth"]   = state::lastMonthUsed;
  doc["protTrip"]    = state::protTrip;
  doc["protReason"]  = state::protReason;
  doc["resetDay"]    = state::monthlyResetDay;
  doc["ovVolt"]      = state::ovVoltThresh;
  doc["uvVolt"]      = state::uvVoltThresh;
  doc["ocCurr"]      = state::ocCurrThresh;
  // PZEM reset visibility: pzemWasReset latches until the dashboard has
  // shown its toast once, so clear it as we serialise.
  doc["pzemWasReset"]   = state::pzemWasReset;
  doc["pzemResetCount"] = state::pzemResetCount;
  state::pzemWasReset = false;
  doc["wifiMode"]   = state::wifiMode;
  doc["staSsid"]    = state::staSsid;
  doc["staOK"]      = wifi::isStationConnected();
  doc["staIP"]      = wifi::isStationConnected() ? WiFi.localIP().toString() : "";
  doc["otaReady"]   = ota::isReady();
  doc["testMode"]   = state::testMode;
  doc["testLeft"]   = state::testMode
    ? (int)((config::TEST_MODE_TIMEOUT_MS - (millis() - state::testModeStart)) / 1000)
    : 0;

  JsonArray lims  = doc.createNestedArray("limits");
  JsonArray enab  = doc.createNestedArray("enabled");
  JsonArray used  = doc.createNestedArray("used");
  JsonArray daily = doc.createNestedArray("daily");
  // Only the active window is serialised — hidden meters are ignored.
  for (int i = 0; i < state::activeMeterCount; i++) {
    lims.add(state::meters[i].energyLimit);
    enab.add(state::meters[i].enabled);
    used.add(state::meters[i].energyUsed);
  }
  // daily[]: oldest entry first, current day last.
  for (int i = 0; i < config::DAILY_HISTORY_DAYS; i++) {
    daily.add(state::dailyUsage[(state::dailyIndex + i) % config::DAILY_HISTORY_DAYS]);
  }
  STATE_UNLOCK();

  // RTC time so the dashboard can compute the next reset date. I2C is
  // Core 1 only and this handler runs on Core 1.
  if (state::rtcOK) {
    DateTime now = hardware::rtc::now();
    doc["rtcDay"]   = now.day();
    doc["rtcMonth"] = now.month();
    doc["rtcYear"]  = now.year();
  }

  String out;
  serializeJson(doc, out);
  json::sendJson(server, out);
}

// GET /api/wifiStatus — polled by the dashboard's WiFi modal.
void handleWifiStatus() {
  StaticJsonDocument<config::JSON_WIFI_STATUS_SIZE> doc;
  doc["mode"]  = state::wifiMode;
  doc["staOK"] = wifi::isStationConnected();
  doc["ssid"]  = state::staSsid;
  doc["staIP"] = wifi::isStationConnected() ? WiFi.localIP().toString() : "";
  doc["rssi"]  = wifi::isStationConnected() ? WiFi.RSSI() : 0;
  if (wifi::lastError().length()) doc["err"] = wifi::lastError();
  String out;
  serializeJson(doc, out);
  json::sendJson(server, out);
}

// GET /api/scanWiFi — list visible networks (blocking scan).
void handleScanWiFi() {
  json::sendJson(server, wifi::scanNetworksJson());
}

// GET /api/events — newest-last JSON list. Snapshotted under the lock
// since a concurrent add() from Core 0 would shift the ring.
void handleEvents() {
  String out = "{\"events\":[";
  STATE_LOCK();
  const uint8_t n = eventlog::count();
  for (uint8_t i = 0; i < n; i++) {
    const eventlog::EventEntry& e = eventlog::at(i);
    char ts[24];
    if (e.epoch) {   // wall-clock when RTC/browser/NTP time is known
      DateTime t((uint32_t)e.epoch);
      snprintf(ts, sizeof(ts), "%02d-%02d %02d:%02d:%02d",
               t.month(), t.day(), t.hour(), t.minute(), t.second());
    } else {         // fall back to uptime seconds
      snprintf(ts, sizeof(ts), "up+%lus", (unsigned long)e.up);
    }
    if (i) out += ',';
    out += "{\"t\":\"";
    out += ts;
    out += "\",\"m\":\"";
    out += json::escape(e.msg);
    out += "\"}";
  }
  STATE_UNLOCK();
  out += "]}";
  json::sendJson(server, out);
}

// GET /api/sysinfo — firmware, chip, network and RTC health.
void handleSysInfo() {
  StaticJsonDocument<config::JSON_SYSINFO_SIZE> doc;
  doc["fw"]    = config::FW_VERSION;
  doc["build"] = FW_BUILD;
  doc["chip"]  = ESP.getChipModel();
  doc["flash"] = ESP.getFlashChipSize();
  doc["heap"]  = ESP.getFreeHeap();
  doc["cpu"]   = getCpuFrequencyMhz();
  doc["uptime"] = eventlog::uptimeSeconds();
  esp_reset_reason_t rr = esp_reset_reason();
  doc["rstReason"] =
    rr == ESP_RST_POWERON  ? "Power-on"    :
    rr == ESP_RST_SW       ? "SW restart"  :
    rr == ESP_RST_PANIC    ? "Panic"       :
    rr == ESP_RST_TASK_WDT ? "Watchdog"    :
    rr == ESP_RST_BROWNOUT ? "Brownout"    : "Other";
  doc["rssi"] = wifi::isStationConnected() ? WiFi.RSSI() : 0;
  doc["hostname"] = "smartmeterats.local";
  doc["ip"]   = wifi::currentIp();
  doc["timeSrc"] = state::rtcOK        ? "RTC (DS3231)"
                 : wifi::hasNtpSync()  ? "NTP"
                 : state::bootEpoch    ? "Browser sync" : "None";
  STATE_LOCK();
  doc["meterCount"] = state::activeMeterCount;
  STATE_UNLOCK();
  // RTC health — I2C is Core 1 only, same as this handler.
  doc["rtcOK"]        = state::rtcOK;
  doc["rtcLostPower"] = state::rtcLostPower;   // true = backup battery dead/missing
  if (state::rtcOK) {
    DateTime now = hardware::rtc::now();
    char buf[24];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
             now.year(), now.month(), now.day(),
             now.hour(), now.minute(), now.second());
    doc["rtcTime"] = buf;
  }
  if (state::lastTimeSyncEpoch) {
    DateTime t(state::lastTimeSyncEpoch);
    char buf[24];
    snprintf(buf, sizeof(buf), "%02d-%02d %02d:%02d", t.month(), t.day(),
             t.hour(), t.minute());
    doc["lastSync"] = buf;
  }
  String out;
  serializeJson(doc, out);
  json::sendJson(server, out);
}

// GET /api/backup — export every persisted setting as a downloadable
// file. Statistics (daily/today/month counters) are runtime data, not
// configuration, and are deliberately excluded.
void handleBackup() {
  StaticJsonDocument<config::JSON_BACKUP_SIZE> doc;
  STATE_LOCK();
  doc["ver"]    = config::BACKUP_SCHEMA_VERSION;
  doc["mcount"] = state::activeMeterCount;
  doc["active"] = state::activeMeter;
  doc["bypass"] = state::bypassMode;
  doc["rday"]   = state::monthlyResetDay;
  doc["ov"] = state::ovVoltThresh;
  doc["uv"] = state::uvVoltThresh;
  doc["oc"] = state::ocCurrThresh;
  doc["wmode"] = state::wifiMode;
  doc["wssid"] = state::staSsid;
  doc["wpass"] = state::staPass;                  // needed for a working restore
  doc["otapass"] = state::otaPassword;            // round-trip the OTA password
  // Monthly-reset state — without these a wipe+restore loses lastMonth
  // and the reset markers, misfiring the next reset. Key names mirror
  // the NVS keys so the backup is self-describing.
  doc["lastmon"] = state::lastMonthUsed;
  doc["lmon"]    = state::lastResetMonth;
  doc["lyear"]   = state::lastResetYear;
  JsonArray lims = doc.createNestedArray("limits");
  JsonArray enab = doc.createNestedArray("enabled");
  for (int i = 0; i < MAX_METERS; i++) {   // all slots, not just the active window
    lims.add(state::meters[i].energyLimit);
    enab.add(state::meters[i].enabled);
  }
  STATE_UNLOCK();
  String out;
  serializeJson(doc, out);
  json::sendCommonHeaders(server);
  server.sendHeader("Content-Disposition",
                    "attachment; filename=smartats-config.json");
  server.send(200, "application/json", out);
}

// ------------------------------------------------------------
//  Mutating endpoints — every one that changes relay state delegates
//  to core::controller so the safety guards live in one place; the
//  rest write settings under the state lock and mark NVS dirty.
// ------------------------------------------------------------

// GET /api/setLimits?l0=..&l1=..  — per-meter energy limits (kWh) for
// the active window. A limit change that already exceeds the active
// meter is re-evaluated immediately by the scheduler.
void handleSetLimits() {
  char argName[8];
  STATE_LOCK();
  for (int i = 0; i < state::activeMeterCount; i++) {
    snprintf(argName, sizeof(argName), "l%d", i);
    if (server.hasArg(argName)) {
      state::meters[i].energyLimit = server.arg(argName).toFloat();
    }
    // Clamp to the documented sane range.
    if (state::meters[i].energyLimit <= 0 ||
        state::meters[i].energyLimit > config::ENERGY_LIMIT_MAX) {
      state::meters[i].energyLimit = config::ENERGY_LIMIT_DEFAULT;
    }
  }
  // If the active meter's new limit is already reached, switch now
  // instead of waiting for the next scheduler cycle.
  autoswitch::reevaluateAfterLimitChange();
  STATE_UNLOCK();

  nvs::markDirty();   // Save Limits sends up to 10 values — one write
  json::sendOk(server);
}

// GET /api/setEnabled?idx=0..N-1&val=0/1
void handleSetEnabled() {
  if (server.hasArg("idx") && server.hasArg("val")) {
    int idx = server.arg("idx").toInt();
    STATE_LOCK();
    bool valid = (idx >= 0 && idx < state::activeMeterCount);
    if (valid) state::meters[idx].enabled = (server.arg("val").toInt() == 1);
    STATE_UNLOCK();
    if (valid) nvs::markDirty();   // toggling several in a row batches
  }
  json::sendOk(server);
}

// GET /api/switchMeter?m=0..N-1
void handleSwitchMeter() {
  if (!server.hasArg("m")) { json::sendError(server, 400, F("m required")); return; }
  sendResult(controller::requestSwitch(server.arg("m").toInt()), 409);
}

// GET /api/resetEnergy
void handleResetEnergy() {
  sendResult(controller::requestResetEnergy(), 409);
}

// GET /api/emergency — authenticated hard OFF.
void handleEmergency() {
  if (!requireAuth()) return;
  sendResult(controller::requestEmergencyOff("web"), 409);
}

// GET /api/setProtection?ov=&uv=&oc= — thresholds, each clamped to its
// documented range; absent or out-of-range args leave that value alone.
void handleSetProtection() {
  STATE_LOCK();
  if (server.hasArg("ov")) {
    float v = server.arg("ov").toFloat();
    if (v >= config::OVER_VOLTAGE_MIN && v <= config::OVER_VOLTAGE_MAX)
      state::ovVoltThresh = v;
  }
  if (server.hasArg("uv")) {
    float v = server.arg("uv").toFloat();
    if (v >= config::UNDER_VOLTAGE_MIN && v <= config::UNDER_VOLTAGE_MAX)
      state::uvVoltThresh = v;
  }
  if (server.hasArg("oc")) {
    float v = server.arg("oc").toFloat();
    if (v >= config::OVER_CURRENT_MIN && v <= config::OVER_CURRENT_MAX)
      state::ocCurrThresh = v;
  }
  STATE_UNLOCK();
  nvs::markDirty();   // thresholds are routine config — batch
  json::sendOk(server);
}

// GET /api/clearFault
void handleClearFault() {
  sendResult(controller::requestClearFault(), 409);
}

// GET /api/setActiveMeters?n=1..MAX_METERS — the one per-install
// setting. The dashboard rebuilds itself from the new count.
void handleSetActiveMeters() {
  if (!server.hasArg("n")) { json::sendError(server, 400, F("n required")); return; }
  sendResult(controller::requestSetActiveMeters(server.arg("n").toInt()), 400);
}

// GET /api/addMeter — append one meter slot with factory defaults.
void handleAddMeter() {
  int newIdx = -1, newCount = 0;
  controller::Result r = controller::requestAddMeter(newIdx, newCount);
  if (!r.ok) { json::sendError(server, 400, r.msg); return; }
  json::sendOk(server, String("\"idx\":") + newIdx + ",\"meterCount\":" + newCount);
}

// GET /api/removeMeter?idx=0..N-1 — remove one slot, shifting the
// slots above it down. Physical wiring never moves (GPIO i stays bound
// to index i); only metadata shifts.
void handleRemoveMeter() {
  if (!server.hasArg("idx")) { json::sendError(server, 400, F("idx required")); return; }
  int newCount = 0;
  controller::Result r = controller::requestRemoveMeter(server.arg("idx").toInt(), newCount);
  if (!r.ok) { json::sendError(server, 400, r.msg); return; }
  json::sendOk(server, String("\"meterCount\":") + newCount);
}

// GET /api/setBypass?val=0/1 — suspend automatic limit-based switching.
void handleSetBypass() {
  if (!server.hasArg("val")) { json::sendError(server, 400, F("val required")); return; }
  sendResult(controller::requestSetBypass(server.arg("val").toInt() == 1), 409);
}

// GET /api/testMode?on=0|1 — enter/exit relay test mode.
void handleTestMode() {
  if (!server.hasArg("on")) { json::sendError(server, 400, F("on required")); return; }
  sendResult(controller::requestTestMode(server.arg("on").toInt() == 1), 409);
}

// GET /api/testRelay?idx=N — toggle one relay, only inside test mode.
void handleTestRelay() {
  if (!server.hasArg("idx")) { json::sendError(server, 400, F("idx required")); return; }
  bool newState = false;
  controller::Result r = controller::requestTestToggle(server.arg("idx").toInt(), newState);
  if (!r.ok) { json::sendError(server, 409, r.msg); return; }
  json::sendOk(server, String("\"on\":") + (newState ? "true" : "false"));
}

// GET /api/setWiFi?mode=0|1|2&ssid=&pass= — persist and apply new
// credentials. wifi:: reconfigures the radio a moment later so this
// response reaches the client before the link drops.
void handleSetWiFi() {
  if (!server.hasArg("mode")) { json::sendError(server, 400, F("mode required (0-2)")); return; }
  String ssid = server.hasArg("ssid") ? server.arg("ssid") : String();
  String pass = server.hasArg("pass") ? server.arg("pass") : String();
  String reason;
  if (wifi::applyConfiguration((uint8_t)server.arg("mode").toInt(),
                               server.hasArg("ssid") ? ssid : state::staSsid,
                               server.hasArg("pass") ? pass : state::staPass,
                               reason)) {
    json::sendOk(server);
  } else {
    json::sendError(server, 400, reason);
  }
}

// GET /api/connectWiFi?mode=&ssid=&pass= — start an immediate STA
// association while keeping the access point up.
void handleConnectWiFi() {
  if (!server.hasArg("ssid")) { json::sendError(server, 400, F("ssid required")); return; }
  String pass = server.hasArg("pass") ? server.arg("pass") : String();
  uint8_t mode = server.hasArg("mode") ? (uint8_t)server.arg("mode").toInt()
                                       : state::wifiMode;
  String reason;
  if (wifi::connectNow(mode, server.arg("ssid"), pass, reason)) {
    json::sendOk(server);
  } else {
    json::sendError(server, 400, reason);
  }
}

// GET /api/setResetDay?d=1..28 — configurable monthly reset day.
void handleSetResetDay() {
  if (server.hasArg("d")) {
    int d = server.arg("d").toInt();
    if (d >= config::RESET_DAY_MIN && d <= config::RESET_DAY_MAX) {
      STATE_LOCK();
      state::monthlyResetDay = (uint8_t)d;
      STATE_UNLOCK();
      nvs::markDirty();
      json::sendOk(server);
      return;
    }
  }
  json::sendError(server, 400, F("d must be 1-28"));
}

// GET /api/setTime?y=&mo=&d=&h=&mi=&s= — browser time sync. Repairs the
// DS3231 when present and rebases bootEpoch so event timestamps become
// wall-clock even with no RTC at all.
void handleSetTime() {
  if (server.hasArg("y") && server.hasArg("mo") && server.hasArg("d") &&
      server.hasArg("h") && server.hasArg("mi") && server.hasArg("s")) {
    int y  = server.arg("y").toInt(),  mo = server.arg("mo").toInt();
    int d  = server.arg("d").toInt(),  h  = server.arg("h").toInt();
    int mi = server.arg("mi").toInt(), s  = server.arg("s").toInt();
    if (y >= config::MIN_VALID_YEAR && y <= config::MAX_VALID_YEAR &&
        mo >= 1 && mo <= 12 && d >= 1 && d <= 31 &&
        h >= 0 && h <= 23 && mi >= 0 && mi <= 59 && s >= 0 && s <= 59) {
      DateTime t(y, mo, d, h, mi, s);
      hardware::rtc::adjust(t);              // I2C from Core 1 — this is Core 1
      hardware::rtc::applyTimeSync(t.unixtime());
      eventlog::add("Time set from browser: %04d-%02d-%02d", y, mo, d);
      json::sendOk(server);
      return;
    }
  }
  json::sendError(server, 400, F("invalid date/time"));
}

// GET /api/clearEvents — wipe the in-RAM event log.
void handleClearEvents() {
  eventlog::clear();
  eventlog::add("Event log cleared");
  json::sendOk(server);
}

// POST /api/restore — import a config JSON. Every field is optional and
// validated with the same clamps load() uses; a malformed file changes
// nothing. Authenticated, since it rewrites the whole configuration.
void handleRestore() {
  if (!requireAuth()) return;
  StaticJsonDocument<config::JSON_BACKUP_SIZE> doc;
  DeserializationError err = deserializeJson(doc, server.arg("plain"));
  if (err) { json::sendError(server, 400, F("invalid JSON")); return; }

  STATE_LOCK();
  if (doc.containsKey("mcount")) {
    int n = doc["mcount"];
    if (n >= 1 && n <= MAX_METERS) state::activeMeterCount = (uint8_t)n;
  }
  if (doc.containsKey("limits")) {
    JsonArray a = doc["limits"];
    for (int i = 0; i < MAX_METERS && i < (int)a.size(); i++) {
      float v = a[i];
      if (!isnan(v) && v > 0 && v <= config::ENERGY_LIMIT_MAX)
        state::meters[i].energyLimit = v;
    }
  }
  if (doc.containsKey("enabled")) {
    JsonArray a = doc["enabled"];
    for (int i = 0; i < MAX_METERS && i < (int)a.size(); i++) {
      state::meters[i].enabled = (bool)a[i];
    }
  }
  if (doc.containsKey("rday")) {
    int d = doc["rday"];
    if (d >= config::RESET_DAY_MIN && d <= config::RESET_DAY_MAX)
      state::monthlyResetDay = (uint8_t)d;
  }
  if (doc.containsKey("ov")) { float v = doc["ov"];
    if (v >= config::OVER_VOLTAGE_MIN  && v <= config::OVER_VOLTAGE_MAX)  state::ovVoltThresh = v; }
  if (doc.containsKey("uv")) { float v = doc["uv"];
    if (v >= config::UNDER_VOLTAGE_MIN && v <= config::UNDER_VOLTAGE_MAX) state::uvVoltThresh = v; }
  if (doc.containsKey("oc")) { float v = doc["oc"];
    if (v >= config::OVER_CURRENT_MIN  && v <= config::OVER_CURRENT_MAX)  state::ocCurrThresh = v; }
  if (doc.containsKey("bypass")) state::bypassMode = (bool)doc["bypass"];
  if (doc.containsKey("wmode")) {
    int m = doc["wmode"];
    if (m >= config::WIFI_MODE_AP_ONLY && m <= config::WIFI_MODE_AP_STA)
      state::wifiMode = (uint8_t)m;
  }
  if (doc.containsKey("wssid")) {
    String s = doc["wssid"].as<String>();
    if (s.length() <= config::MAX_SSID_LEN) state::staSsid = s;
  }
  if (doc.containsKey("wpass")) {
    String s = doc["wpass"].as<String>();
    if (s.length() <= config::MAX_PASS_LEN) state::staPass = s;
  }
  if (doc.containsKey("otapass")) {
    String s = doc["otapass"].as<String>();
    if (s.length() <= config::MAX_PASS_LEN) state::otaPassword = s;
  }
  if (doc.containsKey("lastmon")) {
    float v = doc["lastmon"];
    if (!isnan(v) && v >= 0) state::lastMonthUsed = v;
  }
  if (doc.containsKey("lmon")) {
    int m = doc["lmon"];
    if (m >= 1 && m <= 12) state::lastResetMonth = (uint8_t)m;
  }
  if (doc.containsKey("lyear")) {
    int y = doc["lyear"];
    if (y >= config::MIN_VALID_YEAR) state::lastResetYear = (uint16_t)y;
  }
  // Same guard as load(): STA modes need an SSID.
  if (state::staSsid.length() == 0) state::wifiMode = config::WIFI_MODE_AP_ONLY;
  STATE_UNLOCK();

  // The imported config may disable the active meter or shrink the
  // window. Re-validate exactly like boot — routed through the
  // controller so this layer never touches relays directly.
  controller::validateActiveMeter(true);

  nvs::forceSave();                    // config import is an explicit save
  eventlog::add("Config restored from backup");
  json::sendOk(server);
}

// GET /api/factoryReset — de-energise, erase the whole NVS namespace
// and reboot; the next boot loads defaults. Authenticated.
void handleFactoryReset() {
  if (!requireAuth()) return;
  Serial.println(F("[SYS] FACTORY RESET requested"));
  controller::enterSafeState();        // de-energise before wiping config
  nvs::factoryReset();
  json::sendOk(server);
  ota::requestRestart();               // reboot after the response flushes
}

}  // namespace (internal linkage — handlers and helpers above)

// ------------------------------------------------------------
//  Route registration and servicing
// ------------------------------------------------------------
void begin() {
  // Dashboard — served straight from PROGMEM.
  server.on("/", HTTP_GET, []() {
    json::sendCommonHeaders(server);
    server.send_P(200, "text/html", ui::DASHBOARD_HTML);
  });

  // Read-only endpoints.
  server.on("/api/status",     HTTP_GET, handleStatus);
  server.on("/api/wifiStatus", HTTP_GET, handleWifiStatus);
  server.on("/api/scanWiFi",   HTTP_GET, handleScanWiFi);
  server.on("/api/events",     HTTP_GET, handleEvents);
  server.on("/api/sysinfo",    HTTP_GET, handleSysInfo);
  server.on("/api/backup",     HTTP_GET, handleBackup);

  // Configuration and control.
  server.on("/api/setLimits",       HTTP_GET, handleSetLimits);
  server.on("/api/setEnabled",      HTTP_GET, handleSetEnabled);
  server.on("/api/switchMeter",     HTTP_GET, handleSwitchMeter);
  server.on("/api/resetEnergy",     HTTP_GET, handleResetEnergy);
  server.on("/api/emergency",       HTTP_GET, handleEmergency);
  server.on("/api/setProtection",   HTTP_GET, handleSetProtection);
  server.on("/api/clearFault",      HTTP_GET, handleClearFault);
  server.on("/api/setActiveMeters", HTTP_GET, handleSetActiveMeters);
  server.on("/api/addMeter",        HTTP_GET, handleAddMeter);
  server.on("/api/removeMeter",     HTTP_GET, handleRemoveMeter);
  server.on("/api/setBypass",       HTTP_GET, handleSetBypass);
  server.on("/api/setWiFi",         HTTP_GET, handleSetWiFi);
  server.on("/api/setResetDay",     HTTP_GET, handleSetResetDay);
  server.on("/api/connectWiFi",     HTTP_GET, handleConnectWiFi);
  server.on("/api/clearEvents",     HTTP_GET, handleClearEvents);
  server.on("/api/restore",         HTTP_POST, handleRestore);
  server.on("/api/factoryReset",    HTTP_GET, handleFactoryReset);
  server.on("/api/testMode",        HTTP_GET, handleTestMode);
  server.on("/api/testRelay",       HTTP_GET, handleTestRelay);
  server.on("/api/setTime",         HTTP_GET, handleSetTime);

  // Browser firmware upload — the first callback sends the final reply
  // once the body has streamed; the second streams the chunks into
  // Update. Both auth and flash handling live in ota::.
  server.on("/api/update", HTTP_POST,
            []() { ota::finishFirmwareUpload(server); },
            []() { ota::handleFirmwareUpload(server); });

  server.begin();
  Serial.println(F("[WEB] HTTP server started"));
}

void handleClient() {
  server.handleClient();
}

}  // namespace web
}  // namespace ui
