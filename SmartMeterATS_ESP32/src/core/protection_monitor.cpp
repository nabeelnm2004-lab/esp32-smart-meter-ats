/*
 * protection_monitor.cpp — OV, UV, OC detection and auto-recovery.
 *
 * DETECTION (unchanged)
 * ----------------------
 * Fires immediately when a reading crosses the user-configured threshold.
 * Calls relay::allRelaysOff() and sets protTrip.
 *
 * AUTO-RECOVERY (improved)
 * -------------------------
 * After a trip the readings must stay CONTINUOUSLY below the recovery
 * threshold for the fault's stability window before the load is restored.
 * A single bad sample restarts the window.
 *
 * Recovery uses a small hysteresis dead-band relative to the detection
 * threshold (see HYSTERESIS section below). Detection is unaffected.
 *
 * PZEM OFFLINE DURING RECOVERY
 * ----------------------------
 * If the PZEM goes offline while a recovery window is open, the window
 * start time is reset when the PZEM reconnects, so the full stability
 * period is always served by real readings.
 *
 * NVS WRITES
 * ----------
 * forceSave() is called only when a trip fires or auto-clears — never
 * during the stability window or on normal sample-to-sample transitions.
 */
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

// ----------------------------------------------------------------
//  Recovery hysteresis dead-band.
//  Applied ONLY to the stability check; DETECTION is unchanged.
//
//  OV: recovery requires  V  <  (ovVoltThresh - OV_HYSTERESIS_V)
//  UV: recovery requires  V  >  (uvVoltThresh + UV_HYSTERESIS_V)
//  OC: recovery requires  I  <  (ocCurrThresh - OC_HYSTERESIS_A)
//
//  Without a dead-band, a line oscillating ±1 V around the trip
//  point would repeatedly open and cancel the window, extending
//  recovery indefinitely while staying just below the threshold.
// ----------------------------------------------------------------
constexpr float OV_HYSTERESIS_V = 2.0f;   // V  below OV threshold
constexpr float UV_HYSTERESIS_V = 2.0f;   // V  above UV threshold
constexpr float OC_HYSTERESIS_A = 0.5f;   // A  below OC threshold

// ----------------------------------------------------------------
//  PZEM reconnect tracking.
//  Initialised false so the reconnect branch fires correctly the
//  first time a frame arrives (PZEM starts "offline" at boot).
// ----------------------------------------------------------------
static bool prevPzemOK = false;

// ----------------------------------------------------------------
//  Internal helpers — all callers hold the state lock.
// ----------------------------------------------------------------

// Human-readable fault type name for event log entries.
const char* faultTypeName(uint8_t faultType) {
  switch (faultType) {
    case state::PROT_FAULT_OV: return "Over Voltage";
    case state::PROT_FAULT_UV: return "Under Voltage";
    case state::PROT_FAULT_OC: return "Over Current";
    default:                   return "Unknown";
  }
}

// The live measurement most relevant to the active fault type.
// Voltage for OV/UV, current for OC. Only actually-measured values
// are used — nothing is invented or extrapolated.
float currentFaultMeasurement(uint8_t faultType) {
  return (faultType == state::PROT_FAULT_OC)
             ? state::liveCurrent
             : state::liveVoltage;
}

// DETECTION threshold check — identical to the original behaviour.
// Fires at the raw user-configured threshold with no dead-band.
uint8_t currentAbnormal() {
  if (state::liveVoltage > state::ovVoltThresh) return state::PROT_FAULT_OV;
  if (state::liveVoltage > config::LINE_LIVE_VOLTAGE &&
      state::liveVoltage < state::uvVoltThresh)   return state::PROT_FAULT_UV;
  if (state::liveCurrent > state::ocCurrThresh)  return state::PROT_FAULT_OC;
  return state::PROT_FAULT_NONE;
}

// RECOVERY threshold check — applies the hysteresis dead-band so the
// load is only restored once the condition has clearly retreated from
// the trip point, not merely touched the exact threshold again.
// Returns true while the reading has NOT retreated far enough.
bool stillAbnormalForRecovery(uint8_t faultType) {
  switch (faultType) {
    case state::PROT_FAULT_OV:
      return state::liveVoltage >= (state::ovVoltThresh - OV_HYSTERESIS_V);
    case state::PROT_FAULT_UV:
      // Only active when the line is energised — a dead line is not UV.
      return state::liveVoltage > config::LINE_LIVE_VOLTAGE &&
             state::liveVoltage <= (state::uvVoltThresh + UV_HYSTERESIS_V);
    case state::PROT_FAULT_OC:
      return state::liveCurrent >= (state::ocCurrThresh - OC_HYSTERESIS_A);
    default:
      return false;
  }
}

// Per-fault stability window (ms). Caller holds the state lock.
unsigned long recoveryDelayFor(uint8_t faultType) {
  switch (faultType) {
    case state::PROT_FAULT_OV: return state::ovRecoveryMs;
    case state::PROT_FAULT_UV: return state::uvRecoveryMs;
    case state::PROT_FAULT_OC: return state::ocRecoveryMs;
    default:                   return state::ovRecoveryMs;
  }
}

}  // namespace

