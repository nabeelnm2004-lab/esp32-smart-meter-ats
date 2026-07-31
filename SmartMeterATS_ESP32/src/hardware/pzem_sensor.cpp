#include "pzem_sensor.h"

#include <PZEM004Tv30.h>

#include "../core/config.h"
#include "../core/event_log.h"
#include "../core/system_state.h"
#include "../storage/settings_storage.h"

namespace hardware {
namespace pzem {

using namespace core;

namespace {

// The ESP32 constructor calls Serial2.begin() itself — no manual begin().
PZEM004Tv30 sensor(Serial2, config::PZEM_RX_PIN, config::PZEM_TX_PIN);

// The PZEM zeroes its energy register when it loses power, which would
// otherwise be booked as a huge negative delta. Estimate what was
// consumed during the outage from the last known power, re-baseline,
// and carry on. Caller holds the state lock.
void handleCounterReset(float reading) {
  Serial.printf("[PZEM] Counter reset: prev=%.3f, now=%.3f\n",
                state::prevLiveEnergy, reading);
  eventlog::add("PZEM reset: %.2f->%.2f kWh", state::prevLiveEnergy, reading);

  state::pzemWasReset = true;     // latched until the dashboard shows it
  state::pzemResetCount++;

  float gapHours = (millis() - state::lastEnergySampleMs) / 3600000.0f;
  if (gapHours < 0 || gapHours > config::PZEM_MAX_ESTIMATE_HOURS) gapHours = 0;
  const float estLost = (state::livePower / 1000.0f) * gapHours;   // kWh = kW * h

  Serial.printf("[PZEM] Estimated lost energy: %.3f kWh over %.2f h\n",
                estLost, gapHours);
  state::todayUsed        += estLost;
  state::currentMonthUsed += estLost;
  if (!state::emergencyOff) {
    state::meters[state::activeMeter].energyUsed += estLost;
  }
}

}  // namespace

void begin() {
  Serial.println(F("[PZEM] Hardware UART2 (GPIO16 RX / GPIO17 TX) ready"));
}

void resetEnergyBaseline() {
  state::pzemEnergyBase = state::liveEnergy;
}

void readEnergyData() {
  // Blocking Modbus reads — deliberately outside the lock.
  const float v = sensor.voltage();
  const float c = sensor.current();
  const float p = sensor.power();
  const float e = sensor.energy();

  const bool anyValid = !isnan(v) || !isnan(c) || !isnan(p) || !isnan(e);
  bool counterReset = false;

  STATE_LOCK();
  if (!isnan(v)) state::liveVoltage = v;
  if (!isnan(c)) state::liveCurrent = c;
  if (!isnan(p)) state::livePower   = p;

  if (!isnan(e)) {
    if (state::prevLiveEnergy >= 0.0f && e >= state::prevLiveEnergy) {
      const float statDelta = e - state::prevLiveEnergy;
      if (statDelta < config::PZEM_MAX_SANE_DELTA_KWH) {
        state::todayUsed        += statDelta;
        state::currentMonthUsed += statDelta;
      }
    } else if (state::prevLiveEnergy >= 0.0f && e < state::prevLiveEnergy) {
      // A drop below PZEM_RESET_RATIO of the previous reading means the
      // module reset; anything smaller is read jitter.
      if (e < state::prevLiveEnergy * config::PZEM_RESET_RATIO) {
        handleCounterReset(e);
        counterReset = true;
      }
      // Re-baseline on the lower counter so the active meter keeps
      // accumulating from here instead of jumping.
      state::pzemEnergyBase = e - state::meters[state::activeMeter].energyUsed;
    }

    state::prevLiveEnergy     = e;
    state::lastEnergySampleMs = millis();

    float delta = e - state::pzemEnergyBase;
    if (delta < 0) delta = 0;
    state::liveEnergy = e;
    if (!state::emergencyOff) {
      state::meters[state::activeMeter].energyUsed = delta;
    }
  }

  // All readings NaN means the PZEM is offline — don't report stale values.
  if (!anyValid) {
    state::liveVoltage = 0.0f;
    state::liveCurrent = 0.0f;
    state::livePower   = 0.0f;
  }
  state::pzemOK = anyValid;
  STATE_UNLOCK();

  // Outside the lock: never hold the mutex across a flash write.
  if (counterReset) storage::settings::markDirty();
}

}  // namespace pzem
}  // namespace hardware
