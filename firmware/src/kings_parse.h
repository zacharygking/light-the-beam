// Parse ESPN's team endpoint (GET .../teams/sac) into a GameState.
//
// The response is ~20-25 KB, mostly logo URLs. We hand ArduinoJson a filter document so only
// the dozen fields we need are kept, which keeps the parsed document under ~1 KB (the body
// itself is buffered by HTTPClient::getString in kings_api.cpp).
#pragma once
#include <ArduinoJson.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "game_state.h"

namespace kings {

inline void copyStr(char* dst, size_t n, const char* src) {
  if (!src) { dst[0] = 0; return; }
  strncpy(dst, src, n - 1);
  dst[n - 1] = 0;
}

// "2026-10-06T02:00Z" -> epoch seconds (UTC). Returns 0 on failure. Hand-rolled because
// strptime/timegm are not reliably available on the ESP32 Arduino core.
inline int64_t parseIsoUtc(const char* s) {
  if (!s || strlen(s) < 16) return 0;
  int Y, M, D, h, m;
  if (sscanf(s, "%4d-%2d-%2dT%2d:%2d", &Y, &M, &D, &h, &m) != 5) return 0;
  // days from civil (Howard Hinnant's algorithm)
  int y = Y - (M <= 2);
  int era = (y >= 0 ? y : y - 399) / 400;
  unsigned yoe = (unsigned)(y - era * 400);
  unsigned doy = (153 * (M + (M > 2 ? -3 : 9)) + 2) / 5 + D - 1;
  unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  int64_t days = (int64_t)era * 146097 + (int64_t)doe - 719468;
  return days * 86400 + h * 3600 + m * 60;
}

inline JsonDocument makeFilter() {
  JsonDocument f;
  JsonObject ev = f["team"]["nextEvent"][0].to<JsonObject>();
  ev["id"] = true;
  ev["date"] = true;
  ev["shortName"] = true;
  JsonObject comp = ev["competitions"][0].to<JsonObject>();
  JsonObject st = comp["status"].to<JsonObject>();
  st["displayClock"] = true;
  st["period"] = true;
  st["type"]["state"] = true;
  st["type"]["completed"] = true;
  st["type"]["shortDetail"] = true;
  JsonObject c = comp["competitors"][0].to<JsonObject>();
  c["homeAway"] = true;
  c["score"] = true;             // string in the team endpoint; object elsewhere
  c["winner"] = true;
  c["team"]["abbreviation"] = true;
  c["team"]["shortDisplayName"] = true;
  return f;
}

inline void fillSide(TeamSide& side, JsonObjectConst c) {
  copyStr(side.abbr, sizeof side.abbr, c["team"]["abbreviation"] | "");
  copyStr(side.name, sizeof side.name, c["team"]["shortDisplayName"] | "");
  side.home = strcmp(c["homeAway"] | "", "home") == 0;
  side.winner = c["winner"] | false;
  JsonVariantConst sc = c["score"];
  if (sc.isNull()) side.score = -1;
  else if (sc.is<const char*>()) side.score = atoi(sc.as<const char*>());
  else if (sc.is<JsonObjectConst>()) {   // core/scoreboard APIs: {"value":114.0,"displayValue":"114"}
    JsonVariantConst v = sc["value"];
    side.score = v.isNull() ? -1 : (int)v.as<double>();
  }
  else side.score = sc.as<int>();
}

// Input may be anything ArduinoJson accepts: Stream&, const char*, std::string, std::istream.
template <typename Input>
bool parseGameState(Input& input, GameState& out, const char* ourAbbr) {
  out = GameState{};
  JsonDocument filter = makeFilter();
  JsonDocument doc;
  // ESPN's document nests ~14 levels deep; ArduinoJson's default limit is 10.
  DeserializationError err = deserializeJson(doc, input, DeserializationOption::Filter(filter),
                                             DeserializationOption::NestingLimit(24));
  if (err) return false;
  // With a filter, ArduinoJson reports "Ok" even for non-JSON input (it just skips everything),
  // so an HTML error page would look like an empty document. Require the team object.
  if (!doc["team"].is<JsonObjectConst>()) return false;

  JsonArrayConst events = doc["team"]["nextEvent"].as<JsonArrayConst>();
  if (events.isNull() || events.size() == 0) {
    out.status = GameStatus::None;
    out.valid = true;   // valid response, just nothing scheduled
    return true;
  }
  JsonObjectConst ev = events[0];
  JsonObjectConst comp = ev["competitions"][0];
  JsonObjectConst st = comp["status"];

  if (ev["id"].is<const char*>()) copyStr(out.eventId, sizeof out.eventId, ev["id"].as<const char*>());
  else if (ev["id"].is<long long>()) snprintf(out.eventId, sizeof out.eventId, "%lld", ev["id"].as<long long>());
  copyStr(out.dateUtc, sizeof out.dateUtc, ev["date"] | "");
  out.tipoffEpoch = parseIsoUtc(out.dateUtc);
  copyStr(out.shortName, sizeof out.shortName, ev["shortName"] | "");
  copyStr(out.detail, sizeof out.detail, st["type"]["shortDetail"] | "");
  copyStr(out.clock, sizeof out.clock, st["displayClock"] | "");
  out.period = st["period"] | 0;
  out.completed = st["type"]["completed"] | false;

  // ESPN marks postponed/cancelled games as state "post" with completed=false and no scores.
  const char* state = st["type"]["state"] | "pre";
  if (strcmp(state, "in") == 0) out.status = GameStatus::Live;
  else if (strcmp(state, "post") == 0) out.status = out.completed ? GameStatus::Post : GameStatus::Postponed;
  else out.status = GameStatus::Pre;

  bool haveUs = false, haveThem = false;
  for (JsonObjectConst c : comp["competitors"].as<JsonArrayConst>()) {
    const char* abbr = c["team"]["abbreviation"] | "";
    if (!haveUs && strcasecmp(abbr, ourAbbr) == 0) { fillSide(out.us, c); haveUs = true; }
    else if (!haveThem) { fillSide(out.them, c); haveThem = true; }
  }
  out.valid = haveUs && haveThem;
  return out.valid;
}

}  // namespace kings
