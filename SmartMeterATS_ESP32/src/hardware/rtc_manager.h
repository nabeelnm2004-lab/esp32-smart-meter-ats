/*
 * rtc_manager.h — DS3231 real-time clock on I2C.
 *
 * The chip is probed ONCE at boot and the verdict cached in
 * state::rtcOK, so no code path repeats rtc.begin() I2C churn.
 *
 * I2C is touched from Core 1 only. Every function here must be
 * called from the main loop context, never from the Core 0 sampling
 * task — that invariant is why Wire needs no mutex of its own.
 */
#ifndef HARDWARE_RTC_MANAGER_H
#define HARDWARE_RTC_MANAGER_H

#include <Arduino.h>
#include <RTClib.h>

namespace hardware {
namespace rtc {

// Start I2C and probe the DS3231. Sets state::rtcOK, state::rtcLostPower
// and seeds state::bootEpoch when a valid time is available.
void begin();

DateTime now();

// Set the chip's time. No-op when no RTC is present.
void adjust(const DateTime& t);

// Re-derive bootEpoch from a trusted epoch so event-log timestamps
// become wall-clock even with no RTC at all, and mark the daily
// tracking for re-initialisation on the new date.
// Takes the state lock itself.
void applyTimeSync(uint32_t epoch);

}  // namespace rtc
}  // namespace hardware

#endif  // HARDWARE_RTC_MANAGER_H
