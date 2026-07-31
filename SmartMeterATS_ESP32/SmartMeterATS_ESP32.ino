/*
 * SmartMeterATS_ESP32.ino — thin entry point for the Smart Meter ATS.
 *
 * All behaviour lives in the modules under src/. This sketch only
 * wires them together: setup() brings each subsystem up in dependency
 * order, and loop() drives the Core-1 work list. Core 0 runs the PZEM
 * sampling + protection task, started by core::sampling::begin().
 *
 * The firmware is universal — one binary serves 1..MAX_METERS meters;
 * only the persisted "Active Meters" count differs per installation.
 * GPIO numbers never leave src/core/config.cpp.
 *
 * Boot order matters:
 *   state  -> the mutex must exist before anything touches shared state
 *   relay  -> drive every relay OFF before we could ever energise one
 *   load   -> restore settings, then seed the NVS shadow
 *   rtc    -> probe the DS3231 once; seed bootEpoch for log timestamps
 *   resets -> apply a monthly reset missed while powered off
 *   wifi   -> radio, AP/STA and NTP
 *   ota    -> firmware update responder (needs the radio up)
 *   web    -> HTTP routes (needs WiFi + OTA to delegate to)
 *   boot   -> validate the active meter, then energise it
 *   sample -> arm the watchdog and start the Core-0 task LAST
 */
#include <Arduino.h>

#include "src/core/config.h"
#include "src/core/event_log.h"
#include "src/core/meter_controller.h"
#include "src/core/sampling_task.h"
#include "src/core/system_state.h"
#include "src/hardware/button_manager.h"
#include "src/hardware/relay_manager.h"
#include "src/hardware/rtc_manager.h"
#include "src/network/ota_manager.h"
#include "src/network/wifi_manager.h"
#include "src/scheduler/reset_scheduler.h"
#include "src/scheduler/switch_scheduler.h"
#include "src/storage/settings_storage.h"
#include "src/ui/web_server.h"
#include "src/utilities/serial_reporter.h"

namespace config     = core::config;
namespace state      = core::state;
namespace controller = core::controller;
namespace eventlog   = core::eventlog;
namespace sampling   = core::sampling;
namespace relay      = hardware::relay;
namespace buttons    = hardware::buttons;
namespace rtc        = hardware::rtc;
namespace ota        = network::ota;
namespace wifi       = network::wifi;
namespace resets     = scheduler::resets;
namespace autoswitch = scheduler::autoswitch;
namespace settings   = storage::settings;
namespace web        = ui::web;
namespace serialreport = utilities::serialreport;

// ============================================================
//  SETUP  (Core 1)
// ============================================================
void setup() {
  Serial.begin(115200);
  Serial.println(F("\n[BOOT] Smart Meter ATS System starting..."));

  // The recursive state mutex must exist before any module touches
  // shared state.
  state::begin();

  // Drive every relay OFF before anything else could energise one.
  relay::begin();

  // Restore persisted settings, then seed the RAM shadow so the first
  // deferred save writes only the keys that actually change.
  settings::load();
  settings::seedShadow();
  Serial.printf("[CFG] Active Meters: %d of %d supported\n",
    state::activeMeterCount, config::MAX_METERS);

  // Active Meter Validation on Boot: NVS may hold an activeMeter that
  // was later disabled or now sits outside the window. Correct the
  // selection (or latch emergency OFF if every meter is disabled)
  // WITHOUT energising yet — the initial switch happens below once the
  // RTC and reset checks are done. Persist any correction.
  if (controller::validateActiveMeter(false)) settings::markDirty();

  // Probe the DS3231 once and seed bootEpoch, then apply a monthly
  // reset that was missed while the device was powered off.
  rtc::begin();
  resets::applyMissedReset();

  Serial.println(F("[PZEM] Hardware UART2 (GPIO16 RX / GPIO17 TX) ready"));

  // Radio, then the OTA responder (needs the radio up), then the web
  // server (delegates to both).
  wifi::begin();
  ota::begin();
  web::begin();

  // Energise the validated active meter unless the emergency latch was
  // restored from NVS.
  if (!state::emergencyOff) {
    state::StateLock lock;
    relay::switchToMeter(state::activeMeter);
  } else {
    Serial.println(F("[BOOT] Emergency state restored from NVS — all relays stay OFF"));
  }

  // Log the boot with its reset reason — a fresh Boot entry after
  // unexplained downtime is power-loss evidence.
  esp_reset_reason_t rr = esp_reset_reason();
  eventlog::add("Boot (%s)",
    rr == ESP_RST_POWERON  ? "power-on"  :
    rr == ESP_RST_SW       ? "sw restart":
    rr == ESP_RST_PANIC    ? "panic"     :
    rr == ESP_RST_TASK_WDT ? "watchdog"  :
    rr == ESP_RST_BROWNOUT ? "brownout"  : "other");
  if (state::emergencyOff) eventlog::add("Emergency OFF restored from NVS");

  // Arm the loop-task watchdog and start the Core-0 sampling task LAST,
  // once all shared state it reads is initialised.
  sampling::begin();

  Serial.println(F("[BOOT] System ready."));
}

// ============================================================
//  MAIN LOOP  (Core 1)
// ============================================================
void loop() {
  sampling::feedWatchdog();     // feed the task watchdog every pass

  ota::handle();                // network OTA — non-blocking poll
  web::handleClient();          // serve web requests
  ota::handlePendingRestart();  // deferred reboot — MUST run before the
                                // isUpdating() early-return, or a web
                                // OTA never actually restarts
  if (ota::isUpdating()) return;  // skip relay/NVS work while flashing

  controller::handleTestModeTimeout();  // auto-exit relay test mode
  buttons::handleButtons();             // physical emergency button
  relay::handlePendingSwitch();         // finish a break-before-make switch

  // While test mode is active the scheduler must not touch relays —
  // the user owns them via /api/testRelay.
  if (!state::testMode) autoswitch::handleRelayLogic();

  resets::handleTimeBasedResets();  // daily rollover & monthly reset
  wifi::handleStation();            // STA connect / fallback / NTP adopt
  settings::handleDeferredSave();   // throttled NVS flush
  serialreport::printStatus();      // periodic debug output
}
