/*
 * auth_manager.cpp — see auth_manager.h.
 *
 * Role resolution uses WebServer::authenticate(), which performs the
 * HTTP Basic (and digest, if offered) credential check against a given
 * user/password pair. We test the admin pair first, then the viewer
 * pair, and map the first match to a role.
 */
#include "auth_manager.h"

#include "config.h"
#include "system_state.h"
#include "../utilities/json_response.h"

namespace core {
namespace auth {

namespace config = core::config;
namespace state  = core::state;
namespace json   = utilities::json;

Role identify(WebServer& server) {
  // Admin first so it wins if the viewer secret were ever set equal to
  // the admin one. The admin password is the live device secret, so it
  // is read from state, not a compile-time constant.
  if (server.authenticate(config::AUTH_USER, state::otaPassword.c_str())) {
    return ROLE_ADMIN;
  }
  if (server.authenticate(config::AUTH_VIEWER_USER, state::viewerPassword.c_str())) {
    return ROLE_VIEWER;
  }
  return ROLE_NONE;
}

bool requireRole(WebServer& server, Role minimum) {
  if (identify(server) >= minimum) return true;
  json::sendError(server, 401, F("auth required"));
  return false;
}

}  // namespace auth
}  // namespace core
