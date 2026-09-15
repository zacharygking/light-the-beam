// Host-side tests for the beam policy and the poll scheduler (firmware/src/beam_policy.h).
//   pio test -e native
#include <unity.h>
#include <string.h>
#include "beam_policy.h"

static GameState st(GameStatus s, const char* id, bool won = false, int64_t tipoff = 1000000) {
  GameState g;
  g.status = s; strncpy(g.eventId, id, sizeof g.eventId - 1);
  g.completed = (s == GameStatus::Post);
  g.us.winner = won; g.valid = true; g.tipoffEpoch = tipoff;
  return g;
}
static const int HOLD = 12;
static const int64_t NOW = 1000000 + 3 * 3600;   // 3 h after tipoff

void test_pre_to_live_pulses() {
  BeamDecision d = beamPolicy(BeamMode::Off, st(GameStatus::Pre, "g1"), st(GameStatus::Live, "g1"), "", NOW, HOLD);
  TEST_ASSERT_EQUAL(BeamMode::Pulse, d.mode); TEST_ASSERT_FALSE(d.celebrate);
}
void test_live_keeps_manual_light() {
  BeamDecision d = beamPolicy(BeamMode::Hold, st(GameStatus::Pre, "g1"), st(GameStatus::Live, "g1"), "", NOW, HOLD);
  TEST_ASSERT_EQUAL(BeamMode::Hold, d.mode);
}
void test_win_rises_once() {
  BeamDecision d = beamPolicy(BeamMode::Pulse, st(GameStatus::Live, "g1"), st(GameStatus::Post, "g1", true), "", NOW, HOLD);
  TEST_ASSERT_EQUAL(BeamMode::Rise, d.mode); TEST_ASSERT_TRUE(d.celebrate);
  // re-poll of the same win while holding: nothing changes
  d = beamPolicy(BeamMode::Hold, st(GameStatus::Post, "g1", true), st(GameStatus::Post, "g1", true), "g1", NOW, HOLD);
  TEST_ASSERT_EQUAL(BeamMode::Hold, d.mode); TEST_ASSERT_FALSE(d.celebrate);
}
void test_tapped_off_stays_off_on_repoll() {
  BeamDecision d = beamPolicy(BeamMode::Off, st(GameStatus::Post, "g1", true), st(GameStatus::Post, "g1", true), "g1", NOW, HOLD);
  TEST_ASSERT_EQUAL(BeamMode::Off, d.mode); TEST_ASSERT_FALSE(d.celebrate);
}
void test_no_encore_after_reboot() {
  // reboot: current mode Off, prev state empty, but the persisted celebrated id matches
  BeamDecision d = beamPolicy(BeamMode::Off, GameState{}, st(GameStatus::Post, "g1", true), "g1", NOW, HOLD);
  TEST_ASSERT_EQUAL(BeamMode::Off, d.mode); TEST_ASSERT_FALSE(d.celebrate);
}
void test_stale_win_not_celebrated() {
  // win from 3 days ago still shown as nextEvent (all-star break): no celebration
  int64_t old = NOW - 3 * 86400;
  BeamDecision d = beamPolicy(BeamMode::Off, GameState{}, st(GameStatus::Post, "g9", true, old), "", NOW, HOLD);
  TEST_ASSERT_EQUAL(BeamMode::Off, d.mode); TEST_ASSERT_FALSE(d.celebrate);
  // but with no clock (now == 0) we trust it
  d = beamPolicy(BeamMode::Off, GameState{}, st(GameStatus::Post, "g9", true, old), "", 0, HOLD);
  TEST_ASSERT_TRUE(d.celebrate);
}
void test_new_pre_event_turns_off() {
  BeamDecision d = beamPolicy(BeamMode::Hold, st(GameStatus::Post, "g1", true), st(GameStatus::Pre, "g2"), "g1", NOW, HOLD);
  TEST_ASSERT_EQUAL(BeamMode::Off, d.mode);
  d = beamPolicy(BeamMode::Pulse, st(GameStatus::Live, "g1"), st(GameStatus::Pre, "g2"), "", NOW, HOLD);
  TEST_ASSERT_EQUAL(BeamMode::Off, d.mode);
}
void test_same_pre_event_keeps_manual_light() {
  BeamDecision d = beamPolicy(BeamMode::Hold, st(GameStatus::Pre, "g2"), st(GameStatus::Pre, "g2"), "", NOW, HOLD);
  TEST_ASSERT_EQUAL(BeamMode::Hold, d.mode);
}
void test_loss_from_pulse_off() {
  BeamDecision d = beamPolicy(BeamMode::Pulse, st(GameStatus::Live, "g1"), st(GameStatus::Post, "g1", false), "", NOW, HOLD);
  TEST_ASSERT_EQUAL(BeamMode::Off, d.mode); TEST_ASSERT_FALSE(d.celebrate);
}
void test_live_to_none_off() {
  BeamDecision d = beamPolicy(BeamMode::Pulse, st(GameStatus::Live, "g1"), st(GameStatus::None, ""), "", NOW, HOLD);
  TEST_ASSERT_EQUAL(BeamMode::Off, d.mode);
}
void test_postponed_no_rise() {
  GameState g = st(GameStatus::Postponed, "g1", true); g.completed = false;
  BeamDecision d = beamPolicy(BeamMode::Pulse, st(GameStatus::Live, "g1"), g, "", NOW, HOLD);
  TEST_ASSERT_EQUAL(BeamMode::Off, d.mode); TEST_ASSERT_FALSE(d.celebrate);
}

