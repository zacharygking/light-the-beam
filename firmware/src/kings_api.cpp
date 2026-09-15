#include "kings_api.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "config.h"
#include "kings_parse.h"

namespace kings {

const char* fetchResultName(FetchResult r) {
  switch (r) {
    case FetchResult::Ok: return "ok";
    case FetchResult::NoWifi: return "no-wifi";
    case FetchResult::HttpError: return "http-error";
    case FetchResult::ParseError: return "parse-error";
  }
  return "?";
}

FetchResult fetch(GameState& out, int* httpCode) {
  if (WiFi.status() != WL_CONNECTED) return FetchResult::NoWifi;

  WiFiClientSecure client;
  // ESPN's cert chain rotates; pinning it would brick the display on a rotation. This is a
  // desk toy reading public scores, so we accept the risk of an unverified TLS peer.
  client.setInsecure();
  client.setTimeout(HTTP_TIMEOUT_MS / 1000);

  HTTPClient http;
  http.setReuse(false);
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.setUserAgent(ESPN_USER_AGENT);   // see config.h: browser-like UAs get a 403
  if (!http.begin(client, ESPN_URL)) return FetchResult::HttpError;
  http.addHeader("Accept", "application/json");

  int code = http.GET();
  if (httpCode) *httpCode = code;
  if (code != HTTP_CODE_OK) {
    log_w("ESPN GET -> %d", code);
    http.end();
    return FetchResult::HttpError;
  }

  // The body may be chunked; getString() de-chunks it. ~21 KB, fits comfortably in heap.
  String body = http.getString();
  http.end();

  GameState parsed;
  bool ok = parseGameState(body, parsed, TEAM_ABBR);
  body = String();   // release the buffer before returning
  if (!ok) {
    log_w("ESPN parse failed");
    return FetchResult::ParseError;
  }
  out = parsed;
  return FetchResult::Ok;
}

}  // namespace kings
