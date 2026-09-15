// Host-side tests for the ESPN parser. Fixtures are real API responses captured by
// tools/espn_probe.py and tools/make_fixtures.py.
//   pio test -e native
#include <unity.h>
#include <fstream>
#include <sstream>
#include <string>
#include "kings_parse.h"

static std::string loadFixture(const char* name) {
  std::string path = std::string(FIXTURE_DIR) + "/" + name + ".json";
  std::ifstream f(path, std::ios::binary);
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

static GameState parseFixture(const char* name) {
  std::string json = loadFixture(name);
  TEST_ASSERT_TRUE_MESSAGE(json.size() > 1000, "fixture missing or empty");
  GameState gs;
  bool ok = kings::parseGameState(json, gs, "SAC");
  TEST_ASSERT_TRUE_MESSAGE(ok, "parse failed");
  return gs;
}

void test_pre_game() {
  GameState gs = parseFixture("pre");
  TEST_ASSERT_EQUAL(GameStatus::Pre, gs.status);
  TEST_ASSERT_EQUAL_STRING("SAC", gs.us.abbr);
  TEST_ASSERT_EQUAL_STRING("LAL", gs.them.abbr);
  TEST_ASSERT_EQUAL_STRING("Lakers", gs.them.name);
  TEST_ASSERT_TRUE(gs.us.home);
  TEST_ASSERT_FALSE(gs.them.home);
  TEST_ASSERT_EQUAL(-1, gs.us.score);
  TEST_ASSERT_EQUAL_STRING("2026-10-06T02:00Z", gs.dateUtc);
  TEST_ASSERT_EQUAL_INT64(1791252000LL, gs.tipoffEpoch);   // 2026-10-06 02:00:00 UTC
  TEST_ASSERT_EQUAL_STRING("LAL @ SAC", gs.shortName);
  TEST_ASSERT_FALSE(gs.completed);
  TEST_ASSERT_FALSE(gs.weWon());
}

void test_live_game() {
  GameState gs = parseFixture("live");
  TEST_ASSERT_EQUAL(GameStatus::Live, gs.status);
  TEST_ASSERT_EQUAL(114, gs.us.score);
  TEST_ASSERT_EQUAL(108, gs.them.score);
  TEST_ASSERT_EQUAL_STRING("GS", gs.them.abbr);
  TEST_ASSERT_EQUAL(4, gs.period);
  TEST_ASSERT_EQUAL_STRING("2:31", gs.clock);
  TEST_ASSERT_FALSE(gs.completed);
}

void test_post_win() {
  GameState gs = parseFixture("post_win");
  TEST_ASSERT_EQUAL(GameStatus::Post, gs.status);
  TEST_ASSERT_TRUE(gs.completed);
  TEST_ASSERT_EQUAL(124, gs.us.score);
  TEST_ASSERT_EQUAL(118, gs.them.score);
  TEST_ASSERT_TRUE(gs.us.winner);
  TEST_ASSERT_FALSE(gs.them.winner);
  TEST_ASSERT_TRUE(gs.weWon());
}

void test_post_loss() {
  GameState gs = parseFixture("post_loss");
  TEST_ASSERT_EQUAL(GameStatus::Post, gs.status);
  TEST_ASSERT_EQUAL(109, gs.us.score);
  TEST_ASSERT_EQUAL(138, gs.them.score);
  TEST_ASSERT_EQUAL_STRING("LAC", gs.them.abbr);
  TEST_ASSERT_FALSE(gs.weWon());
}

void test_empty_next_event() {
  std::string json = "{\"team\":{\"abbreviation\":\"SAC\",\"nextEvent\":[]}}";
  GameState gs;
  TEST_ASSERT_TRUE(kings::parseGameState(json, gs, "SAC"));
  TEST_ASSERT_EQUAL(GameStatus::None, gs.status);
  TEST_ASSERT_TRUE(gs.valid);
}

void test_garbage() {
  std::string json = "<html>Access Denied</html>";
  GameState gs;
  TEST_ASSERT_FALSE(kings::parseGameState(json, gs, "SAC"));
  TEST_ASSERT_FALSE(gs.valid);
}

void test_iso_parse() {
  TEST_ASSERT_EQUAL_INT64(0LL, kings::parseIsoUtc("1970-01-01T00:00Z"));
  TEST_ASSERT_EQUAL_INT64(946684800LL, kings::parseIsoUtc("2000-01-01T00:00Z"));
  TEST_ASSERT_EQUAL_INT64(0LL, kings::parseIsoUtc("nope"));
}


// ---- edge cases from the pre-flash review ------------------------------------------

static std::string wrapEvent(const std::string& ev) {
  return "{\"team\":{\"abbreviation\":\"SAC\",\"nextEvent\":[" + ev + "]}}";
}
static const char* TWO_TEAMS =
  "\"competitors\":[{\"homeAway\":\"home\",\"team\":{\"abbreviation\":\"SAC\",\"shortDisplayName\":\"Kings\"}},"
  "{\"homeAway\":\"away\",\"team\":{\"abbreviation\":\"GS\",\"shortDisplayName\":\"Warriors\"}}]";

void test_postponed_is_not_final() {
  // ESPN: state "post", completed false, no scores, shortDetail "Postponed"
  std::string ev = "{\"id\":\"1\",\"date\":\"2026-11-02T03:00Z\",\"competitions\":[{" + std::string(TWO_TEAMS) +
    ",\"status\":{\"displayClock\":\"0.0\",\"period\":0,\"type\":{\"state\":\"post\",\"completed\":false,\"shortDetail\":\"Postponed\"}}}]}";
  std::string json = wrapEvent(ev);
  GameState gs;
  TEST_ASSERT_TRUE(kings::parseGameState(json, gs, "SAC"));
  TEST_ASSERT_EQUAL(GameStatus::Postponed, gs.status);
  TEST_ASSERT_FALSE(gs.weWon());
  TEST_ASSERT_EQUAL(-1, gs.us.score);
  TEST_ASSERT_EQUAL_STRING("Postponed", gs.detail);
}

void test_score_as_object() {
  std::string ev = "{\"id\":\"1\",\"date\":\"2026-11-02T03:00Z\",\"competitions\":[{"
    "\"competitors\":[{\"homeAway\":\"home\",\"score\":{\"value\":114.0,\"displayValue\":\"114\"},\"team\":{\"abbreviation\":\"SAC\"}},"
    "{\"homeAway\":\"away\",\"score\":{\"value\":108,\"displayValue\":\"108\"},\"team\":{\"abbreviation\":\"GS\"}}],"
    "\"status\":{\"period\":4,\"type\":{\"state\":\"in\",\"completed\":false}}}]}";
  std::string json = wrapEvent(ev);
  GameState gs;
  TEST_ASSERT_TRUE(kings::parseGameState(json, gs, "SAC"));
  TEST_ASSERT_EQUAL(114, gs.us.score);
  TEST_ASSERT_EQUAL(108, gs.them.score);
}

void test_score_numeric() {
  std::string ev = "{\"id\":\"1\",\"competitions\":[{"
    "\"competitors\":[{\"homeAway\":\"home\",\"score\":99,\"team\":{\"abbreviation\":\"SAC\"}},"
    "{\"homeAway\":\"away\",\"score\":\"101\",\"team\":{\"abbreviation\":\"GS\"}}],"
    "\"status\":{\"type\":{\"state\":\"in\"}}}]}";
  std::string json = wrapEvent(ev);
  GameState gs;
  TEST_ASSERT_TRUE(kings::parseGameState(json, gs, "SAC"));
  TEST_ASSERT_EQUAL(99, gs.us.score);
  TEST_ASSERT_EQUAL(101, gs.them.score);
}

void test_numeric_event_id() {
  std::string ev = "{\"id\":401898716,\"competitions\":[{" + std::string(TWO_TEAMS) + ",\"status\":{\"type\":{\"state\":\"pre\"}}}]}";
  std::string json = wrapEvent(ev);
  GameState gs;
  TEST_ASSERT_TRUE(kings::parseGameState(json, gs, "SAC"));
  TEST_ASSERT_EQUAL_STRING("401898716", gs.eventId);
}

void test_us_listed_second() {
  std::string ev = "{\"id\":\"1\",\"competitions\":[{"
    "\"competitors\":[{\"homeAway\":\"home\",\"team\":{\"abbreviation\":\"GS\"}},{\"homeAway\":\"away\",\"team\":{\"abbreviation\":\"SAC\"}}],"
    "\"status\":{\"type\":{\"state\":\"pre\"}}}]}";
  std::string json = wrapEvent(ev);
  GameState gs;
  TEST_ASSERT_TRUE(kings::parseGameState(json, gs, "SAC"));
  TEST_ASSERT_EQUAL_STRING("SAC", gs.us.abbr);
  TEST_ASSERT_EQUAL_STRING("GS", gs.them.abbr);
  TEST_ASSERT_FALSE(gs.weAreHome());
}

void test_competitor_problems() {
  const char* cases[] = {
    "{\"id\":\"1\",\"competitions\":[{\"status\":{\"type\":{\"state\":\"pre\"}}}]}",                                          // no competitors
    "{\"id\":\"1\",\"competitions\":[{\"competitors\":[{\"team\":{\"abbreviation\":\"SAC\"}}],\"status\":{\"type\":{\"state\":\"pre\"}}}]}",   // only us
    "{\"id\":\"1\",\"competitions\":[{\"competitors\":[{\"team\":{\"abbreviation\":\"GS\"}},{\"team\":{\"abbreviation\":\"LAL\"}}],\"status\":{\"type\":{\"state\":\"pre\"}}}]}", // neither is us
  };
  for (const char* c : cases) {
    std::string json = wrapEvent(c);
    GameState gs;
    TEST_ASSERT_FALSE(kings::parseGameState(json, gs, "SAC"));
    TEST_ASSERT_FALSE(gs.valid);
  }
}

void test_no_next_event_key_or_null() {
  for (const char* body : {"{\"team\":{\"abbreviation\":\"SAC\"}}", "{\"team\":{\"abbreviation\":\"SAC\",\"nextEvent\":null}}"}) {
    std::string json = body;
    GameState gs;
    TEST_ASSERT_TRUE(kings::parseGameState(json, gs, "SAC"));   // treated as "no game scheduled"
    TEST_ASSERT_EQUAL(GameStatus::None, gs.status);
    TEST_ASSERT_TRUE(gs.valid);
  }
}

void test_error_shapes_rejected() {
  for (const char* body : {"{\"code\":404,\"message\":\"not found\"}", "{\"team\":[]}", "", "null", "[]"}) {
    std::string json = body;
    GameState gs;
    TEST_ASSERT_FALSE_MESSAGE(kings::parseGameState(json, gs, "SAC"), body);
  }
}

void test_truncated_fixture() {
  std::string full = loadFixture("live");
  for (size_t cut : {(size_t)10000, full.size() * 6 / 10}) {
    std::string json = full.substr(0, cut);
    GameState gs;
    TEST_ASSERT_FALSE(kings::parseGameState(json, gs, "SAC"));
  }
}

void test_nesting_limit_regression() {
  // guards the NestingLimit(24): ESPN's document is deeper than ArduinoJson's default of 10
  std::string json = loadFixture("live");
  JsonDocument filter = kings::makeFilter();
  JsonDocument doc;
  DeserializationError e = deserializeJson(doc, json, DeserializationOption::Filter(filter));
  TEST_ASSERT_EQUAL(DeserializationError::TooDeep, e.code());
  e = deserializeJson(doc, json, DeserializationOption::Filter(filter), DeserializationOption::NestingLimit(24));
  TEST_ASSERT_EQUAL(DeserializationError::Ok, e.code());
}

void test_iso_variants() {
  TEST_ASSERT_EQUAL_INT64(1791252000LL, kings::parseIsoUtc("2026-10-06T02:00:30Z"));   // seconds ignored
  TEST_ASSERT_EQUAL_INT64(1709164800LL, kings::parseIsoUtc("2024-02-29T00:00Z"));      // leap day
  TEST_ASSERT_EQUAL_INT64(0LL, kings::parseIsoUtc("2026-10-06"));
  TEST_ASSERT_EQUAL_INT64(0LL, kings::parseIsoUtc(nullptr));
}

void test_long_strings_truncate() {
  std::string ev = "{\"id\":\"1\",\"competitions\":[{"
    "\"competitors\":[{\"homeAway\":\"home\",\"team\":{\"abbreviation\":\"SAC\",\"shortDisplayName\":\"ABCDEFGHIJKLMNOPQRSTUVWXYZ01234\"}},"
    "{\"homeAway\":\"away\",\"team\":{\"abbreviation\":\"GS\"}}],"
    "\"status\":{\"type\":{\"state\":\"pre\",\"shortDetail\":\"0123456789012345678901234567890123456789ABCDE\"}}}]}";
  std::string json = wrapEvent(ev);
  GameState gs;
  TEST_ASSERT_TRUE(kings::parseGameState(json, gs, "SAC"));
  TEST_ASSERT_EQUAL(23, strlen(gs.us.name));
  TEST_ASSERT_EQUAL(39, strlen(gs.detail));
}

void test_winner_without_post() {
  std::string ev = "{\"id\":\"1\",\"competitions\":[{"
    "\"competitors\":[{\"homeAway\":\"home\",\"winner\":true,\"score\":\"50\",\"team\":{\"abbreviation\":\"SAC\"}},"
    "{\"homeAway\":\"away\",\"score\":\"40\",\"team\":{\"abbreviation\":\"GS\"}}],"
    "\"status\":{\"period\":2,\"type\":{\"state\":\"in\",\"completed\":false}}}]}";
  std::string json = wrapEvent(ev);
  GameState gs;
  TEST_ASSERT_TRUE(kings::parseGameState(json, gs, "SAC"));
  TEST_ASSERT_EQUAL(GameStatus::Live, gs.status);
  TEST_ASSERT_FALSE(gs.weWon());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_pre_game);
  RUN_TEST(test_live_game);
  RUN_TEST(test_post_win);
  RUN_TEST(test_post_loss);
  RUN_TEST(test_empty_next_event);
  RUN_TEST(test_garbage);
  RUN_TEST(test_iso_parse);
  RUN_TEST(test_postponed_is_not_final);
  RUN_TEST(test_score_as_object);
  RUN_TEST(test_score_numeric);
  RUN_TEST(test_numeric_event_id);
  RUN_TEST(test_us_listed_second);
  RUN_TEST(test_competitor_problems);
  RUN_TEST(test_no_next_event_key_or_null);
  RUN_TEST(test_error_shapes_rejected);
  RUN_TEST(test_truncated_fixture);
  RUN_TEST(test_nesting_limit_regression);
  RUN_TEST(test_iso_variants);
  RUN_TEST(test_long_strings_truncate);
  RUN_TEST(test_winner_without_post);
  return UNITY_END();
}
