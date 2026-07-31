#include "event_log.h"

#include <esp_timer.h>
#include <stdarg.h>

#include "system_state.h"

namespace core {
namespace eventlog {
namespace {

EventEntry buffer[config::EVENT_LOG_SIZE];
uint8_t    head  = 0;   // next write position
uint8_t    used  = 0;   // entries currently held

}  // namespace

uint32_t uptimeSeconds() {
  return (uint32_t)(esp_timer_get_time() / 1000000ULL);
}

void add(const char* fmt, ...) {
  char buf[config::EVENT_MSG_LEN];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);

  // The events endpoint concatenates msg into JSON directly. Strip the
  // two characters that would break that JSON here at the single sink,
  // so every caller — including user-controlled text like a WiFi SSID
  // — is safe without auditing each call site.
  for (char* q = buf; *q; q++) {
    if (*q == '"' || *q == '\\') *q = '\'';
  }

  const uint32_t up = uptimeSeconds();

  STATE_LOCK();
  EventEntry& e = buffer[head];
  e.epoch = state::bootEpoch ? state::bootEpoch + up : 0;
  e.up    = up;
  strncpy(e.msg, buf, sizeof(e.msg));
  e.msg[sizeof(e.msg) - 1] = '\0';
  head = (head + 1) % config::EVENT_LOG_SIZE;
  if (used < config::EVENT_LOG_SIZE) used++;
  STATE_UNLOCK();

  Serial.printf("[EVT] %s\n", buf);
}

void clear() {
  STATE_LOCK();
  head = 0;
  used = 0;
  STATE_UNLOCK();
}

uint8_t count() {
  return used;
}

const EventEntry& at(uint8_t index) {
  const uint8_t start =
    (uint8_t)((head - used + config::EVENT_LOG_SIZE) % config::EVENT_LOG_SIZE);
  return buffer[(start + index) % config::EVENT_LOG_SIZE];
}

}  // namespace eventlog
}  // namespace core
