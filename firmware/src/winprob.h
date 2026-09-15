// Live win probability from the score, clock and period the firmware already polls.
// Coefficients come from tools/train_winprob.py (see docs/winprob.md); this header is pure
// and header-only so the host test bench (test/test_winprob) runs the exact ESP32 code path.
#pragma once
#include <math.h>
#include <stdlib.h>
#include "game_state.h"
#include "winprob_coef.h"

namespace kings {

static const float WINPROB_REG_SECS = 2880.0f;

// "4:50" -> 290, "0:03.7" -> 3.7, "57.4" -> 57.4, "" -> 0. Only the last two fields count.
inline float clockSeconds(const char* clock) {
  if (!clock || !*clock) return 0.0f;
  const char* colon = nullptr;
  for (const char* c = clock; *c; ++c) if (*c == ':') colon = c;
  if (!colon) return (float)atof(clock);
  int mins = 0;
  const char* m = colon;
  while (m > clock && m[-1] >= '0' && m[-1] <= '9') --m;
  if (m < colon) mins = atoi(m);
  return mins * 60.0f + (float)atof(colon + 1);
}

// Fraction of regulation time remaining. Overtime is treated as its own clock over 48 min,
// which is small and matches how the model was trained.
inline float fracLeft(int period, float secsLeftInPeriod) {
  if (period < 1) return 1.0f;
  float secs = period <= 4 ? (4 - period) * 720.0f + secsLeftInPeriod : secsLeftInPeriod;
  if (secs < 0.0f) secs = 0.0f;
  float f = secs / WINPROB_REG_SECS;
  return f > 1.0f ? 1.0f : f;
}

// P(home team wins) given home lead (negative when trailing) and fraction of regulation left.
inline float winProbHome(int homeMargin, float frac) {
  float s = sqrtf(frac + WINPROB_EPS);
  float z = WINPROB_A + WINPROB_B_MS * (homeMargin / s) + WINPROB_B_S * s + WINPROB_B_M * homeMargin;
  if (z > 30.0f) z = 30.0f;
  if (z < -30.0f) z = -30.0f;
  return 1.0f / (1.0f + expf(-z));
}

// P(our team wins) for the polled game state. Returns -1 when there is no live score.
inline float winProbUs(const GameState& gs) {
  if (gs.us.score < 0 || gs.them.score < 0 || gs.period < 1) return -1.0f;
  float f = fracLeft(gs.period, clockSeconds(gs.clock));
  int ourMargin = gs.us.score - gs.them.score;
  if (gs.weAreHome()) return winProbHome(ourMargin, f);
  return 1.0f - winProbHome(-ourMargin, f);
}

}  // namespace kings
