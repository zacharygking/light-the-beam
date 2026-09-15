// Host-side test bench for the live win-probability model (firmware/src/winprob.h).
//   pio test -e native -f test_winprob
// 1. Parity: the ESP32 code reproduces the Python trainer's probability on 300 sampled plays.
// 2. Properties: symmetry, monotonicity, sensible end points.
// 3. Replay: every play of the held-out 2025-26 season through the firmware function, scored
//    against the outcome and against ESPN's own in-game win probability.
#include <unity.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "winprob.h"
#include "../fixtures/winprob_cases.h"

using namespace kings;

// ---- 1. parity with Python -----------------------------------------------------------------
void test_parity_with_trainer() {
  const int n = sizeof(WINPROB_CASES) / sizeof(WINPROB_CASES[0]);
  TEST_ASSERT_TRUE(n >= 200);
  float worst = 0.0f;
  for (int i = 0; i < n; ++i) {
    const WinProbCase& c = WINPROB_CASES[i];
    float p = winProbHome(c.margin, fracLeft(c.period, c.secsLeft));
    float d = fabsf(p - c.p);
    if (d > worst) worst = d;
  }
  char msg[64];
  snprintf(msg, sizeof msg, "worst |C++ - Python| = %.2e", worst);
  TEST_ASSERT_TRUE_MESSAGE(worst < 2e-4f, msg);
}

// ---- 2. properties ------------------------------------------------------------------------
void test_clock_parsing() {
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 290.0f, clockSeconds("4:50"));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 3.7f, clockSeconds("0:03.7"));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 57.4f, clockSeconds("57.4"));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 720.0f, clockSeconds("12:00"));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, clockSeconds(""));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, clockSeconds(nullptr));
}
void test_frac_left() {
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, fracLeft(1, 720.0f));
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.5f, fracLeft(3, 720.0f));   // start of Q3
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, fracLeft(4, 0.0f));
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 300.0f / 2880.0f, fracLeft(5, 300.0f));  // OT clock
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, fracLeft(0, 0.0f));     // pregame
}
void test_symmetric_in_margin_at_tip() {
  // home court is the only asymmetry: p(+m) + p(-m) should be near 1 up to home advantage
  float pre = winProbHome(0, 1.0f);
  TEST_ASSERT_TRUE(pre > 0.50f && pre < 0.62f);   // home court prior
  float up = winProbHome(10, 0.5f), down = winProbHome(-10, 0.5f);
  TEST_ASSERT_TRUE(up > 0.5f && down < 0.5f);
}
void test_monotone_in_margin_and_time() {
  float prev = 0.0f;
  for (int m = -30; m <= 30; ++m) {
    float p = winProbHome(m, 0.25f);
    TEST_ASSERT_TRUE(p >= prev);
    prev = p;
  }
  // a 10-point lead is worth more the less time remains
  TEST_ASSERT_TRUE(winProbHome(10, 0.05f) > winProbHome(10, 0.5f));
  TEST_ASSERT_TRUE(winProbHome(10, 0.5f) > winProbHome(10, 1.0f));
}
void test_end_points() {
  // Up 1 with 0.0 on the clock: the sqrt(t + eps) smoothing keeps this short of certainty
  // (a buzzer foul can still send someone to the line). The pill is not shown once the game
  // is Final, so this is a display of confidence, not a verdict.
  TEST_ASSERT_TRUE(winProbHome(1, 0.0f) > 0.75f);
  TEST_ASSERT_TRUE(winProbHome(-1, 0.0f) < 0.25f);
  TEST_ASSERT_TRUE(winProbHome(3, 0.0f) > 0.97f);      // up 3 at the horn
  TEST_ASSERT_TRUE(winProbHome(25, 0.25f) > 0.98f);   // up 25 entering Q4
  float tie = winProbHome(0, 0.0f);
  TEST_ASSERT_TRUE(tie > 0.4f && tie < 0.65f);         // tied at the buzzer: overtime, near even
}
void test_game_state_wrapper() {
  GameState g;
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, -1.0f, winProbUs(g));   // no score yet
  g.us.score = 100; g.them.score = 95; g.period = 4; strcpy(g.clock, "2:00");
  g.us.home = true;  float home = winProbUs(g);
  g.us.home = false; float away = winProbUs(g);
  TEST_ASSERT_TRUE(home > 0.8f && away > 0.8f);
  TEST_ASSERT_TRUE(home > away);                        // same lead, home court helps a little
  // exact complement: us away up 5 == home team down 5
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f - winProbHome(-5, fracLeft(4, 120.0f)), away);
}

