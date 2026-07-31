#include "switch_scheduler.h"

#include <Arduino.h>

#include "../core/system_state.h"
#include "../hardware/relay_manager.h"
#include "../storage/settings_storage.h"

namespace scheduler {
namespace autoswitch {

namespace state = core::state;
namespace relay = hardware::relay;
namespace nvs   = storage::settings;

namespace {

// Shared by the periodic check and the post-limit-change re-evaluation
// so both enforce exactly the same guards.
// Caller must hold the state lock. Returns true if a switch started.
bool switchIfLimitReached() {
  if (state::emergencyOff || state::protTrip || state::testMode ||
      state::bypassMode   || state::pendingMeter >= 0) {
    return false;
  }

  const state::MeterConfig& active = state::meters[state::activeMeter];
  if (active.energyLimit <= 0 || active.energyUsed < active.energyLimit) return false;

  Serial.printf("[SWITCH] Meter %d limit (%.2f kWh) reached\n",
                state::activeMeter + 1, active.energyLimit);

  const int next = relay::nextEnabledMeter(state::activeMeter);
  if (next == state::activeMeter) {
    Serial.println(F("[SWITCH] All meters exhausted or disabled"));
    return false;
  }

  // The next meter starts its allowance from the current reading.
  state::pzemEnergyBase = state::liveEnergy;
  relay::switchToMeter(next);
  return true;
}

}  // namespace

void handleRelayLogic() {
  STATE_LOCK();
  const bool switched = switchIfLimitReached();
  STATE_UNLOCK();

  // A limit-hit switch is routine, so the write is batched rather than
  // forced — never mark dirty while holding the lock.
  if (switched) nvs::markDirty();
}

void reevaluateAfterLimitChange() {
  switchIfLimitReached();
}

}  // namespace autoswitch
}  // namespace scheduler
