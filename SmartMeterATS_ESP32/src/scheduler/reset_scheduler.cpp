#include "reset_scheduler.h"

#include "../core/config.h"
#include "../core/event_log.h"
#include "../core/system_state.h"
#include "../hardware/relay_manager.h"
#include "../hardware/rtc_manager.h"
#include "../storage/settings_storage.h"

namespace scheduler {
namespace resets {

namespace config = core::config;
namespace state  = core::state;
namespace relay  = hardware::relay;
namespace nvs    = storage::settings;

void currentBillingPeriod(const DateTime& now, uint16_t& year, uint8_t& month) {
  int y = now.year();
  int m = now.month();
  if (now.day() < state::monthlyResetDay) {
    m -= 1;
    if (m < 1) { m = 12; y -= 1; }
  }
  year  = (uint16_t)y;
  month = (uint8_t)m;
}

void performMonthlyReset(const DateTime& now) {
  Serial.printf("[RESET] Monthly reset (day %d) — archiving %.3f kWh\n",
                state::monthlyResetDay, state::currentMonthUsed);
  state::lastMonthUsed    = state::currentMonthUsed;
  state::currentMonthUsed = 0.0f;
  core::eventlog::add("Monthly reset (day %d)", state::monthlyResetDay);

  for (int i = 0; i < config::MAX_METERS; i++) state::meters[i].energyUsed = 0.0f;
  state::pzemEnergyBase = state::liveEnergy;
  // A new billing period starts every meter with a fresh allowance, so
  // an emergency latch raised by the previous period's exhaustion is
  // no longer meaningful.
  state::emergencyOff = false;
  relay::switchToMeter(relay::firstEnabledMeter());

  uint16_t periodYear;
  uint8_t  periodMonth;
  currentBillingPeriod(now, periodYear, periodMonth);
  state::lastResetMonth = periodMonth;
  state::lastResetYear  = periodYear;
}

void applyMissedReset() {
  if (!state::rtcOK) return;

  const DateTime now = hardware::rtc::now();
  uint16_t periodYear;
  uint8_t  periodMonth;
  currentBillingPeriod(now, periodYear, periodMonth);

  if (state::lastResetMonth <= 0) {
    // First boot ever: adopt the current period as the baseline without
    // wiping counters.
    state::lastResetMonth = periodMonth;
    state::lastResetYear  = periodYear;
    nvs::save();
    return;
  }

  if (periodMonth != state::lastResetMonth || periodYear != state::lastResetYear) {
    Serial.println(F("[RESET] Missed monthly reset detected at boot — applying now"));
    performMonthlyReset(now);
    nvs::save();
  }
}

void handleTimeBasedResets() {
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck < config::RESET_CHECK_INTERVAL) return;
  lastCheck = millis();

  if (!state::rtcOK) return;
  const DateTime now = hardware::rtc::now();

  // First pass after boot or after a time sync: adopt the current date
  // as the tracking baseline so a sync does not look like a rollover.
  if (state::lastTrackedDay == -1) {
    state::lastTrackedDay   = now.day();
    state::lastTrackedMonth = now.month();
    state::lastDay          = now.day();
    return;
  }

  bool needSave = false;
  STATE_LOCK();

  // Day changed: archive yesterday into the ring and start today at zero.
  if (now.day() != state::lastTrackedDay) {
    state::dailyUsage[state::dailyIndex] = state::todayUsed;
    state::dailyIndex = (state::dailyIndex + 1) % config::DAILY_HISTORY_DAYS;
    state::todayUsed  = 0.0f;
    needSave = true;
  }

  uint16_t periodYear;
  uint8_t  periodMonth;
  currentBillingPeriod(now, periodYear, periodMonth);
  if (state::lastResetMonth <= 0) {
    state::lastResetMonth = periodMonth;
    state::lastResetYear  = periodYear;
    needSave = true;
  } else if (periodMonth != state::lastResetMonth ||
             periodYear  != state::lastResetYear) {
    performMonthlyReset(now);
    needSave = true;
  }

  // Updated on every pass, not only when the day changed, so the
  // tracking state can never go stale.
  state::lastTrackedDay   = now.day();
  state::lastTrackedMonth = now.month();
  state::lastDay          = now.day();
  STATE_UNLOCK();

  // Calendar rollovers are rare and define billing boundaries, so they
  // are written immediately rather than batched with routine edits.
  if (needSave) nvs::save();
}

}  // namespace resets
}  // namespace scheduler
