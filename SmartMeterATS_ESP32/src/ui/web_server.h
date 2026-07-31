/*
 * web_server.h — the HTTP presentation layer.
 *
 * Owns the single WebServer instance, registers every route, and holds
 * the API handlers that translate an HTTP request into a call on the
 * application modules (core::controller, network::wifi, network::ota,
 * storage::settings, ...). Per architecture.md this layer never touches
 * relays or latches directly: mutating requests go through the
 * controller, and responses are formatted by utilities::json.
 *
 * The dashboard itself lives in dashboard.h and is streamed from
 * PROGMEM by the "/" route.
 *
 * All of this runs on Core 1 (the loop task); handleClient() must be
 * called every loop pass.
 */
#ifndef UI_WEB_SERVER_H
#define UI_WEB_SERVER_H

namespace ui {
namespace web {

// Register all routes and start listening on config::HTTP_PORT. Call
// once in setup(), after WiFi and OTA are up.
void begin();

// Service pending HTTP requests. Call every loop pass.
void handleClient();

}  // namespace web
}  // namespace ui

#endif  // UI_WEB_SERVER_H
