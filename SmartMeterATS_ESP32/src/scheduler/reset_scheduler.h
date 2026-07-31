/*
 * reset_scheduler.h — daily rollover and monthly billing reset.
 *
 * The monthly reset does NOT require the device to be awake on the
 * reset day. Each calendar date maps onto the billing period it
 * belongs to (honouring the configurable reset day); a reset fires
 * whenever the current period differs from the period recorded for
 * the last reset. Powering on two weeks late still triggers it, which
 * is what makes the behaviour survive an unexpected power loss.
 *
 * Reads the RTC, so every function here must run on Core 1.
 *
 * This module schedules; it never drives GPIO itself. Relay changes
 * go through hardware::relay, which the monthly reset uses to move
 * back to the first enabled meter.
 */
#ifndef SCHEDULER_RESET_SCHEDULER_H
#define SCHEDULER_RESET_SCHEDULER_H

#include <Arduino.h>
#include <RTClib.h>

namespace scheduler {
namespace resets {

// Map a date onto its billing period. Before the reset day the date
// still belongs to the PREVIOUS period — with a reset day of 5,
// Jan 4 belongs to December and Jan 5..Feb 4 belong to January.
void currentBillingPeriod(const DateTime& now, uint16_t& year, uint8_t& month);

// Archive the current month, zero every counter and return to the
// first enabled meter. Records the billing period so the same reset
// is not applied twice.
// Caller must hold the state lock, or be single-threaded at boot.
void performMonthlyReset(const DateTime& now);

// Catch a reset that was missed while the device was powered off, and
// adopt the current period as the baseline on a first-ever boot.
// Call once in setup(), after the RTC is probed and before the
// sampling task starts.
void applyMissedReset();

// Daily rollover and monthly reset check, rate-limited internally to
// one pass per RESET_CHECK_INTERVAL. Call every loop pass.
void handleTimeBasedResets();

}  // namespace resets
}  // namespace scheduler

#endif  // SCHEDULER_RESET_SCHEDULER_H