// ---- 3. replay of held-out games ----------------------------------------------------------
struct Acc { double brier = 0, ll = 0; long n = 0, correct = 0; };
static void add(Acc& a, double p, int y) {
  double q = p < 1e-6 ? 1e-6 : p > 1 - 1e-6 ? 1 - 1e-6 : p;
  a.brier += (p - y) * (p - y);
  a.ll -= y ? log(q) : log(1 - q);
  a.correct += ((p >= 0.5) == (y == 1));
  a.n++;
}
void test_replay_held_out_games() {
  // 5 bytes per play, little-endian: int8 home margin; uint16 frac*10000 with bit 15 = home won;
  // uint16 ESPN wp*1000 (0xFFFF = missing) with bit 14 = first play of a game.
  FILE* f = fopen(FIXTURE_DIR "/winprob_eval.bin", "rb");
  TEST_ASSERT_NOT_NULL_MESSAGE(f, "missing test/fixtures/winprob_eval.bin (run tools/train_winprob.py)");
  Acc ours, espn, prior, oursQ4, espnQ4;
  const double homePrior = winProbHome(0, 1.0f);
  double binP[10] = {0}, binY[10] = {0}; long binN[10] = {0};
  long games = 0;
  unsigned char row[5];
  while (fread(row, 1, 5, f) == 5) {
    int margin = (signed char)row[0];
    unsigned fbits = row[1] | (row[2] << 8), ebits = row[3] | (row[4] << 8);
    int won = (fbits & 0x8000) ? 1 : 0;
    float frac = (fbits & 0x7FFF) / 10000.0f;
    bool hasEspn = ebits != 0xFFFF;
    if (hasEspn && (ebits & 0x4000)) games++;
    double wp = hasEspn ? (ebits & 0x3FFF) / 1000.0 : 0.0;
    double p = winProbHome(margin, frac);
    add(ours, p, won);
    add(prior, homePrior, won);
    if (hasEspn) add(espn, wp, won);
    if (frac < 0.25f) { add(oursQ4, p, won); if (hasEspn) add(espnQ4, wp, won); }
    int b = (int)(p * 10); if (b > 9) b = 9;
    binP[b] += p; binY[b] += won; binN[b]++;
  }
  fclose(f);
  printf("\n  replay: %ld games, %ld plays\n", games, ours.n);
  printf("  this model  Brier %.4f  log loss %.4f  accuracy %.3f\n", ours.brier / ours.n, ours.ll / ours.n, (double)ours.correct / ours.n);
  printf("  ESPN        Brier %.4f  log loss %.4f  accuracy %.3f\n", espn.brier / espn.n, espn.ll / espn.n, (double)espn.correct / espn.n);
  printf("  home prior  Brier %.4f  log loss %.4f  (no score, no clock)\n", prior.brier / prior.n, prior.ll / prior.n);
  printf("  4th quarter Brier: this model %.4f  ESPN %.4f\n", oursQ4.brier / oursQ4.n, espnQ4.brier / espnQ4.n);
  printf("  calibration (bin: plays, predicted, observed)\n");
  double worstGap = 0;
  for (int b = 0; b < 10; ++b) {
    if (!binN[b]) continue;
    double mp = binP[b] / binN[b], ob = binY[b] / binN[b];
    printf("    %.1f-%.1f: %6ld  %.3f  %.3f\n", b / 10.0, (b + 1) / 10.0, binN[b], mp, ob);
    if (binN[b] >= 5000 && fabs(mp - ob) > worstGap) worstGap = fabs(mp - ob);
  }
  TEST_ASSERT_TRUE(games >= 1000 && ours.n > 500000);
  // ESPN also knows team strength and possession; the score/clock model concedes early-game
  // Brier but must match ESPN once the game is decided by what is on the scoreboard.
  TEST_ASSERT_TRUE_MESSAGE(ours.brier / ours.n <= espn.brier / espn.n + 0.02, "Brier score more than 0.02 worse than ESPN over the game");
  TEST_ASSERT_TRUE_MESSAGE(oursQ4.brier / oursQ4.n <= espnQ4.brier / espnQ4.n + 0.005, "4th-quarter Brier more than 0.005 worse than ESPN");
  TEST_ASSERT_TRUE_MESSAGE(ours.brier / ours.n < prior.brier / prior.n - 0.05, "barely better than the home-court prior");
  TEST_ASSERT_TRUE_MESSAGE(worstGap <= 0.04, "a calibration bin is off by more than 4 points");
  TEST_ASSERT_TRUE((double)ours.correct / ours.n > 0.70);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_parity_with_trainer);
  RUN_TEST(test_clock_parsing);
  RUN_TEST(test_frac_left);
  RUN_TEST(test_symmetric_in_margin_at_tip);
  RUN_TEST(test_monotone_in_margin_and_time);
  RUN_TEST(test_end_points);
  RUN_TEST(test_game_state_wrapper);
  RUN_TEST(test_replay_held_out_games);
  return UNITY_END();
}
