#include "protection_monitor.h"

#include <Arduino.h>

#include "../hardware/relay_manager.h"
#include "../storage/settings_storage.h"
#include "config.h"
#include "event_log.h"
#include "system_state.h"

namespace core {
namespace protection {

namespace relay = hardware::relay;

void check() {
  bool tripped = false;

  STATE_LOCK();
  if (!state::pzemOK || state::protTrip) {
    STATE_UNLOCK();
    return;
  }

  if (state::liveVoltage > state::ovVoltThresh) {
    state::protReason = "Over Voltage: " + String(state::liveVoltage, 1) +
                        "V (limit " + String(state::ovVoltThresh, 0) + "V)";
    tripped = true;
  } else if (state::liveVoltage > config::LINE_LIVE_VOLTAGE &&
             state::liveVoltage < state::uvVoltThresh) {
    // Only when the line is actually energised — a dead line is not an
    // under-voltage fault.
    state::protReason = "Under Voltage: " + String(state::liveVoltage, 1) +
                        "V (limit " + String(state::uvVoltThresh, 0) + "V)";
    tripped = true;
  } else if (state::liveCurrent > state::ocCurrThresh) {
    state::protReason = "Over Current: " + String(state::liveCurrent, 2) +
                        "A (limit " + String(state::ocCurrThresh, 1) + "A)";
    tripped = true;
  }

  if (tripped) {
    state::protTrip = true;
    relay::allRelaysOff();   // also cancels any pending switch on Core 1
    Serial.printf("[PROT] TRIP: %s\n", state::protReason.c_str());
    eventlog::add("Trip: %s", state::protReason.c_str());
  }
  STATE_UNLOCK();

  // Critical event — persist immediately, but only after releasing the
  // mutex: never hold it across flash I/O.
  if (tripped) storage::settings::forceSave();
}

}  // namespace protection
}  // namespace core
