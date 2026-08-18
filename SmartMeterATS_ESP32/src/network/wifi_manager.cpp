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

// Latched by disconnectStation() so the poll loop stays off the radio
// until the user explicitly reconnects or applies a new configuration.
// The AP is unaffected — the dashboard stays reachable the whole time.
bool          userDisconnected = false;

bool          applyPending = false;      // deferred radio reconfigure
unsigned long applyAtMs    = 0;

// Station Only: the SoftAP is not torn down the instant the station link is
// proven. It stays up while a browser is still connected to it so the
// dashboard can be handed off to the router address (mDNS / STA IP) first;
// the AP is retired later, once idle. Set when the link establishes and
// evaluated from handleStation().
bool          staOnlyApTeardown   = false;
unsigned long staOnlyApTeardownAt = 0;

bool     ntpConfigured = false;
uint32_t ntpSyncEpoch  = 0;

// After an AP fallback, retry the configured STA network on this cadence so a
// transient outage at boot doesn't strand the device in AP mode until reboot.
const unsigned long STA_RETRY_INTERVAL_MS = 60000;   // 1 min

// WPA2-personal minimum passphrase length. A non-empty password shorter than
// this can never associate, so it's rejected up front instead of being saved
// as a permanent silent connect failure. (Empty == open network, allowed.)
const uint8_t WPA_MIN_PASS_LEN = 8;

// The ESP32 core has no WiFi.disconnectReason(), so the last disconnect is
// captured from the WIFI_EVENT_STA_DISCONNECTED event and classified here.
wifi_err_reason_t lastDisconnectReason = WIFI_REASON_UNSPECIFIED;

void startAccessPoint() {
  WiFi.softAPConfig(config::AP_IP, config::AP_IP, config::AP_SUBNET);
  WiFi.softAP(config::AP_SSID, state::apPassword.c_str());
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
  staOnlyApTeardown = false;
  lastDisconnectReason = WIFI_REASON_UNSPECIFIED;

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
  if (ssid.length() > config::SSID_MAX_LENGTH) ssid = ssid.substring(0, config::SSID_MAX_LENGTH);
  if (pass.length() > config::PASS_MAX_LENGTH) pass = pass.substring(0, config::PASS_MAX_LENGTH);
  if (mode != config::WIFI_MODE_AP_ONLY && ssid.length() == 0) {
    reason = F("ssid required for station modes");
    return false;
  }
  // WPA2-personal requires >=8 characters. A shorter non-empty password can
  // never associate and produces a silent permanent connect failure. Empty
  // password == open network, allowed.
  if (pass.length() > 0 && pass.length() < WPA_MIN_PASS_LEN) {
    reason = F("password must be 8+ characters or empty");
    return false;
  }
  return true;
}

// Reuse the saved password when the caller reconnects to the network that is
// already stored. The dashboard never echoes the secret back to the browser,
// so a reconnect leaves the password field blank — and that blank must mean
// "use the saved password", never "this network is now open". Without this
// guard the blank would be written over the stored password in RAM, NVS and
// the WiFi driver's flash config, which is exactly the "password lost after
// Disconnect" symptom.
void reuseSavedPassword(String& ssid, String& pass) {
  if (pass.length() == 0 && ssid.equalsIgnoreCase(state::staSsid)) {
    pass = state::staPass;
  }
}

void onLinkEstablished() {
  staConnected = true;
  staError     = "";
  Serial.printf("[WiFi] STA connected to \"%s\"  IP: %s\n",
                state::staSsid.c_str(), WiFi.localIP().toString().c_str());
  core::eventlog::add("WiFi connected: %s", WiFi.localIP().toString().c_str());
  startNtp();
  if (state::wifiMode == config::WIFI_MODE_STA_ONLY) {
    // Station Only — but do NOT drop the SoftAP yet: a browser connected to
    // it must learn the new STA address and be handed off before the AP can
    // disappear. The AP is retired from handleStation() once the grace
    // period has passed and no client is associated any more.
    staOnlyApTeardown   = true;
    staOnlyApTeardownAt = millis();
    Serial.println(F("[WiFi] STA_ONLY: AP kept alive for client handoff"));
  }
  restartMdns();
}

