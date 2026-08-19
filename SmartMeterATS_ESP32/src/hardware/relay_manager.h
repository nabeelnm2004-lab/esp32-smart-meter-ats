/*
 * relay_manager.h — the ONLY module that drives relay GPIOs.
 *
 * Meter index in, GPIO out. No other module may call digitalWrite()
 * on a relay pin; per the architecture rules, network code in
 * particular must go through core::controller rather than reach in
 * here directly.
 *
 * Switching is non-blocking and break-before-make: all relays are
 * driven OFF and activeMeter is committed immediately, then the new
 * relay is energised RELAY_SWITCH_DELAY later by handlePendingSwitch().
 * That dead time prevents two meters ever being bridged.
 */
#ifndef HARDWARE_RELAY_MANAGER_H
#define HARDWARE_RELAY_MANAGER_H

#include <Arduino.h>

namespace hardware {
namespace relay {

// Configure all MAX_METERS relay outputs and drive them OFF, plus the
// emergency button input. Unpopulated positions stay OFF forever.
void begin();

// Schedule a break-before-make switch to meterIndex. If that meter is
// disabled the next enabled one is chosen instead. Out-of-range
// indices are ignored.
// Caller must hold the state lock.
void switchToMeter(int meterIndex);

// Complete a scheduled switch once the dead time has elapsed.
// Re-verifies the emergency/fault/test latches before energising, so
// a trip raised while the switch was in flight cancels it.
// Takes the state lock itself.
void handlePendingSwitch();

// Drive every relay position OFF and cancel any in-flight switch, so
// a pending relay-ON can never fire after an emergency or a trip.
// Caller must hold the state lock.
void allRelaysOff();

// Directly toggle one relay. ONLY valid in relay test mode; the
// controller enforces that.
// Caller must hold the state lock. Returns the new state.
bool toggle(int meterIndex);

// True if the relay for meterIndex is currently energised.
bool isOn(int meterIndex);

// Next/first enabled meter within the active window. Both fall back
// to the current index (or 0) when nothing else is enabled.
// Caller must hold the state lock.
int nextEnabledMeter(int current);
int firstEnabledMeter();

// Enabled meter with the most allowance left (skips meters that already
// reached their energy limit; energyLimit <= 0 is unlimited and always
// wins). Returns -1 when every meter is disabled or exhausted, so the
// caller can stay on the current meter instead of bouncing the load.
// Caller must hold the state lock.
int fullestAvailableMeter();

}  // namespace relay
}  // namespace hardware

#endif  // HARDWARE_RELAY_MANAGER_H
