/*
 * meter_controller.h — the ONLY entry point for mutating relay state.
 *
 * architecture.md requires that network code never directly control
 * hardware. Every API handler therefore calls a request*() function
 * here instead of touching relays or latches; this module owns the
 * guards (emergency, protection trip, test mode, energy limit,
 * index range) so they can never be enforced inconsistently across
 * entry points.
 *
 * Each request returns a Result: ok plus a message suitable for the
 * JSON error field. Handlers translate that into an HTTP status —
 * they decide how to reply, never whether the action is allowed.
 *
 * All functions take the state lock themselves and perform any NVS
 * write after releasing it.
 */
#ifndef CORE_METER_CONTROLLER_H
#define CORE_METER_CONTROLLER_H

#include <Arduino.h>

namespace core {
namespace controller {

struct Result {
  bool   ok;
  String msg;   // empty on success
};

// Manual switch to meterIndex. Rejected while test mode or a
// protection trip is active, when the meter is disabled or out of
// range, or when it already reached its energy limit (unless bypass
// mode is on). Success clears the emergency latch and re-bases the
// per-meter energy baseline.
Result requestSwitch(int meterIndex);

// Latch emergency OFF and cut every relay. Always succeeds; persisted
// immediately since it must survive an unexpected reboot.
// The source string ("web", "button") is recorded in the event log.
Result requestEmergencyOff(const char* source);

// Clear a latched protection trip and restore the active meter unless
// the emergency latch is still set. Persisted immediately.
Result requestClearFault();

// Zero every per-meter energy counter and re-base the PZEM baseline.
Result requestResetEnergy();

// Set the active meter count (1..MAX_METERS). If the current active
// meter falls outside the new window, moves to the first enabled meter
// inside it.
Result requestSetActiveMeters(int count);

// Append one meter slot with factory defaults. outIndex/outCount
// receive the new index and count on success.
Result requestAddMeter(int& outIndex, int& outCount);

// Remove meter idx, shifting higher slots down. Metadata only: relay
// GPIO i stays bound to index i, so nothing physically moves. Refuses
// to remove the active meter or the last remaining one.
Result requestRemoveMeter(int idx, int& outCount);

// Suspend/resume automatic limit-based switching.
Result requestSetBypass(bool on);

// Enter/exit relay test mode. Entering drives everything OFF; exiting
// restores the active meter unless a latch is still set.
Result requestTestMode(bool on);

// Toggle one relay. Valid only inside test mode, and never past an
// emergency or trip latch. outState receives the new relay state.
Result requestTestToggle(int idx, bool& outState);

// Auto-exit test mode once TEST_MODE_TIMEOUT_MS of inactivity passes,
// so a forgotten browser tab cannot leave loads in a test state.
// Call every loop pass.
void handleTestModeTimeout();

// De-energise everything for an operation that must not run with loads
// connected — a firmware write, or wiping the configuration. Sets no
// latch, so it is not an emergency stop; use restoreOperation() to come
// back. Callers outside core use this instead of touching relays, which
// architecture.md forbids them from doing.
void enterSafeState();

// Return from enterSafeState() by re-energising the active meter,
// unless an emergency, a protection trip or test mode still holds the
// relays.
void restoreOperation();

// Ensure the active meter is valid — inside the active window and
// enabled — after settings were loaded from NVS or imported from a
// backup. Corrects it to the nearest enabled meter, or latches
// emergency OFF when every meter in the window is disabled. When
// `energize` is true and no latch or test mode holds the relays, the
// corrected meter is switched on immediately (config restore at
// runtime); when false only the selection is corrected, leaving the
// caller to energise once afterwards (boot). Returns true when it had
// to change the selection or latch, so the caller can persist.
// Takes the state lock itself; performs no NVS write of its own.
bool validateActiveMeter(bool energize);

}  // namespace controller
}  // namespace core

#endif  // CORE_METER_CONTROLLER_H
