/*
 * sampling_task.h — owns the Core-0 sampling task.
 *
 * A PZEM Modbus read blocks for roughly 100 ms per register. Running
 * that on Core 0 keeps the web server on Core 1 responsive, which is
 * the whole reason the firmware is dual-core.
 *
 * The task handle lives here rather than in the sketch so that no
 * other module has to know FreeRTOS or watchdog internals. The OTA
 * path in particular must stop sampling while flash is being written,
 * and does so through pause()/resume() instead of touching the task.
 *
 * A suspended task cannot feed the task watchdog, so pause()
 * unsubscribes it from the TWDT first and resume() re-subscribes it.
 * Suspending without that ordering panics the device mid-flash.
 */
#ifndef CORE_SAMPLING_TASK_H
#define CORE_SAMPLING_TASK_H

namespace core {
namespace sampling {

// Arm the task watchdog on the calling (loop) task, then start the
// sampling task pinned to Core 0. Call once at the end of setup().
void begin();

// Suspend sampling. Safe to call when the task never started.
void pause();

// Resume sampling after pause(). Safe to call when not paused.
void resume();

// Feed the task watchdog from the loop task. Call every loop pass.
void feedWatchdog();

}  // namespace sampling
}  // namespace core

#endif  // CORE_SAMPLING_TASK_H
