#include "sampling_task.h"

#include <Arduino.h>
#include <esp_task_wdt.h>

#include "../hardware/pzem_sensor.h"
#include "config.h"
#include "protection_monitor.h"

namespace core {
namespace sampling {

namespace {

TaskHandle_t taskHandle = nullptr;
bool         paused     = false;

// Core 0: sample the meter, then evaluate protection against the fresh
// readings. Pacing comes from vTaskDelay(), never from millis() polling,
// so the core is genuinely idle between samples.
void samplingLoop(void* param) {
  (void)param;
  esp_task_wdt_add(nullptr);
  for (;;) {
    hardware::pzem::readEnergyData();   // blocking Modbus, no lock held
    protection::check();
    esp_task_wdt_reset();
    vTaskDelay(pdMS_TO_TICKS(config::PZEM_READ_INTERVAL));
  }
}

// The task watchdog panics and resets the chip if a subscribed task
// stops feeding it, which is what turns a hung loop into a recovery
// instead of a silent dead device.
void armWatchdog() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  // Core 3.x initialises the TWDT itself, so it is reconfigured here.
  esp_task_wdt_config_t wdtConfig = {
    .timeout_ms     = config::WDT_TIMEOUT_S * 1000,
    .idle_core_mask = 0,      // watch our tasks explicitly, not the idle ones
    .trigger_panic  = true,
  };
  esp_task_wdt_reconfigure(&wdtConfig);
#else
  esp_task_wdt_init(config::WDT_TIMEOUT_S, true);
#endif
  esp_task_wdt_add(nullptr);   // subscribe the calling (loop) task
  Serial.printf("[WDT] Task watchdog armed (%ds, panic+reset)\n",
                config::WDT_TIMEOUT_S);
}

}  // namespace

void begin() {
  armWatchdog();

  const BaseType_t created = xTaskCreatePinnedToCore(
      samplingLoop,
      "pzemTask",
      config::PZEM_TASK_STACK,
      nullptr,
      config::PZEM_TASK_PRIO,
      &taskHandle,
      config::PZEM_TASK_CORE);

  if (created != pdPASS) {
    // Without the sampler there are no readings and no protection, so
    // this is reported loudly rather than swallowed. The loop task
    // still runs, keeping the dashboard reachable to diagnose it.
    taskHandle = nullptr;
    Serial.println(F("[TASK] FAILED to start PZEM/protection task"));
    return;
  }
  Serial.println(F("[TASK] PZEM/protection task started on Core 0"));
}

void pause() {
  if (!taskHandle || paused) return;
  esp_task_wdt_delete(taskHandle);   // must precede suspend: see header
  vTaskSuspend(taskHandle);
  paused = true;
}

void resume() {
  if (!taskHandle || !paused) return;
  vTaskResume(taskHandle);
  esp_task_wdt_add(taskHandle);
  paused = false;
}

void feedWatchdog() {
  esp_task_wdt_reset();
}

}  // namespace sampling
}  // namespace core
