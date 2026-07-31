/*
 * system_state.h — shared runtime state and the mutex that guards it.
 *
 * DUAL-CORE CONCURRENCY
 * ---------------------
 * pzemTask (Core 0) samples the PZEM and evaluates protection.
 * loop()   (Core 1) runs the web server, buttons, relay logic, resets.
 *
 * Every field below is touched by both cores, so every access must
 * hold the state lock. The mutex is recursive so a function that
 * locks (switchToMeter, allRelaysOff) can be called from a section
 * that already holds it.
 *
 * Keep critical sections short. NEVER hold the lock across a PZEM
 * Modbus read or an NVS write — both block for ~100ms and would
 * stall the other core.
 *
 * I2C (the DS3231) is only ever touched from Core 1, so Wire needs
 * no lock of its own.
 */
#ifndef CORE_SYSTEM_STATE_H
#define CORE_SYSTEM_STATE_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "config.h"

namespace core {
namespace state {

struct MeterConfig {
  float energyLimit;   // kWh ceiling before an automatic switch
  bool  enabled;       // user may exclude a meter from the rotation
  float energyUsed;    // kWh accumulated on this meter since reset
};

// ------------------------------------------------------------
//  Mutex
// ------------------------------------------------------------
extern SemaphoreHandle_t stateMux;

void begin();   // create the mutex; call before anything touches state

#define STATE_LOCK()   xSemaphoreTakeRecursive(::core::state::stateMux, portMAX_DELAY)
#define STATE_UNLOCK() xSemaphoreGiveRecursive(::core::state::stateMux)

// RAII guard for the state mutex. Prefer this over the macros in new
// code: it cannot leak a lock through an early return.
class StateLock {
 public:
  StateLock()  { STATE_LOCK(); }
  ~StateLock() { STATE_UNLOCK(); }
  StateLock(const StateLock&) = delete;
  StateLock& operator=(const StateLock&) = delete;
};

// ------------------------------------------------------------
//  Meter configuration and selection
// ------------------------------------------------------------
extern MeterConfig meters[config::MAX_METERS];
// Runtime meter count (1..MAX_METERS), persisted. Relay logic, JSON,
// scheduling and restore all loop over this — never over a literal.
extern uint8_t activeMeterCount;
extern int     activeMeter;      // 0-indexed

// Non-blocking switch state machine:
//   pendingMeter == -1 : idle
//   pendingMeter >=  0 : relays are OFF, waiting RELAY_SWITCH_DELAY
//                        before energising that meter.
extern int           pendingMeter;
extern unsigned long switchOffTime;

// ------------------------------------------------------------
//  Operating mode latches
// ------------------------------------------------------------
extern bool emergencyOff;   // all relays OFF; persisted, survives reboot
extern bool bypassMode;     // automatic limit switching suspended; manual still works
extern bool testMode;       // relays driven only by /api/testRelay
extern unsigned long testModeStart;

// ------------------------------------------------------------
//  Live PZEM readings
// ------------------------------------------------------------
extern float liveVoltage;
extern float liveCurrent;
extern float livePower;
extern float liveEnergy;      // kWh reported by the PZEM
extern float pzemEnergyBase;  // baseline for per-meter accounting
extern bool  pzemOK;          // true only while the PZEM returns valid data

// PZEM counter-reset tracking: the module zeroes its energy register
// when it loses power, which would otherwise look like a huge
// negative delta.
extern unsigned long lastEnergySampleMs;
extern bool          pzemWasReset;    // latched until the dashboard shows it
extern uint16_t      pzemResetCount;  // resets since boot
extern float         prevLiveEnergy;  // -1 = not yet initialised

// ------------------------------------------------------------
//  Statistics
// ------------------------------------------------------------
extern float todayUsed;
extern float currentMonthUsed;
extern float lastMonthUsed;
extern float dailyUsage[config::DAILY_HISTORY_DAYS];
extern int   dailyIndex;

// Billing period tracking. lastResetMonth <= 0 means "never reset";
// the (year, month) tuple lets a reset missed while powered off be
// detected and applied at the next boot.
extern int lastResetMonth;
extern int lastResetYear;
extern int monthlyResetDay;   // 1..28
extern int lastTrackedDay;
extern int lastTrackedMonth;
extern int lastDay;

// ------------------------------------------------------------
//  Protection
// ------------------------------------------------------------
extern float  ovVoltThresh;
extern float  uvVoltThresh;
extern float  ocCurrThresh;
extern bool   protTrip;      // latched until cleared from the dashboard
extern String protReason;    // human-readable trip cause

// ------------------------------------------------------------
//  Time
//  bootEpoch lets the event log timestamp entries without touching
//  I2C, since logEvent() is called from both cores but the RTC is
//  Core 1 only: epoch = bootEpoch + uptime.
// ------------------------------------------------------------
extern bool     rtcOK;
extern bool     rtcLostPower;      // DS3231 backup battery dead/missing
extern uint32_t bootEpoch;
extern uint32_t lastTimeSyncEpoch;

// ------------------------------------------------------------
//  Network settings
//  Persisted here rather than inside the network layer so that
//  storage never has to depend on network.
// ------------------------------------------------------------
extern uint8_t wifiMode;      // config::WifiMode
extern String  staSsid;
extern String  staPass;
extern String  otaPassword;   // also the dashboard Basic-auth password

// ------------------------------------------------------------
//  Deferred actions
// ------------------------------------------------------------
extern bool          restartPending;
extern unsigned long restartAtMs;
extern bool          otaInProgress;

}  // namespace state
}  // namespace core

#endif  // CORE_SYSTEM_STATE_H
