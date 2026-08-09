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

// Starts "offline" so the status only ever becomes Connected after a real
// successful read, never merely because the radio/UART was initialised or a
// meter was configured. Consecutive failed cycles count up from here; a
// single good frame resets it to 0 and reconnects immediately.
uint8_t consecutiveFailures = config::PZEM_OFFLINE_FAILURES;

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

  // Validate power before estimating: if it's zero, negative, or non-finite,
  // estimation is meaningless. Use 0 to avoid corrupting usage totals with
  // bad data (undercounts, but safer than overcounting or adding garbage).
  float powerKW = state::livePower / 1000.0f;
  if (!isfinite(powerKW) || powerKW <= 0) powerKW = 0;

  float estLost = powerKW * gapHours;   // kWh = kW * h
  // Cap the estimate: a corrupted huge power value or miscalculated gap
  // should not blow up usage totals. Typical residential breaker: 100A @
  // 230V = 23 kW. Over the max estimate window that's a reasonable upper
  // bound; anything larger is almost certainly corrupt.
  const float maxReasonable = 30.0f * config::PZEM_MAX_ESTIMATE_HOURS;
  if (estLost > maxReasonable) {
    Serial.printf("[PZEM] WARNING: Capped excessive estimate %.3f -> %.3f kWh\n",
                  estLost, maxReasonable);
    estLost = maxReasonable;
  }

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
  //
  // The PZEM004Tv30 library caches values for UPDATE_TIME (200 ms): only
  // the FIRST getter in a cycle (voltage, below) actually issues a Modbus
  // read; current/power/energy then return that same frame's cached values.
  // So a finite voltage proves a real frame arrived, while a NAN voltage
  // means the read failed — and the other three getters would then return
  // STALE cached values (zeros at boot), which must NOT be treated as a
  // live device. Connectivity is therefore judged from voltage alone.
  const float v = sensor.voltage();
  const float c = sensor.current();
  const float p = sensor.power();
  const float e = sensor.energy();

  // A genuine response this cycle. isfinite() also rejects +/-Inf, which a
  // corrupted !isnan() check would accept as a reading.
  const bool commOK = isfinite(v);

  // Consecutive-failure counting: one bad frame is line noise, a sustained
  // run means the module is gone. A successful read resets the counter, so
  // reconnection happens automatically on the next good frame.
  if (commOK) {
    consecutiveFailures = 0;
  } else if (consecutiveFailures < config::PZEM_OFFLINE_FAILURES) {
    consecutiveFailures++;
  }
  const bool connected = consecutiveFailures < config::PZEM_OFFLINE_FAILURES;

  bool counterReset = false;

  STATE_LOCK();
  if (commOK) {
    // A real response: all four values come from the same fresh frame, so
    // they are applied together.
    state::liveVoltage = v;
    state::liveCurrent = c;
    state::livePower   = p;

    if (isfinite(e)) {
      if (state::prevLiveEnergy >= 0.0f && e >= state::prevLiveEnergy) {
        const float statDelta = e - state::prevLiveEnergy;
        if (statDelta < config::PZEM_MAX_SANE_DELTA_KWH) {
          state::todayUsed        += statDelta;
          state::currentMonthUsed += statDelta;
        } else {
          // Insane but finite spike: already rejected from the aggregate
          // totals above. Advance the baseline by the same delta so the
          // per-meter energyUsed (e - pzemEnergyBase, applied below) does
          // not absorb the spike either — otherwise a single glitched
          // reading corrupts per-meter usage and can trip a false limit.
          state::pzemEnergyBase += statDelta;
        }
      } else if (state::prevLiveEnergy >= 0.0f && e < state::prevLiveEnergy) {
        // A drop below PZEM_RESET_RATIO of the previous reading means the
        // module reset; anything smaller is read jitter.
        if (e < state::prevLiveEnergy * config::PZEM_RESET_RATIO) {
          handleCounterReset(e);
          counterReset = true;
        }
        // Re-baseline on the lower counter so the active meter keeps
        // accumulating from here instead of jumping. Clamp to zero: if the
        // meter's accumulated usage exceeds the new counter reading, the
        // baseline would go negative (conceptually wrong).
        float newBase = e - state::meters[state::activeMeter].energyUsed;
        if (newBase < 0) newBase = 0;
        state::pzemEnergyBase = newBase;
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
  } else {
    // No real response this cycle: clear the live readings so the dashboard
    // shows "--" instead of stale cached values.
    state::liveVoltage = 0.0f;
    state::liveCurrent = 0.0f;
    state::livePower   = 0.0f;
  }

  // pzemOK follows the sustained-health signal, not a single frame.
  state::pzemOK = connected;
  STATE_UNLOCK();

  // Outside the lock: never hold the mutex across a flash write.
  if (counterReset) storage::settings::markDirty();
}

}  // namespace pzem
}  // namespace hardware
