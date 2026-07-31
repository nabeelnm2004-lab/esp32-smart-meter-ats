/*
 * pzem_sensor.h — PZEM-004T v3.0 energy monitor on hardware UART2.
 *
 * Owns the Modbus transport and all energy accounting derived from
 * it. Reads block ~100ms each, so they run on Core 0 (pzemTask) and
 * are performed WITHOUT the state lock; only the shared-state update
 * afterwards is locked.
 */
#ifndef HARDWARE_PZEM_SENSOR_H
#define HARDWARE_PZEM_SENSOR_H

#include <Arduino.h>

namespace hardware {
namespace pzem {

// UART2 is opened by the PZEM004Tv30 constructor, so this only logs
// the configuration.
void begin();

// Sample all four registers and fold the result into live readings,
// per-meter usage and the daily/monthly statistics. Blocking; call
// only from the Core 0 sampling task.
void readEnergyData();

// Re-baseline per-meter accounting against the current reading, so
// the next meter starts from zero.
// Caller must hold the state lock.
void resetEnergyBaseline();

}  // namespace pzem
}  // namespace hardware

#endif  // HARDWARE_PZEM_SENSOR_H
