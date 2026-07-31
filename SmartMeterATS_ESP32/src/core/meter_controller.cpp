#include "meter_controller.h"

#include "../hardware/relay_manager.h"
#include "../storage/settings_storage.h"
#include "config.h"
#include "event_log.h"
#include "system_state.h"

namespace core {
namespace controller {

namespace relay = hardware::relay;
namespace nvs   = storage::settings;

namespace {

const Result OK_RESULT  = {true, ""};

Result fail(const char* msg) { return Result{false, msg}; }

void applyDefaults(state::MeterConfig& m) {
  m.energyLimit = config::ENERGY_LIMIT_DEFAULT;
  m.enabled     = true;
  m.energyUsed  = 0.0f;
}

}  // namespace

Result requestSwitch(int meterIndex) {
  const char* err  = "invalid meter";
  bool        done = false;

  STATE_LOCK();
  // Relays belong to the user while test mode is active.
  if (state::testMode) {
    err = "test mode active";
  } else if (state::protTrip) {
    err = "protection trip active";
  } else if (meterIndex >= 0 && meterIndex < state::activeMeterCount &&
             state::meters[meterIndex].enabled) {
    // Reject a meter that already reached its limit, unless bypassed.
    if (!state::bypassMode && state::meters[meterIndex].energyLimit > 0 &&
        state::meters[meterIndex].energyUsed >= state::meters[meterIndex].energyLimit) {
      err = "meter limit reached";
    } else {
      state::emergencyOff   = false;
      state::pzemEnergyBase = state::liveEnergy;
      relay::switchToMeter(meterIndex);
      done = true;
    }
  }
  STATE_UNLOCK();

  if (!done) return fail(err);
  nvs::markDirty();   // manual switch is routine, not critical
  return OK_RESULT;
}

Result requestEmergencyOff(const char* source) {
  STATE_LOCK();
  state::emergencyOff = true;
  relay::allRelaysOff();
  STATE_UNLOCK();

  Serial.printf("[EMERG] Emergency OFF (%s)\n", source);
  eventlog::add("Emergency OFF (%s)", source);
  nvs::forceSave();   // must survive an unexpected reboot
  return OK_RESULT;
}

Result requestClearFault() {
  bool cleared = false;

  STATE_LOCK();
  if (state::protTrip) {
    state::protTrip   = false;
    state::protReason = "";
    cleared           = true;
    if (!state::emergencyOff) relay::switchToMeter(state::activeMeter);
  }
  STATE_UNLOCK();

  if (cleared) {
    Serial.println(F("[PROT] Fault cleared"));
    eventlog::add("Fault cleared");
    nvs::forceSave();   // critical event
  }
  return OK_RESULT;
}

Result requestResetEnergy() {
  STATE_LOCK();
  for (int i = 0; i < config::MAX_METERS; i++) state::meters[i].energyUsed = 0.0f;
  state::pzemEnergyBase = state::liveEnergy;   // new baseline
  STATE_UNLOCK();

  Serial.println(F("[WEB] Energy reset"));
  nvs::markDirty();
  return OK_RESULT;
}

Result requestSetActiveMeters(int count) {
  if (count < 1 || count > config::MAX_METERS) return fail("n must be 1-10");

  STATE_LOCK();
  state::activeMeterCount = (uint8_t)count;
  // If the active meter fell outside the new window, move to the first
  // enabled meter inside it.
  if (state::activeMeter >= state::activeMeterCount) {
    relay::allRelaysOff();
    if (!state::emergencyOff && !state::protTrip) {
      state::pzemEnergyBase = state::liveEnergy;
      relay::switchToMeter(relay::firstEnabledMeter());
    } else {
      state::activeMeter = relay::firstEnabledMeter();
    }
  }
  STATE_UNLOCK();

  nvs::markDirty();
  Serial.printf("[CFG] Active Meters set to %d\n", count);
  return OK_RESULT;
}

Result requestAddMeter(int& outIndex, int& outCount) {
  bool done = false;

  STATE_LOCK();
  if (state::activeMeterCount < config::MAX_METERS) {
    outIndex = state::activeMeterCount;
    applyDefaults(state::meters[outIndex]);
    state::activeMeterCount++;
    outCount = state::activeMeterCount;
    done     = true;
  }
  STATE_UNLOCK();

  if (!done) return fail("Maximum 10 meters reached");
  nvs::markDirty();
  Serial.printf("[CFG] Meter %d added (now %d)\n", outIndex + 1, outCount);
  eventlog::add("Meter %d added", outIndex + 1);
  return OK_RESULT;
}

Result requestRemoveMeter(int idx, int& outCount) {
  const char* err  = "invalid meter index";
  bool        done = false;

  STATE_LOCK();
  if (idx < 0 || idx >= state::activeMeterCount) {
    err = "invalid meter index";
  } else if (idx == state::activeMeter) {
    err = "Cannot remove active meter";
  } else if (state::activeMeterCount <= 1) {
    err = "Minimum 1 meter required";
  } else {
    for (int i = idx; i < state::activeMeterCount - 1; i++) {
      state::meters[i] = state::meters[i + 1];
    }
    applyDefaults(state::meters[state::activeMeterCount - 1]);   // vacated top slot
    // Re-base indices so the same physical relay stays energised.
    if (state::activeMeter > idx) state::activeMeter--;
    // Cancel or re-base an in-flight switch that referenced a shifted index.
    if (state::pendingMeter > idx)       state::pendingMeter--;
    else if (state::pendingMeter == idx) state::pendingMeter = -1;
    state::activeMeterCount--;
    outCount = state::activeMeterCount;
    done     = true;
  }
  STATE_UNLOCK();

  if (!done) return fail(err);
  nvs::markDirty();
  Serial.printf("[CFG] Meter %d removed (now %d)\n", idx + 1, outCount);
  eventlog::add("Meter %d removed", idx + 1);
  return OK_RESULT;
}

Result requestSetBypass(bool on) {
  STATE_LOCK();
  state::bypassMode = on;
  STATE_UNLOCK();

  nvs::markDirty();
  Serial.printf("[CFG] Bypass mode %s\n", on ? "ENABLED" : "DISABLED");
  return OK_RESULT;
}

Result requestTestMode(bool on) {
  STATE_LOCK();
  if (on && !state::testMode) {
    state::testMode      = true;
    state::testModeStart = millis();
    relay::allRelaysOff();
    eventlog::add("Relay test mode ON");
  } else if (!on && state::testMode) {
    state::testMode = false;
    relay::allRelaysOff();
    if (!state::emergencyOff && !state::protTrip) {
      relay::switchToMeter(state::activeMeter);
    }
    eventlog::add("Relay test mode OFF");
  }
  STATE_UNLOCK();
  return OK_RESULT;
}

Result requestTestToggle(int idx, bool& outState) {
  const char* err  = "test mode not active";
  bool        done = false;

  STATE_LOCK();
  if (!state::testMode) {
    err = "test mode not active";
  } else if (state::emergencyOff || state::protTrip) {
    // Latches override test mode — never energise past one.
    err = state::emergencyOff ? "emergency active" : "protection trip active";
  } else if (idx < 0 || idx >= state::activeMeterCount) {
    err = "invalid relay index";
  } else {
    outState = relay::toggle(idx);
    state::testModeStart = millis();   // activity extends the timeout
    eventlog::add("Test: relay %d %s", idx + 1, outState ? "ON" : "OFF");
    done = true;
  }
  STATE_UNLOCK();

  return done ? OK_RESULT : fail(err);
}

void handleTestModeTimeout() {
  STATE_LOCK();
  if (state::testMode &&
      (millis() - state::testModeStart) > config::TEST_MODE_TIMEOUT_MS) {
    state::testMode = false;
    relay::allRelaysOff();
    if (!state::emergencyOff && !state::protTrip) {
      relay::switchToMeter(state::activeMeter);
    }
    eventlog::add("Test mode timeout — normal operation");
  }
  STATE_UNLOCK();
}

void enterSafeState() {
  state::StateLock lock;
  relay::allRelaysOff();
}

void restoreOperation() {
  state::StateLock lock;
  if (!state::emergencyOff && !state::protTrip && !state::testMode) {
    relay::switchToMeter(state::activeMeter);
  }
}

bool validateActiveMeter(bool energize) {
  bool changed = false;

  STATE_LOCK();
  if (state::activeMeter >= state::activeMeterCount ||
      !state::meters[state::activeMeter].enabled) {
    // Prefer the next enabled meter after the current one; fall back to
    // the first enabled meter when the index is past the window edge.
    const int corrected = (state::activeMeter < state::activeMeterCount)
                            ? relay::nextEnabledMeter(state::activeMeter)
                            : relay::firstEnabledMeter();
    if (!state::meters[corrected].enabled) {
      // Every meter in the window is disabled — there is nothing safe to
      // energise, so latch OFF rather than leave loads connected.
      state::activeMeter  = 0;
      state::emergencyOff = true;
      relay::allRelaysOff();
    } else if (energize && !state::emergencyOff && !state::protTrip &&
               !state::testMode) {
      state::pzemEnergyBase = state::liveEnergy;
      relay::switchToMeter(corrected);
    } else {
      // Boot path, or a latch holds the relays: correct the selection
      // only and let the caller energise once when it is safe.
      state::activeMeter = corrected;
    }
    changed = true;
  }
  STATE_UNLOCK();

  return changed;
}

}  // namespace controller
}  // namespace core
