/*
 * event_log.h — circular in-RAM event log.
 *
 * RAM only: zero flash wear, and a reboot naturally shows up as a
 * fresh "Boot" entry, which makes unexplained power loss visible.
 * Entries survive until reboot.
 *
 * Called from BOTH cores (protection trips on Core 0, everything else
 * on Core 1), so the ring buffer is guarded by the state mutex.
 */
#ifndef CORE_EVENT_LOG_H
#define CORE_EVENT_LOG_H

#include <Arduino.h>

#include "config.h"

namespace core {
namespace eventlog {

struct EventEntry {
  uint32_t epoch;   // unix time when known, else 0
  uint32_t up;      // uptime seconds at the moment of the event
  char     msg[config::EVENT_MSG_LEN];
};

// printf-style. Messages are truncated to EVENT_MSG_LEN.
void add(const char* fmt, ...);

void clear();

// Snapshot for serialisation. Caller must hold the state lock for the
// whole read, since a concurrent add() would shift the ring.
uint8_t count();
// Oldest-first index i (0..count()-1) into the ring.
const EventEntry& at(uint8_t index);

// Uptime in seconds, from the 64-bit hardware timer so it never wraps
// the way millis() does after ~49 days.
uint32_t uptimeSeconds();

}  // namespace eventlog
}  // namespace core

#endif  // CORE_EVENT_LOG_H
