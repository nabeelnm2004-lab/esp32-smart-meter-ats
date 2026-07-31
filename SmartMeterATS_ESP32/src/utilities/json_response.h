/*
 * json_response.h — one place that formats every REST reply.
 *
 * Without this each handler repeats the CORS header, the status
 * envelope and the HTTP code, which is exactly how those three drift
 * apart. Per docs/api_rules.md every response body is
 *
 *   success : {"status":"ok", ...optional fields}
 *   failure : {"status":"error","msg":"<human readable reason>"}
 *
 * HTTP codes used by the API:
 *   200  request accepted and applied
 *   400  malformed or out-of-range argument
 *   401  authentication required or wrong password
 *   409  well-formed but refused by current state (trip, test mode,
 *        limit reached, last meter)
 *   500  the device failed to carry the request out
 *
 * Messages are escaped here, so a caller may pass text derived from
 * user input (an SSID, an upload error) without breaking the JSON.
 */
#ifndef UTILITIES_JSON_RESPONSE_H
#define UTILITIES_JSON_RESPONSE_H

#include <Arduino.h>
#include <WebServer.h>

namespace utilities {
namespace json {

// Escape the characters that would otherwise terminate or corrupt a
// JSON string literal.
String escape(const String& raw);

// {"status":"ok"} with HTTP 200.
void sendOk(WebServer& server);

// {"status":"ok",<extraFields>} with HTTP 200. extraFields must be
// well-formed JSON member text without the surrounding braces, e.g.
// "\"idx\":2,\"meterCount\":3".
void sendOk(WebServer& server, const String& extraFields);

// {"status":"error","msg":"<message>"} with the given HTTP code.
void sendError(WebServer& server, int httpCode, const String& message);

// Send a pre-serialised JSON document with HTTP 200.
void sendJson(WebServer& server, const String& body);

// Attach the headers every response shares. Called by the senders
// above; exposed for handlers that must stream their own body.
void sendCommonHeaders(WebServer& server);

}  // namespace json
}  // namespace utilities

#endif  // UTILITIES_JSON_RESPONSE_H
