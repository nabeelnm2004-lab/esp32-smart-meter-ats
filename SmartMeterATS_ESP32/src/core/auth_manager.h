/*
 * auth_manager.h — HTTP role-based access control for the REST API.
 *
 * Two roles are recognised from HTTP Basic auth credentials:
 *
 *   ROLE_ADMIN  — full access. Authenticates with config::AUTH_USER and
 *                 the device secret (NVS "otapass", state::otaPassword),
 *                 the same credential the OTA path and the existing
 *                 dashboard login already use, so admin access is
 *                 unchanged and backward compatible.
 *   ROLE_VIEWER — read-only. Authenticates with config::AUTH_VIEWER_USER
 *                 and config::VIEWER_PASSWORD. Accepted on telemetry
 *                 endpoints, refused on every mutating route.
 *   ROLE_NONE   — missing or invalid credentials.
 *
 * The roles are ordered (NONE < VIEWER < ADMIN) so a single
 * requireRole(server, minimum) call expresses "this route needs at least
 * this role". Keeping every credential comparison here means the auth
 * boundary lives in exactly one auditable place.
 */
#ifndef CORE_AUTH_MANAGER_H
#define CORE_AUTH_MANAGER_H

#include <Arduino.h>
#include <WebServer.h>

namespace core {
namespace auth {

// Ordered so ROLE_NONE < ROLE_VIEWER < ROLE_ADMIN; requireRole() relies
// on this ordering.
enum Role : uint8_t {
  ROLE_NONE   = 0,   // no credentials, or they matched no account
  ROLE_VIEWER = 1,   // read-only telemetry endpoints
  ROLE_ADMIN  = 2,   // full access
};

// Resolve the caller's role from the request's Basic auth header.
// Returns ROLE_NONE when the credentials are absent or match neither
// account. Admin is checked first, so it wins if both somehow matched.
Role identify(WebServer& server);

// Ensure the caller holds at least `minimum`. Returns true on success.
// On failure sends a 401 error response and returns false, so a handler
// can early-out with:  if (!requireRole(server, ROLE_ADMIN)) return;
bool requireRole(WebServer& server, Role minimum);

}  // namespace auth
}  // namespace core

#endif  // CORE_AUTH_MANAGER_H
