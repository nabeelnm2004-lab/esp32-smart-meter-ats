#include "json_response.h"

namespace utilities {
namespace json {

namespace {

constexpr const char* CONTENT_TYPE = "application/json";

}  // namespace

String escape(const String& raw) {
  String out;
  out.reserve(raw.length() + 8);
  for (size_t i = 0; i < raw.length(); i++) {
    const char c = raw[i];
    switch (c) {
      case '"':  out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n";  break;
      case '\r': out += "\\r";  break;
      case '\t': out += "\\t";  break;
      default:
        // Control characters are illegal raw inside a JSON string.
        if ((uint8_t)c < 0x20) {
          char esc[7];
          snprintf(esc, sizeof(esc), "\\u%04x", (unsigned)(uint8_t)c);
          out += esc;
        } else {
          out += c;
        }
    }
  }
  return out;
}

void sendCommonHeaders(WebServer& server) {
  server.sendHeader("Access-Control-Allow-Origin", "*");
}

void sendOk(WebServer& server) {
  sendCommonHeaders(server);
  server.send(200, CONTENT_TYPE, F("{\"status\":\"ok\"}"));
}

void sendOk(WebServer& server, const String& extraFields) {
  sendCommonHeaders(server);
  String body = F("{\"status\":\"ok\"");
  if (extraFields.length()) {
    body += ',';
    body += extraFields;
  }
  body += '}';
  server.send(200, CONTENT_TYPE, body);
}

void sendError(WebServer& server, int httpCode, const String& message) {
  sendCommonHeaders(server);
  String body = F("{\"status\":\"error\",\"msg\":\"");
  body += escape(message);
  body += F("\"}");
  server.send(httpCode, CONTENT_TYPE, body);
}

void sendJson(WebServer& server, const String& body) {
  sendCommonHeaders(server);
  server.send(200, CONTENT_TYPE, body);
}

}  // namespace json
}  // namespace utilities
