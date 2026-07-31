/*
 * settings_storage.h — persistence in NVS via Preferences.
 *
 * FLASH WEAR PROTECTION
 * ---------------------
 * Routine changes call markDirty(); handleDeferredSave() flushes only
 * once the change burst has settled (debounce) AND a minimum interval
 * has passed since the last real write (throttle), so several quick
 * dashboard edits collapse into one write. A starvation guard forces
 * the flush if changes never stop arriving.
 *
 * Critical events — emergency stop, protection trip, fault clear,
 * WiFi config, config restore — call forceSave() instead, which is an
 * explicitly requested immediate write.
 *
 * On top of that, save() keeps a RAM shadow of everything last
 * written and writes ONLY the keys that actually changed. If nothing
 * changed the namespace is not even opened: zero wear.
 *
 * All MAX_METERS slots are always persisted, so settings survive a
 * later increase of the Active Meters count.
 *
 * NVS keys (namespace "smartats")
 *   mcount       uchar   active meter count (1..MAX_METERS)
 *   lim0..lim9   float   per-meter kWh limits
 *   en0..en9     bool    per-meter enabled flags
 *   active       uchar   active meter index
 *   daily        bytes   float[30] daily usage ring buffer
 *   didx         uchar   ring write index
 *   today        float   today kWh
 *   curmon       float   current month kWh
 *   lastmon      float   last month kWh
 *   ov, uv, oc   float   protection thresholds
 *   emerg        bool    latched emergency-off state
 *   bypass       bool    bypass mode flag
 *   lmon         uchar   month of the last monthly reset
 *   lyear        ushort  year of the last monthly reset
 *   rday         uchar   monthly reset day (1..28)
 *   wmode        uchar   WiFi mode (0=AP, 1=STA, 2=AP+STA)
 *   wssid        string  home WiFi SSID
 *   wpass        string  home WiFi password
 *   otapass      string  OTA / dashboard password
 */
#ifndef STORAGE_SETTINGS_STORAGE_H
#define STORAGE_SETTINGS_STORAGE_H

namespace storage {
namespace settings {

// Populate state from NVS, falling back to validated defaults for any
// key that is absent or out of range. Call once in setup() before the
// sampling task starts.
void load();

// Snapshot state under the lock, then write only changed keys.
void save();

// Seed the RAM shadow from the live values right after load(), so the
// first save writes only what changed since boot.
void seedShadow();

// Mark settings dirty for a batched, debounced flush.
void markDirty();

// Immediate write, bypassing debounce and throttle. For critical state
// that must survive an imminent reboot or power loss.
void forceSave();

// Flush if the debounce and throttle conditions are met. Call every
// loop pass.
void handleDeferredSave();

// Erase the whole namespace. The next boot loads defaults.
void factoryReset();

}  // namespace settings
}  // namespace storage

#endif  // STORAGE_SETTINGS_STORAGE_H
