/*
 * protection_monitor.h — over/under voltage and over current cut-out.
 *
 * Runs on Core 0 (the sampling task) immediately after each PZEM read.
 * A single PZEM monitors the common line, so protection is global: a
 * trip cuts ALL relay positions regardless of the active meter count.
 *
 * The trip is latched (state::protTrip) and only the dashboard can
 * clear it, via core::controller::requestClearFault().
 */
#ifndef CORE_PROTECTION_MONITOR_H
#define CORE_PROTECTION_MONITOR_H

namespace core {
namespace protection {

// Evaluate the live readings against the thresholds and trip if any is
// exceeded. Takes the state lock itself; the NVS write happens after
// the lock is released.
void check();

}  // namespace protection
}  // namespace core

#endif  // CORE_PROTECTION_MONITOR_H