// Station Only cleanup: once the handoff grace has passed and no client is
// associated to the SoftAP any more, drop it so the install returns to a
// true station-only node. If a client is still attached, the AP stays up —
// the transition must never strand a connected dashboard.
void retireApIfIdle() {
  if (!staOnlyApTeardown || !staConnected) return;
  if ((millis() - staOnlyApTeardownAt) < config::STA_ONLY_AP_GRACE_MS) return;
  if (WiFi.softAPgetStationNum() > 0) return;
  staOnlyApTeardown = false;
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  Serial.println(F("[WiFi] STA_ONLY: Access Point stopped (idle)"));
}

// Records the reason code from the STA disconnect event. Called from the WiFi
// event task; it only writes a scalar, so it is safe to keep this light.
void onStaDisconnected(const arduino_event_info_t& info) {
  lastDisconnectReason = (wifi_err_reason_t)info.wifi_sta_disconnected.reason;
}

void onLinkLost() {
  staConnected  = false;
  staOnlyApTeardown = false;   // a fresh link may re-arm it; AP stays up meanwhile
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

unsigned long staGaveUpAt = 0;   // millis() of the last AP fallback

// Map the WiFi status/reason codes to a human-readable reason the
// dashboard can show verbatim. The ESP32 core collapses both a wrong
// password and a generic failure into WL_CONNECT_FAILED, so the finer
// detail comes from the last captured STA disconnect event reason.
void classifyConnectError() {
  const wifi_err_reason_t reason = lastDisconnectReason;
  if (reason == WIFI_REASON_AUTH_FAIL ||
      reason == WIFI_REASON_AUTH_EXPIRE ||
      reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT ||
      reason == WIFI_REASON_802_1X_AUTH_FAILED) {
    staError = F("Incorrect password");
  } else if (reason == WIFI_REASON_NO_AP_FOUND ||
             WiFi.status() == WL_NO_SSID_AVAIL) {
    staError = F("Network not found");
  } else if (reason != WIFI_REASON_UNSPECIFIED &&
             reason != WIFI_REASON_AUTH_LEAVE) {
    staError = F("Connection failed");
  } else {
    staError = F("Connection timeout");
  }
}

void onConnectTimeout() {
  staGaveUp   = true;
  staGaveUpAt = millis();
  staOnlyApTeardown = false;   // runtime fallback is AP only
  classifyConnectError();
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
  WiFi.onEvent([](arduino_event_id_t event, arduino_event_info_t info) {
      onStaDisconnected(info);
    },
    ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  startRadio();
  // Mains-powered device: disable WiFi modem-sleep power save. The core's
  // default WIFI_PS_MIN_MODEM lets the radio sleep between beacons, which
  // delays softAP beacon/response delivery and is a documented cause of AP
  // clients seeing lag or random disconnects — worst in AP+STA coexistence.
  WiFi.setSleep(false);
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

  retireApIfIdle();   // Station Only: drop the AP once the handoff is done

  if (userDisconnected) return;   // user chose to stay off until reconnecting

  if (state::wifiMode == config::WIFI_MODE_AP_ONLY ||
      state::staSsid.length() == 0) {
    return;
  }

  // After an AP fallback, periodically re-attempt the configured STA network:
  // a transient outage at boot must not strand the device in AP mode until a
  // manual reboot. Clearing staGaveUp lets the poll loop below drive a fresh
  // connect attempt (and a new timeout if it fails again).
  if (staGaveUp) {
    if ((millis() - staGaveUpAt) < STA_RETRY_INTERVAL_MS) return;
    Serial.println(F("[WiFi] Retrying configured STA network after AP fallback"));
    staGaveUp = false;
    lastDisconnectReason = WIFI_REASON_UNSPECIFIED;
    WiFi.mode(WIFI_AP_STA);
    startAccessPoint();          // keep the dashboard reachable during retry
    WiFi.begin(state::staSsid.c_str(), state::staPass.c_str());
    staConnectStart = millis();
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

// True while the user has explicitly disconnected the station; the device
// stays off the router until they reconnect or a new config is applied.
bool isStationOff() { return userDisconnected; }

const String& lastError() { return staError; }

bool hasNtpSync() { return ntpSyncEpoch != 0; }

String currentIp() {
  return staConnected ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
}

bool applyConfiguration(uint8_t mode, const String& ssid, const String& pass,
                        String& reason) {
  String workingSsid = ssid;
  String workingPass = pass;
  // Reconnecting to the already-saved network with a blank password must
  // reuse the stored one instead of overwriting it.
  reuseSavedPassword(workingSsid, workingPass);
  if (!validateCredentials(mode, workingSsid, workingPass, reason)) return false;

  state::wifiMode = mode;
  state::staSsid  = workingSsid;
  state::staPass  = workingPass;
  userDisconnected = false;   // an explicit connect overrides a manual disconnect
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
  // Same-network reconnect with a blank password field: reuse the stored
  // password instead of saving an empty one over it.
  reuseSavedPassword(workingSsid, workingPass);
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
  staOnlyApTeardown = false;
  staError        = "";
  lastDisconnectReason = WIFI_REASON_UNSPECIFIED;
  userDisconnected = false;   // an explicit connect overrides a manual disconnect
  applyPending    = false;   // cancel any deferred applyConfiguration() reconfigure

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

bool inApFallback() { return staGaveUp; }

void disconnectStation() {
  userDisconnected = true;
  staConnected = false;
  staGaveUp    = false;
  staOnlyApTeardown = false;   // user wants the station off — AP stays up
  staError     = "";
  // Cancel any deferred radio reconfigure: the user explicitly wants the
  // station off, and handleStation() would otherwise re-associate it a
  // second later when the queued applyConfiguration() fires.
  applyPending = false;
  // wifioff=false drops only the station link (esp_wifi_disconnect) and
  // leaves the radio/AP running. wifioff=true would call esp_wifi_stop()
  // and tear the whole radio down — in AP+STA nothing below restarts it
  // and the dashboard would go unreachable. Saved credentials stay in NVS.
  WiFi.disconnect(false, false);   // drop the link, keep saved credentials in NVS
  if (state::wifiMode == config::WIFI_MODE_STA_ONLY) {
    // Station Only may have retired the AP once it went idle; bring it
    // back so the dashboard stays reachable.
    WiFi.mode(WIFI_AP_STA);
    startAccessPoint();
  }
  restartMdns();
  Serial.println(F("[WiFi] Station disconnected by user — AP still up"));
  core::eventlog::add("WiFi disconnected by user");
}

void forgetNetwork() {
  userDisconnected = true;
  staConnected = false;
  staGaveUp    = false;
  staOnlyApTeardown = false;   // dropping to AP only — nothing to retire
  staError     = "";
  state::staSsid = "";
  state::staPass = "";
  state::wifiMode = config::WIFI_MODE_AP_ONLY;   // station modes need an SSID
  // Cancel any deferred applyConfiguration(): the credentials it would
  // start the radio with are now erased, and the redundant AP restart a
  // second later would needlessly drop whatever client just reconnected.
  applyPending = false;
  nvs::forceSave();                              // credentials must not survive
  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP);
  startAccessPoint();
  restartMdns();
  Serial.println(F("[WiFi] Saved network forgotten — AP only mode"));
  core::eventlog::add("WiFi network forgotten");
}

bool scanStart() {
  // A scan is already in flight — nothing to do, the same result will be
  // collected by the next scanPollJson().
  if (WiFi.scanComplete() == WIFI_SCAN_RUNNING) return true;
  WiFi.scanDelete();                                   // clear stale results
  // async = true: the WiFi driver scans in its own task and the loop task
  // (web server, relays, NVS) is never blocked by it.
  return (WiFi.scanNetworks(true, false) != WIFI_SCAN_FAILED);
}

String scanPollJson() {
  const int16_t state = WiFi.scanComplete();
  if (state == WIFI_SCAN_RUNNING) {
    return F("{\"status\":\"running\"}");
  }
  if (state == WIFI_SCAN_FAILED) {
    WiFi.scanDelete();
    return F("{\"status\":\"error\",\"msg\":\"scan failed\"}");
  }

  // state >= 0 — the scan finished. Serialise the visible networks, then
  // free the result buffer so a future scanStart() can begin cleanly.
  String out = F("{\"status\":\"ok\",\"networks\":[");
  int added = 0;
  for (int i = 0; i < state && added < config::MAX_SCAN_RESULTS; i++) {
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
