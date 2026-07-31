#include "wifi_manager.h"

#include <ESPmDNS.h>
#include <WiFi.h>
#include <time.h>

#include "../core/config.h"
#include "../core/event_log.h"
#include "../core/system_state.h"
#include "../hardware/rtc_manager.h"
#include "../storage/settings_storage.h"
#include "../utilities/json_response.h"

namespace network {
namespace wifi {

namespace config = core::config;
namespace state  = core::state;
namespace nvs    = storage::settings;

namespace {

bool          staConnected    = false;
bool          staGaveUp       = false;   // fell back to AP after the timeout
unsigned long staConnectStart = 0;
String        staError;                  // last failure reason, for the dashboard

bool          applyPending = false;      // deferred radio reconfigure
unsigned long applyAtMs    = 0;

bool     ntpConfigured = false;
uint32_t ntpSyncEpoch  = 0;

void startAccessPoint() {
  WiFi.softAPConfig(config::AP_IP, config::AP_IP, config::AP_SUBNET);
  WiFi.softAP(config::AP_SSID, config::AP_PASSWORD);
}

// The responder binds to the interfaces that exist when it starts and
// stops answering on any that appear later, so it is restarted after
// every interface change. end() first, because begin() on an already
// running responder is a no-op.
void restartMdns() {
  MDNS.end();
  MDNS.begin(config::MDNS_HOSTNAME);
  MDNS.addService("_http", "_tcp", config::HTTP_PORT);
  Serial.printf("[mDNS] http://%s.local/\n", config::MDNS_HOSTNAME);
}

// Registers the servers only. The first lookup is asynchronous, so the
// epoch is adopted later in handleNtpSync() and nothing blocks here.
void startNtp() {
  if (ntpConfigured) return;
  // UTC: bootEpoch and DateTime are UTC throughout the firmware.
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  ntpConfigured = true;
  Serial.println(F("[NTP] configured — awaiting first sync"));
}

void handleNtpSync() {
  // A healthy DS3231 remains the authority; NTP is the fallback for a
  // missing chip or a dead backup battery.
  if (!ntpConfigured || !staConnected) return;
  if (state::rtcOK && !state::rtcLostPower) return;

  static unsigned long lastPoll = 0;
  if (millis() - lastPoll < config::NTP_POLL_INTERVAL_MS) return;
  lastPoll = millis();

  struct tm timeInfo;
  if (!getLocalTime(&timeInfo, 0)) return;         // not synced yet
  const uint32_t epoch = (uint32_t)mktime(&timeInfo);
  if (epoch < config::MIN_VALID_EPOCH) return;     // implausible, ignore
  if (epoch == ntpSyncEpoch) return;               // nothing new

  const bool first = (ntpSyncEpoch == 0);
  ntpSyncEpoch = epoch;
  hardware::rtc::applyTimeSync(epoch);
  if (first) core::eventlog::add("NTP time synced");
}

// Reconfigure the radio for the currently persisted mode.
void startRadio() {
  staConnected = false;
  staGaveUp    = false;

  if (state::wifiMode == config::WIFI_MODE_AP_ONLY || state::staSsid.length() == 0) {
    WiFi.mode(WIFI_AP);
    startAccessPoint();
    Serial.printf("[WiFi] AP started  SSID: %s  IP: %s\n",
                  config::AP_SSID, WiFi.softAPIP().toString().c_str());
    restartMdns();
    return;
  }

  // Station modes still bring the AP up. In station-only it is torn
  // down once, and only once, the link is proven.
  WiFi.mode(WIFI_AP_STA);
  startAccessPoint();
  WiFi.begin(state::staSsid.c_str(), state::staPass.c_str());
  staConnectStart = millis();
  Serial.printf("[WiFi] AP up (%s @ %s), connecting STA to \"%s\"...\n",
                config::AP_SSID, WiFi.softAPIP().toString().c_str(),
                state::staSsid.c_str());
  restartMdns();
}

// Shared validation for both configuration entry points.
bool validateCredentials(uint8_t mode, String& ssid, String& pass, String& reason) {
  if (mode > config::WIFI_MODE_AP_STA) {
    reason = F("mode must be 0-2");
    return false;
  }
  if (ssid.length() > config::MAX_SSID_LEN) ssid = ssid.substring(0, config::MAX_SSID_LEN);
  if (pass.length() > config::MAX_PASS_LEN) pass = pass.substring(0, config::MAX_PASS_LEN);
  if (mode != config::WIFI_MODE_AP_ONLY && ssid.length() == 0) {
    reason = F("ssid required for station modes");
    return false;
  }
  return true;
}

void onLinkEstablished() {
  staConnected = true;
  staError     = "";
  Serial.printf("[WiFi] STA connected to \"%s\"  IP: %s\n",
                state::staSsid.c_str(), WiFi.localIP().toString().c_str());
  core::eventlog::add("WiFi connected: %s", WiFi.localIP().toString().c_str());
  startNtp();
  if (state::wifiMode == config::WIFI_MODE_STA_ONLY) {
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    Serial.println(F("[WiFi] STA_ONLY: Access Point stopped"));
  }
  restartMdns();
}

void onLinkLost() {
  staConnected  = false;
  ntpConfigured = false;   // re-register NTP on the next link
  Serial.println(F("[WiFi] STA link lost — retrying"));
  core::eventlog::add("WiFi disconnected");
  if (state::wifiMode == config::WIFI_MODE_STA_ONLY) {
    // Bring the AP back so the dashboard stays reachable meanwhile.
    WiFi.mode(WIFI_AP_STA);
    startAccessPoint();
  }
  restartMdns();
  WiFi.begin(state::staSsid.c_str(), state::staPass.c_str());
  staConnectStart = millis();
}

void onConnectTimeout() {
  staGaveUp = true;
  staError  = (WiFi.status() == WL_NO_SSID_AVAIL) ? F("Network not found")
                                                  : F("Connection timeout");
  Serial.printf("[WiFi] STA connect to \"%s\" failed after %lus — falling back to AP\n",
                state::staSsid.c_str(), config::STA_CONNECT_TIMEOUT_MS / 1000);
  // Runtime fallback only: NVS keeps the configured mode so the next
  // boot retries instead of silently demoting the installation.
  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP);
  startAccessPoint();
  Serial.printf("[WiFi] AP restored  SSID: %s  IP: %s\n",
                config::AP_SSID, WiFi.softAPIP().toString().c_str());
  restartMdns();
}

}  // namespace

void begin() {
  startRadio();
}

void handleStation() {
  if (applyPending && (millis() - applyAtMs) >= config::WIFI_APPLY_DELAY_MS) {
    applyPending = false;
    Serial.println(F("[WiFi] Applying new WiFi configuration"));
    WiFi.disconnect(true, true);
    startRadio();
    return;
  }

  handleNtpSync();

  if (state::wifiMode == config::WIFI_MODE_AP_ONLY || staGaveUp ||
      state::staSsid.length() == 0) {
    return;
  }

  static unsigned long lastPoll = 0;
  if (millis() - lastPoll < config::WIFI_POLL_INTERVAL_MS) return;
  lastPoll = millis();

  const bool linked = (WiFi.status() == WL_CONNECTED);
  if (linked && !staConnected) {
    onLinkEstablished();
  } else if (!linked && staConnected) {
    onLinkLost();
  } else if (!linked && !staConnected &&
             (millis() - staConnectStart) > config::STA_CONNECT_TIMEOUT_MS) {
    onConnectTimeout();
  }
}

bool isStationConnected() { return staConnected; }

const String& lastError() { return staError; }

bool hasNtpSync() { return ntpSyncEpoch != 0; }

String currentIp() {
  return staConnected ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
}

bool applyConfiguration(uint8_t mode, const String& ssid, const String& pass,
                        String& reason) {
  String workingSsid = ssid;
  String workingPass = pass;
  if (!validateCredentials(mode, workingSsid, workingPass, reason)) return false;

  state::wifiMode = mode;
  state::staSsid  = workingSsid;
  state::staPass  = workingPass;
  nvs::forceSave();          // connectivity config must survive a reboot

  applyPending = true;       // radio reconfigures after the response flushes
  applyAtMs    = millis();
  Serial.printf("[WiFi] Config saved: mode=%u ssid=\"%s\" — applying shortly\n",
                mode, workingSsid.c_str());
  return true;
}

bool connectNow(uint8_t mode, const String& ssid, const String& pass,
                String& reason) {
  String workingSsid = ssid;
  String workingPass = pass;
  if (workingSsid.length() == 0) {
    reason = F("ssid required");
    return false;
  }
  // An AP-only device cannot hold a station link, so a connect request
  // implies at least AP+STA.
  if (mode == config::WIFI_MODE_AP_ONLY || mode > config::WIFI_MODE_AP_STA) {
    mode = config::WIFI_MODE_AP_STA;
  }
  if (!validateCredentials(mode, workingSsid, workingPass, reason)) return false;

  state::wifiMode = mode;
  state::staSsid  = workingSsid;
  state::staPass  = workingPass;
  staConnected    = false;
  staGaveUp       = false;
  staError        = "";

  WiFi.disconnect(true, false);   // drop the old link, keep the AP config
  WiFi.mode(WIFI_AP_STA);
  startAccessPoint();             // dashboard stays reachable during the attempt
  WiFi.begin(state::staSsid.c_str(), state::staPass.c_str());
  staConnectStart = millis();

  nvs::forceSave();
  Serial.printf("[WiFi] connect requested: \"%s\" mode=%u\n",
                workingSsid.c_str(), mode);
  core::eventlog::add("WiFi connect: %s", workingSsid.c_str());
  return true;
}

String scanNetworksJson() {
  int found = WiFi.scanNetworks(false, false);   // synchronous, skip hidden
  if (found < 0) found = 0;

  String out = F("{\"networks\":[");
  int added = 0;
  for (int i = 0; i < found && added < config::MAX_SCAN_RESULTS; i++) {
    const String ssid = WiFi.SSID(i);
    if (ssid.length() == 0) continue;            // hidden network
    if (added++) out += ',';
    // An SSID is arbitrary user-controlled text, so it is escaped.
    out += F("{\"ssid\":\"");
    out += utilities::json::escape(ssid);
    out += F("\",\"rssi\":");
    out += WiFi.RSSI(i);
    out += F(",\"encrypted\":");
    out += (WiFi.encryptionType(i) != WIFI_AUTH_OPEN) ? F("true") : F("false");
    out += F(",\"channel\":");
    out += WiFi.channel(i);
    out += '}';
  }
  out += F("]}");
  WiFi.scanDelete();               // free the result buffer straight away
  return out;
}

}  // namespace wifi
}  // namespace network