// ---- poll scheduler ----
static uint32_t iv(const GameState& g, int64_t now) { return pollIntervalMs(g, now, 600000, 30000, 20000, 300000, 1200); }
void test_poll_intervals() {
  TEST_ASSERT_EQUAL_UINT32(20000, iv(st(GameStatus::Live, "g"), NOW));
  TEST_ASSERT_EQUAL_UINT32(300000, iv(st(GameStatus::Post, "g"), NOW));
  TEST_ASSERT_EQUAL_UINT32(600000, iv(st(GameStatus::None, ""), NOW));
  TEST_ASSERT_EQUAL_UINT32(600000, iv(st(GameStatus::Postponed, "g"), NOW));
  GameState pre = st(GameStatus::Pre, "g", false, NOW + 2 * 3600);
  TEST_ASSERT_EQUAL_UINT32(600000, iv(pre, NOW));                       // 2 h out: slow
  pre.tipoffEpoch = NOW + 600;  TEST_ASSERT_EQUAL_UINT32(30000, iv(pre, NOW));   // 10 min out: fast
  pre.tipoffEpoch = NOW - 3600; TEST_ASSERT_EQUAL_UINT32(30000, iv(pre, NOW));   // 1 h past: still fast (ESPN lag)
  pre.tipoffEpoch = NOW - 4 * 3600; TEST_ASSERT_EQUAL_UINT32(600000, iv(pre, NOW)); // 4 h past: stale, slow
  pre.tipoffEpoch = NOW + 600;  TEST_ASSERT_EQUAL_UINT32(600000, iv(pre, 0));     // no clock: slow
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_pre_to_live_pulses);
  RUN_TEST(test_live_keeps_manual_light);
  RUN_TEST(test_win_rises_once);
  RUN_TEST(test_tapped_off_stays_off_on_repoll);
  RUN_TEST(test_no_encore_after_reboot);
  RUN_TEST(test_stale_win_not_celebrated);
  RUN_TEST(test_new_pre_event_turns_off);
  RUN_TEST(test_same_pre_event_keeps_manual_light);
  RUN_TEST(test_loss_from_pulse_off);
  RUN_TEST(test_live_to_none_off);
  RUN_TEST(test_postponed_no_rise);
  RUN_TEST(test_poll_intervals);
  return UNITY_END();
}
