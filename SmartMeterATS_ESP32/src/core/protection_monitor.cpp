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

namespace {

// Which OV/UV/OC condition, if any, is abnormal RIGHT NOW. Mirrors the
// trip tests below so detection and recovery use identical thresholds.
// Caller holds the state lock.
uint8_t currentAbnormal() {
  if (state::liveVoltage > state::ovVoltThresh) return state::PROT_FAULT_OV;
  if (state::liveVoltage > config::LINE_LIVE_VOLTAGE &&
      state::liveVoltage < state::uvVoltThresh)   return state::PROT_FAULT_UV;
  if (state::liveCurrent > state::ocCurrThresh)  return state::PROT_FAULT_OC;
  return state::PROT_FAULT_NONE;
}

// The stability window for the fault that actually tripped.
// Caller holds the state lock.
unsigned long recoveryDelayFor(uint8_t faultType) {
  switch (faultType) {
    case state::PROT_FAULT_OV: return state::ovRecoveryMs;
    case state::PROT_FAULT_UV: return state::uvRecoveryMs;
    case state::PROT_FAULT_OC: return state::ocRecoveryMs;
    default:                   return state::ovRecoveryMs;
  }
}

}  // namespace

void check() {
  bool tripped     = false;   // a NEW trip fired this pass  -> forceSave
  bool autoCleared = false;   // a recovery completed this pass -> forceSave

  STATE_LOCK();
  if (!state::pzemOK) {
    // No valid readings: can't judge stability, so hold whatever latch is
    // set and don't advance any recovery window. (Same early-out as before
    // for pzemOK; recovery simply waits for data to return.)
    STATE_UNLOCK();
    return;
  }

  if (!state::protTrip) {
    // -------- Detection (unchanged behaviour) --------
    const uint8_t fault = currentAbnormal();
    if (fault == state::PROT_FAULT_OV) {
      state::protReason = "Over Voltage: " + String(state::liveVoltage, 1) +
                          "V (limit " + String(state::ovVoltThresh, 0) + "V)";
    } else if (fault == state::PROT_FAULT_UV) {
      // Only when the line is actually energised — a dead line is not an
      // under-voltage fault.
      state::protReason = "Under Voltage: " + String(state::liveVoltage, 1) +
                          "V (limit " + String(state::uvVoltThresh, 0) + "V)";
    } else if (fault == state::PROT_FAULT_OC) {
      state::protReason = "Over Current: " + String(state::liveCurrent, 2) +
                          "A (limit " + String(state::ocCurrThresh, 1) + "A)";
    }

    if (fault != state::PROT_FAULT_NONE) {
      state::protTrip       = true;
      state::protFaultType  = fault;
      state::protRecovering = false;
      state::lastFaultEpoch = state::bootEpoch + eventlog::uptimeSeconds();
      relay::allRelaysOff();   // also cancels any pending switch on Core 1
      Serial.printf("[PROT] TRIP: %s\n", state::protReason.c_str());
      eventlog::add("Trip: %s", state::protReason.c_str());
      tripped = true;
    }
  } else {
    // -------- Auto-recovery for an active OV/UV/OC trip --------
    // Only OV/UV/OC set protFaultType; a manual latch (emergency) uses a
    // different flag and never reaches here with a recovery type set.
    if (state::protFaultType != state::PROT_FAULT_NONE) {
      const bool abnormal = (currentAbnormal() != state::PROT_FAULT_NONE);

      if (abnormal) {
        // Condition still bad (or a fault returned mid-window): cancel any
        // in-flight recovery so the stability window must start over.
        if (state::protRecovering) {
          state::protRecovering = false;
          Serial.println(F("[PROT] Recovery cancelled — fault returned"));
          eventlog::add("Recovery cancelled: fault returned");
        }
      } else if (!state::protRecovering) {
        // First normal sample: open the stability window.
        state::protRecovering    = true;
        state::protRecoverStartMs = millis();
        const unsigned long secs = recoveryDelayFor(state::protFaultType) / 1000UL;
        Serial.printf("[PROT] Recovery started (%lus stable required)\n", secs);
        eventlog::add("Recovery started (%lus)", secs);
      } else if ((millis() - state::protRecoverStartMs) >=
                 recoveryDelayFor(state::protFaultType)) {
        // Continuously normal for the whole window -> clear and restore once.
        state::protTrip       = false;
        state::protReason     = "";
        state::protRecovering = false;
        state::protFaultType  = state::PROT_FAULT_NONE;
        // Re-energise via the same lock-held path the manual clear uses;
        // switchToMeter()/handlePendingSwitch() still re-check emergencyOff
        // and protTrip, so any other latch vetoes the restore.
        if (!state::emergencyOff) relay::switchToMeter(state::activeMeter);
        Serial.println(F("[PROT] Fault auto-cleared — operation restored"));
        eventlog::add("Fault auto-cleared");
        autoCleared = true;
      }
    }
  }
  STATE_UNLOCK();

  // Critical events — persist immediately, but only after releasing the
  // mutex: never hold it across flash I/O.
  if (tripped || autoCleared) storage::settings::forceSave();
}

}  // namespace protection
}  // namespace core
