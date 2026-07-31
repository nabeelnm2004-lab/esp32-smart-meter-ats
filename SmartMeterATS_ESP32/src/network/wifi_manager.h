/*
 * wifi_manager.h — radio, mDNS and NTP supervision.
 *
 * Three modes are supported: AP only, station only, and both. The
 * access point is kept alive until a station link is actually proven,
 * even in station-only mode, so a wrong password can never take the
 * dashboard away from an installer standing in front of the unit.
 *
 * Everything is non-blocking. begin() starts an association and
 * returns; handleStation() polls the result, falls back to AP after
 * STA_CONNECT_TIMEOUT_MS and retries a link that drops later.
 *
 * NTP is only adopted when the DS3231 is missing or its battery is
 * dead. A healthy RTC stays the authority, and when NTP does apply it
 * repairs the chip so the reset scheduler reads correct time too.
 *
 * This module owns no hardware beyond the radio and never touches
 * relays.
 */
#ifndef NETWORK_WIFI_MANAGER_H
#define NETWORK_WIFI_MANAGER_H

#include <Arduino.h>

namespace network {
namespace wifi {

// Bring the radio up for the persisted mode and begin a station
// association when one is configured.
void begin();

// Poll the station link, apply a deferred configuration change and
// adopt an NTP sync when one becomes available. Call every loop pass.
void handleStation();

// True once the station link is up.
bool isStationConnected();

// Reason the last station attempt failed, empty while none has.
const String& lastError();

// True once an NTP sync has been adopted.
bool hasNtpSync();

// Address the dashboard is reachable on: the station IP when linked,
// otherwise the access point IP.
String currentIp();

// Persist and apply a new mode and credentials. The radio is
// reconfigured WIFI_APPLY_DELAY_MS later so the HTTP response that
// triggered this reaches the client first.
// Returns false with reason set when the arguments are unusable.
bool applyConfiguration(uint8_t mode, const String& ssid, const String& pass,
                        String& reason);

// Start an immediate association attempt, keeping the access point up
// so the dashboard stays reachable while it runs.
// Returns false with reason set when the arguments are unusable.
bool connectNow(uint8_t mode, const String& ssid, const String& pass,
                String& reason);

// Serialise the visible networks as the "networks" array documented in
// docs/api_rules.md. Blocking: a scan takes a couple of seconds.
String scanNetworksJson();

}  // namespace wifi
}  // namespace network

#endif  // NETWORK_WIFI_MANAGER_H