// check() is called from the Core 0 sampling task after every PZEM read.
void check() {
  bool tripped     = false;   // a NEW trip fired this pass  -> forceSave
  bool autoCleared = false;   // a recovery completed this pass -> forceSave

  STATE_LOCK();

  if (!state::pzemOK) {
    // No valid readings: hold any existing latch and freeze the recovery
    // window. Specifically, do NOT advance protRecoverStartMs — the window
    // must not expire on invalid data. When the PZEM reconnects (see below)
    // the window is restarted cleanly.
    prevPzemOK = false;
    STATE_UNLOCK();
    return;
  }

  // pzemOK is true here. Detect a PZEM reconnect: if this is the first
  // valid frame after an offline period AND a recovery window was open,
  // restart the window from now so the full stability period is served
  // exclusively by real readings, not by a mix of valid and zero samples.
  if (!prevPzemOK && state::protRecovering) {
    state::protRecoverStartMs = millis();
    Serial.println(F("[PROT] PZEM reconnected — stability window restarted"));
    // No event log entry: the window continues silently; it is neither
    // cancelled nor logged as a new event to avoid log noise.
  }
  prevPzemOK = true;

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

      // Use the hysteresis-aware check for the recovery phase.
      const bool abnormal = stillAbnormalForRecovery(state::protFaultType);

      if (abnormal) {
        // Condition still bad (or not yet retreated far enough past the
        // dead-band): cancel any in-flight window so it must restart.
        if (state::protRecovering) {
          state::protRecovering = false;
          // Include fault type and the offending measurement so the
          // operator can see exactly what cancelled the window.
          const uint8_t ft  = state::protFaultType;
          const float   mea = currentFaultMeasurement(ft);
          if (ft == state::PROT_FAULT_OC) {
            Serial.printf("[PROT] Recovery cancelled — %s (%.2fA)\n",
                          faultTypeName(ft), mea);
            eventlog::add("Recovery cancelled: %s (%.2fA)",
                          faultTypeName(ft), mea);
          } else {
            Serial.printf("[PROT] Recovery cancelled — %s (%.1fV)\n",
                          faultTypeName(ft), mea);
            eventlog::add("Recovery cancelled: %s (%.1fV)",
                          faultTypeName(ft), mea);
          }
        }

      } else if (!state::protRecovering) {
        // First reading that has retreated past the dead-band:
        // open the stability window.
        state::protRecovering     = true;
        state::protRecoverStartMs = millis();
        const unsigned long secs  = recoveryDelayFor(state::protFaultType) / 1000UL;
        const uint8_t ft          = state::protFaultType;
        const float   mea         = currentFaultMeasurement(ft);
        // Include fault type in the log so the entry is self-explanatory
        // when viewed in isolation, without needing the original trip entry.
        if (ft == state::PROT_FAULT_OC) {
          Serial.printf("[PROT] Recovery started: %s (%.2fA, %lus stable)\n",
                        faultTypeName(ft), mea, secs);
          eventlog::add("Recovery started: %s (%.2fA, %lus)",
                        faultTypeName(ft), mea, secs);
        } else {
          Serial.printf("[PROT] Recovery started: %s (%.1fV, %lus stable)\n",
                        faultTypeName(ft), mea, secs);
          eventlog::add("Recovery started: %s (%.1fV, %lus)",
                        faultTypeName(ft), mea, secs);
        }

      } else if ((millis() - state::protRecoverStartMs) >=
                 recoveryDelayFor(state::protFaultType)) {
        // Continuously normal for the whole stability window ->
        // auto-clear the trip and restore the active meter.
        //
        // Capture fault type and final measurement BEFORE clearing
        // protFaultType, so the log entry records the correct type.
        const uint8_t clearedFault = state::protFaultType;
        const float   finalMea     = currentFaultMeasurement(clearedFault);

        state::protTrip       = false;
        state::protReason     = "";
        state::protRecovering = false;
        state::protFaultType  = state::PROT_FAULT_NONE;

        // Re-energise via the same lock-held path the manual clear uses;
        // switchToMeter()/handlePendingSwitch() still re-check emergencyOff
        // and protTrip, so any other latch vetoes the restore.
        if (!state::emergencyOff) relay::switchToMeter(state::activeMeter);

        // Recovery log entry: fault type + final measurement at clearance.
        if (clearedFault == state::PROT_FAULT_OC) {
          Serial.printf("[PROT] Recovery: %s cleared (%.2fA) — operation restored\n",
                        faultTypeName(clearedFault), finalMea);
          eventlog::add("Recovery: %s cleared (%.2fA)",
                        faultTypeName(clearedFault), finalMea);
        } else {
          Serial.printf("[PROT] Recovery: %s cleared (%.1fV) — operation restored\n",
                        faultTypeName(clearedFault), finalMea);
          eventlog::add("Recovery: %s cleared (%.1fV)",
                        faultTypeName(clearedFault), finalMea);
        }
        autoCleared = true;
      }
    }
  }
  STATE_UNLOCK();

  // Critical events — persist immediately, but only AFTER releasing the
  // mutex: never hold the state lock across an NVS (flash) write.
  if (tripped || autoCleared) storage::settings::forceSave();
}

}  // namespace protection
}  // namespace core
