/*
 * switch_scheduler.h — automatic limit-based meter switching.
 *
 * Watches the active meter's usage against its energy limit and moves
 * to the next enabled meter once the limit is reached. This is the
 * only automatic switching policy in the firmware; manual switching
 * lives in core::controller.
 *
 * Suspended while bypass mode, relay test mode, an emergency latch or
 * a protection trip is active, and while a switch is already in
 * flight.
 */
#ifndef SCHEDULER_SWITCH_SCHEDULER_H
#define SCHEDULER_SWITCH_SCHEDULER_H

namespace scheduler {
namespace autoswitch {

// Evaluate the active meter against its limit and switch if needed.
// Takes the state lock itself. Call every loop pass.
void handleRelayLogic();

// Re-evaluate immediately after the limits change, so a newly lowered
// limit that is already exceeded switches now instead of waiting for
// the next cycle. Caller must hold the state lock.
void reevaluateAfterLimitChange();

}  // namespace autoswitch
}  // namespace scheduler

#endif  // SCHEDULER_SWITCH_SCHEDULER_H
