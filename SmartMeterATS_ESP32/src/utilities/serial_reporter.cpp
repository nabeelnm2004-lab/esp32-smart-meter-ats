#include "serial_reporter.h"

#include <Arduino.h>

#include "../core/config.h"
#include "../core/system_state.h"
#include "../hardware/relay_manager.h"

namespace utilities {
namespace serialreport {

namespace config = core::config;
namespace state  = core::state;
namespace relay  = hardware::relay;

void printStatus() {
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint < config::SERIAL_PRINT_INTERVAL) return;
  lastPrint = millis();

  // Snapshot under the lock, print outside it: Serial.printf() is slow
  // enough that holding the mutex across it would stall the sampler.
  float  voltage, current, power, energy;
  int    activeMeter, meterCount;
  bool   emergency, tripped, bypass;
  String reason;
  bool   relayOn[config::MAX_METERS];

  STATE_LOCK();
  voltage     = state::liveVoltage;
  current     = state::liveCurrent;
  power       = state::livePower;
  energy      = state::liveEnergy;
  activeMeter = state::activeMeter;
  meterCount  = state::activeMeterCount;
  emergency   = state::emergencyOff;
  tripped     = state::protTrip;
  bypass      = state::bypassMode;
  reason      = state::protReason;
  for (int i = 0; i < meterCount; i++) relayOn[i] = relay::isOn(i);
  STATE_UNLOCK();

  Serial.println(F("------ STATUS ------"));
  Serial.printf("Voltage  : %.1f V\n",   voltage);
  Serial.printf("Current  : %.2f A\n",   current);
  Serial.printf("Power    : %.1f W\n",   power);
  Serial.printf("Energy   : %.3f kWh\n", energy);
  Serial.printf("Meters   : %d active\n", meterCount);
  Serial.printf("Active   : Meter %d\n",  activeMeter + 1);
  for (int i = 0; i < meterCount; i++) {
    Serial.printf("Relay%-2d  : %s\n", i + 1, relayOn[i] ? "ON" : "OFF");
  }
  Serial.printf("Mode     : %s\n",
                emergency ? "EMERGENCY" : bypass ? "BYPASS" : "AUTO");
  if (tripped) Serial.printf("PROT     : TRIP - %s\n", reason.c_str());
  Serial.println(F("--------------------"));
}

}  // namespace serialreport
}  // namespace utilities
