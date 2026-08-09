#include "rtc_manager.h"

#include <Wire.h>

#include "../core/config.h"
#include "../core/event_log.h"
#include "../core/system_state.h"

namespace hardware {
namespace rtc {

using namespace core;

namespace {
RTC_DS3231 chip;
}  // namespace

void begin() {
  Wire.begin(config::I2C_SDA_PIN, config::I2C_SCL_PIN);
  state::rtcOK = chip.begin();

  if (!state::rtcOK) {
    Serial.println(F("[RTC] DS3231 not found! Check wiring."));
    return;
  }

  Serial.println(F("[RTC] DS3231 OK"));
  // lostPower() true means the backup battery is dead or missing, so
  // the stored time is untrustworthy until the user syncs it.
  state::rtcLostPower = chip.lostPower();

  const DateTime t = chip.now();
  // A corrupt I2C read can return a structurally invalid date (e.g. month or
  // day 0, out-of-range year). Seeding bootEpoch from that poisons every
  // event-log timestamp until the next sync, so only trust a valid DateTime.
  // On a bad read, route through the existing untrustworthy-time path so the
  // stored time is treated as needing a sync, and leave bootEpoch alone.
  if (!t.isValid()) {
    state::rtcLostPower = true;
    Serial.println(F("[RTC] Invalid date read — awaiting time sync"));
    return;
  }

  state::bootEpoch = t.unixtime() - eventlog::uptimeSeconds();
  state::lastDay   = t.day();

  Serial.printf("[RTC] Date: %04d-%02d-%02d %02d:%02d:%02d\n",
                t.year(), t.month(), t.day(),
                t.hour(), t.minute(), t.second());
}

DateTime now() {
  return chip.now();
}

void adjust(const DateTime& t) {
  if (state::rtcOK) chip.adjust(t);
}

void applyTimeSync(uint32_t epoch) {
  STATE_LOCK();
  state::bootEpoch         = epoch - eventlog::uptimeSeconds();
  state::lastTimeSyncEpoch = epoch;
  state::rtcLostPower      = false;   // time is trustworthy again
  state::lastTrackedDay    = -1;      // re-init daily tracking on the new date
  STATE_UNLOCK();
}

}  // namespace rtc
}  // namespace hardware
