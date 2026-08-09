#include "ota_manager.h"

#include <ArduinoOTA.h>
#include <Update.h>
#include <esp_task_wdt.h>

#include "../core/config.h"
#include "../core/event_log.h"
#include "../core/meter_controller.h"
#include "../core/sampling_task.h"
#include "../core/system_state.h"
#include "../utilities/json_response.h"

namespace network {
namespace ota {

namespace config = core::config;
namespace state  = core::state;

namespace {

bool ready = false;

// Last time the ArduinoOTA push made progress (start or a data chunk). If a
// push stalls (network drop mid-transfer) ArduinoOTA may not fire onError
// promptly, leaving the device in safe-state (relays tripped, sampling
// paused) forever. The watchdog in handle() unwinds after this idle window.
unsigned long otaProgressMs      = 0;
const unsigned long OTA_STALL_TIMEOUT_MS = 60000;   // 60s with no progress

void enterUpdateState() {
  state::otaInProgress = true;
  core::sampling::pause();          // no Modbus/mutex traffic while flashing
  core::controller::enterSafeState();
}

// An aborted update goes back to normal operation; the controller
// refuses to re-energise past an emergency or trip latch.
void abortUpdateState() {
  state::otaInProgress = false;
  core::sampling::resume();
  core::controller::restoreOperation();
}

void onOtaStart() {
  enterUpdateState();
  otaProgressMs = millis();
  Serial.printf("[OTA] Update started (%s)\n",
                ArduinoOTA.getCommand() == U_FLASH ? "firmware" : "filesystem");
}

void onOtaProgress(unsigned int progress, unsigned int total) {
  esp_task_wdt_reset();             // uploads can exceed the WDT window
  otaProgressMs = millis();         // feed the stall watchdog
  static unsigned int lastPct = 101;
  const unsigned int pct = total ? (progress * 100) / total : 0;
  if (pct != lastPct && pct % 10 == 0) {
    lastPct = pct;
    Serial.printf("[OTA] %u%%\n", pct);
  }
}

void onOtaError(ota_error_t error) {
  // onError can fire before onStart (e.g. auth failure) — only unwind
  // the update state if flashing had actually begun.
  const bool wasFlashing = state::otaInProgress;
  if (wasFlashing) abortUpdateState();
  const char* msg =
      error == OTA_AUTH_ERROR    ? "auth failed"    :
      error == OTA_BEGIN_ERROR   ? "begin failed"   :
      error == OTA_CONNECT_ERROR ? "connect failed" :
      error == OTA_RECEIVE_ERROR ? "receive failed" :
      error == OTA_END_ERROR     ? "end failed"     : "unknown";
  Serial.printf("[OTA] Error: %s\n", msg);
}

}  // namespace

void begin() {
  ArduinoOTA.setHostname(config::OTA_HOSTNAME);
  ArduinoOTA.setPassword(state::otaPassword.c_str());

  ArduinoOTA.onStart(onOtaStart);
  ArduinoOTA.onProgress(onOtaProgress);
  ArduinoOTA.onEnd([]() {
    Serial.println(F("[OTA] Update complete — rebooting"));
  });
  ArduinoOTA.onError(onOtaError);

  ArduinoOTA.begin();
  ready = true;
  Serial.printf("[OTA] Ready — hostname: %s (password protected)\n",
                config::OTA_HOSTNAME);
}

void handle() {
  if (ready) ArduinoOTA.handle();

  // Stall watchdog: only the ArduinoOTA path uses otaInProgress via
  // onOtaStart. If a push stalls and onError never fires, unwind after the
  // idle window so the device leaves safe-state and resumes normal operation.
  // (The web-upload path is driven synchronously by the HTTP handler and does
  //  not rely on this timer.)
  if (state::otaInProgress && otaProgressMs != 0 &&
      (millis() - otaProgressMs) > OTA_STALL_TIMEOUT_MS) {
    Serial.println(F("[OTA] Push stalled — aborting and restoring operation"));
    core::eventlog::add("OTA aborted: stalled");
    Update.abort();
    abortUpdateState();
    otaProgressMs = 0;
  }
}

bool isUpdating() { return state::otaInProgress; }

bool isReady() { return ready; }

void handleFirmwareUpload(WebServer& server) {
  HTTPUpload& up = server.upload();
  // WebServer streams the body before the final handler runs, so the
  // auth gate must live here too — otherwise an unauthenticated POST
  // writes straight into the flash partition. Checked on every chunk.
  if (!server.authenticate(config::AUTH_USER, state::otaPassword.c_str())) {
    if (state::otaInProgress) {   // never hijack an authenticated stream
      Update.abort();
      abortUpdateState();
      core::eventlog::add("OTA rejected: auth failed");
    }
    return;
  }

  if (up.status == UPLOAD_FILE_START) {
    Serial.printf("[OTA] Web upload start: %s\n", up.filename.c_str());
    core::eventlog::add("OTA upload started");
    enterUpdateState();
    otaProgressMs = millis();        // arm the handle() stall watchdog for
                                     // this path too: a half-open client
                                     // socket may stop delivering chunks
                                     // without ever firing UPLOAD_FILE_ABORTED.
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      Serial.printf("[OTA] begin failed: %s\n", Update.errorString());
    }
  } else if (up.status == UPLOAD_FILE_WRITE) {
    // The ESP32 image magic byte on the very first chunk rejects
    // non-firmware files before wasting flash cycles. On rejection, abort
    // AND unwind immediately so the device leaves safe-state now instead of
    // staying tripped for the rest of a doomed upload.
    if (up.totalSize == 0 && up.currentSize > 0 && up.buf[0] != 0xE9) {
      Update.abort();
      abortUpdateState();
      core::eventlog::add("OTA rejected: not a firmware image");
      Serial.println(F("[OTA] Rejected: not an ESP32 firmware image"));
      return;
    }
    if (!Update.hasError() &&
        Update.write(up.buf, up.currentSize) != up.currentSize) {
      // A short write means the stream is broken; abort and recover now
      // rather than streaming the rest into a dead Update object.
      Serial.printf("[OTA] write failed: %s\n", Update.errorString());
      Update.abort();
      abortUpdateState();
      core::eventlog::add("OTA failed: write error");
      return;
    }
    esp_task_wdt_reset();           // large uploads exceed the WDT window
    otaProgressMs = millis();       // feed the stall watchdog on each chunk
  } else if (up.status == UPLOAD_FILE_END) {
    // Final validation of the whole image. If end() fails, recover here so
    // finishFirmwareUpload() sees the error and never reboots.
    if (!Update.end(true)) {
      Serial.printf("[OTA] end failed: %s\n", Update.errorString());
      core::eventlog::add("OTA failed: %s", Update.errorString());
      abortUpdateState();
    }
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    Update.abort();
    abortUpdateState();             // client bailed — restore normal operation
    Serial.println(F("[OTA] Upload aborted by client"));
  }
}

void finishFirmwareUpload(WebServer& server) {
  // Re-check auth so an unauthenticated request with an empty body (the
  // upload callback never ran) can still never reach the reboot path.
  if (!server.authenticate(config::AUTH_USER, state::otaPassword.c_str())) {
    Update.abort();
    utilities::json::sendError(server, 401, F("auth required"));
    return;
  }
  // Guard against a partial/absent upload: if Update was never started, never
  // wrote successfully, or didn't reach UPLOAD_FILE_END, isFinished() is false
  // and a reboot would restart into a half-flashed or empty partition (brick).
  if (!Update.isFinished()) {
    utilities::json::sendError(server, 400, F("upload incomplete or never started"));
    core::eventlog::add("OTA rejected: incomplete upload");
    abortUpdateState();
    return;
  }
  if (Update.hasError()) {
    utilities::json::sendError(server, 500, Update.errorString());
    core::eventlog::add("OTA failed: %s", Update.errorString());
    abortUpdateState();
  } else {
    utilities::json::sendOk(server);
    core::eventlog::add("OTA update OK — rebooting");
    requestRestart();
  }
}

void requestRestart() {
  state::restartPending = true;
  state::restartAtMs    = millis();
}

void handlePendingRestart() {
  if (state::restartPending &&
      (millis() - state::restartAtMs) > config::RESTART_GRACE_MS) {
    Serial.println(F("[SYS] Restarting..."));
    ESP.restart();
  }
}

}  // namespace ota
}  // namespace network
