/*
 * ota_manager.h — firmware updates, over the network and from the
 * browser, plus the deferred restart both of them need.
 *
 * Update logic is kept in this one module so the rest of the firmware
 * never has to reason about a half-written flash partition.
 *
 * While an image is being written:
 *   - every relay is driven OFF, because the code that would manage
 *     them is about to be replaced;
 *   - the Core-0 sampler is paused, so no Modbus or mutex traffic can
 *     stall the transfer;
 *   - the watchdog is fed from the progress callback, since an upload
 *     easily outlasts the timeout;
 *   - loop() skips relay and NVS work while isUpdating() is true.
 *
 * An aborted update restores normal operation and re-energises the
 * active meter, unless an emergency or trip latch is still set.
 *
 * A restart is always deferred by RESTART_GRACE_MS so the HTTP
 * response reaches the browser before the connection dies.
 */
#ifndef NETWORK_OTA_MANAGER_H
#define NETWORK_OTA_MANAGER_H

#include <Arduino.h>
#include <WebServer.h>

namespace network {
namespace ota {

// Start the ArduinoOTA responder. Call once in setup(), after WiFi.
void begin();

// Service network OTA. Call every loop pass.
void handle();

// True while an image is being written and normal work must be skipped.
bool isUpdating();

// True once the ArduinoOTA responder is listening.
bool isReady();

// Streaming handler for the browser upload (POST /api/update).
// Registered by the web server as the route's upload callback.
void handleFirmwareUpload(WebServer& server);

// Final reply for the browser upload, once the body has streamed.
void finishFirmwareUpload(WebServer& server);

// Reboot after RESTART_GRACE_MS, letting the pending response flush.
void requestRestart();

// Perform a restart once the grace period has elapsed. Call every loop
// pass, and BEFORE the isUpdating() early return, or a successful
// browser update never reboots.
void handlePendingRestart();

}  // namespace ota
}  // namespace network

#endif  // NETWORK_OTA_MANAGER_H
