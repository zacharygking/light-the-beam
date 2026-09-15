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

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_pre_game);
  RUN_TEST(test_live_game);
  RUN_TEST(test_post_win);
  RUN_TEST(test_post_loss);
  RUN_TEST(test_empty_next_event);
  RUN_TEST(test_garbage);
  RUN_TEST(test_iso_parse);
  return UNITY_END();
}
