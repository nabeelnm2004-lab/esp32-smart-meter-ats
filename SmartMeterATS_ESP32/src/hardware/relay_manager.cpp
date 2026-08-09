#include "relay_manager.h"

#include "../core/config.h"
#include "../core/event_log.h"
#include "../core/system_state.h"

namespace hardware {
namespace relay {

using namespace core;

void begin() {
  // Every position is configured, populated or not — an unpopulated
  // slot must still be a driven output rather than a floating pin.
  for (int i = 0; i < config::MAX_METERS; i++) {
    pinMode(config::RELAY_PINS[i], OUTPUT);
    digitalWrite(config::RELAY_PINS[i], config::RELAY_OFF);
  }
  // GPIO35 is input-only with no internal pull, so an external ~10k
  // pull-up to 3V3 holds it HIGH; a press pulls it LOW.
  pinMode(config::BTN_EMERGENCY, INPUT);
  Serial.printf("[PINS] %d relay outputs configured, all OFF\n", config::MAX_METERS);
}

void allRelaysOff() {
  state::pendingMeter = -1;
  for (int i = 0; i < config::MAX_METERS; i++) {
    digitalWrite(config::RELAY_PINS[i], config::RELAY_OFF);
  }
  Serial.println(F("[RELAY] All relays OFF"));
}

void switchToMeter(int meterIndex) {
  // Caller holds the state lock (see relay_manager.h). This function
  // must NOT take it again: it re-enters allRelaysOff() below, which
  // also assumes the lock is held, and re-acquiring a non-recursive
  // state mutex here would deadlock the caller.
  if (meterIndex < 0 || meterIndex >= state::activeMeterCount) {
    return;
  }
  if (!state::meters[meterIndex].enabled) {
    meterIndex = nextEnabledMeter(meterIndex);
  }

  allRelaysOff();

  // activeMeter is committed now so saves and per-meter accounting
  // already refer to the new target during the dead time.
  state::activeMeter   = meterIndex;
  state::pendingMeter  = meterIndex;
  state::switchOffTime = millis();

  Serial.printf("[RELAY] Switch to Meter %d scheduled (+%lums)\n",
                meterIndex + 1, config::RELAY_SWITCH_DELAY);
}

void handlePendingSwitch() {
  STATE_LOCK();
  if (state::pendingMeter >= 0 &&
      (millis() - state::switchOffTime) >= config::RELAY_SWITCH_DELAY) {
    // Both cores serialise on the state mutex, so once a protection
    // trip on Core 0 has run allRelaysOff(), no path can reach the
    // energise below: the pending switch is cancelled and these
    // guards reject any stale state.
    if (state::protTrip || state::emergencyOff || state::testMode ||
        state::pendingMeter >= state::activeMeterCount) {
      Serial.println(F("[RELAY] Pending switch cancelled (fault/emergency)"));
      state::pendingMeter = -1;
    } else {
      digitalWrite(config::RELAY_PINS[state::pendingMeter], config::RELAY_ON);
      Serial.printf("[RELAY] Meter %d ON\n", state::pendingMeter + 1);
      eventlog::add("Relay %d ON", state::pendingMeter + 1);
      state::pendingMeter = -1;
    }
  }
  STATE_UNLOCK();
}

bool toggle(int meterIndex) {
  const bool newState = !isOn(meterIndex);
  digitalWrite(config::RELAY_PINS[meterIndex],
               newState ? config::RELAY_ON : config::RELAY_OFF);
  return newState;
}

bool isOn(int meterIndex) {
  return digitalRead(config::RELAY_PINS[meterIndex]) == config::RELAY_ON;
}

int nextEnabledMeter(int current) {
  for (int i = 1; i <= state::activeMeterCount; i++) {
    const int candidate = (current + i) % state::activeMeterCount;
    if (state::meters[candidate].enabled) return candidate;
  }
  return current;   // nothing else enabled
}

int firstEnabledMeter() {
  for (int i = 0; i < state::activeMeterCount; i++) {
    if (state::meters[i].enabled) return i;
  }
  return 0;   // user disabled everything
}

}  // namespace relay
}  // namespace hardware
