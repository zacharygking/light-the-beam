// Host-side check of the on-device trainer (firmware/src/winprob_train.h).
//   pio test -e native -f test_winprob_train
// Refits the model from the embedded table with the exact ESP32 code and compares with the
// Python fit compiled into winprob_coef.h; also recovers known coefficients from synthetic data.
#include <unity.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <vector>
#include "winprob_train.h"

using namespace kings;

static std::vector<uint8_t> readAll(const char* path) {
  std::vector<uint8_t> buf;
  FILE* f = fopen(path, "rb");
  if (!f) return buf;
  uint8_t tmp[4096]; size_t n;
  while ((n = fread(tmp, 1, sizeof tmp, f)) > 0) buf.insert(buf.end(), tmp, tmp + n);
  fclose(f);
  return buf;
}

void test_refit_matches_python() {
  std::vector<uint8_t> table = readAll(DATA_DIR "/winprob_train.bin");
  TEST_ASSERT_TRUE_MESSAGE(!table.empty(), "missing firmware/data/winprob_train.bin (run tools/train_winprob.py)");
  WinProbFit fit;
  clock_t t0 = clock();
  TEST_ASSERT_TRUE(winProbTrain(table.data(), table.size(), WINPROB_EPS, fit));
  double ms = 1000.0 * (clock() - t0) / CLOCKS_PER_SEC;
  printf("\n  refit: %u cells, %u plays, %d iterations, %.0f ms on this host\n", fit.cells, fit.plays, fit.iterations, ms);
  printf("  C++    a=%.6f b_ms=%.6f b_s=%.6f b_m=%.6f\n", fit.coef.a, fit.coef.b_ms, fit.coef.b_s, fit.coef.b_m);
  printf("  Python a=%.6f b_ms=%.6f b_s=%.6f b_m=%.6f\n", WINPROB_A, WINPROB_B_MS, WINPROB_B_S, WINPROB_B_M);
  TEST_ASSERT_EQUAL_UINT32(WINPROB_TRAIN_CELLS, fit.cells);
  TEST_ASSERT_EQUAL_UINT32(WINPROB_TRAIN_PLAYS, fit.plays);
  TEST_ASSERT_TRUE(fit.iterations >= 3 && fit.iterations <= 12);
  // What matters is that both fits give the same probabilities everywhere the display can show.
  float worst = 0;
  for (int m = -40; m <= 40; ++m)
    for (int t = 0; t <= 2880; t += 15) {
      float f = t / 2880.0f;
      float d = fabsf(winProbHome(m, f, fit.coef) - winProbHome(m, f, WINPROB_COMPILED));
      if (d > worst) worst = d;
    }
  char msg[64]; snprintf(msg, sizeof msg, "worst |C++ fit - Python fit| = %.2e", worst);
  printf("  %s\n", msg);
  TEST_ASSERT_TRUE_MESSAGE(worst < 1e-3f, msg);
  TEST_ASSERT_FLOAT_WITHIN(2e-3f, WINPROB_A, fit.coef.a);
  TEST_ASSERT_FLOAT_WITHIN(2e-3f, WINPROB_B_MS, fit.coef.b_ms);
  TEST_ASSERT_FLOAT_WITHIN(2e-3f, WINPROB_B_S, fit.coef.b_s);
  TEST_ASSERT_FLOAT_WITHIN(2e-3f, WINPROB_B_M, fit.coef.b_m);
}

// Build a table from a known model and check the trainer recovers it.
static void put16(std::vector<uint8_t>& v, unsigned x) { v.push_back(x & 0xFF); v.push_back((x >> 8) & 0xFF); }
void test_recovers_known_coefficients() {
  const WinProbCoef truth = {0.05f, 0.12f, 0.15f, -0.01f, 0.005f};
  std::vector<uint8_t> t = {'W', 'P', 5, 1};
  const unsigned N = 4000;   // plays per cell; large so the sample proportions are tight
  for (int m = -30; m <= 30; m += 2)
    for (int b = 0; b <= 576; b += 12) {
      float p = winProbHome(m, b * 5 / 2880.0f, truth);
      unsigned wins = (unsigned)lround(p * N);
      t.push_back((uint8_t)(int8_t)m); put16(t, b); put16(t, wins); put16(t, N - wins);
    }
  WinProbFit fit;
  TEST_ASSERT_TRUE(winProbTrain(t.data(), t.size(), truth.eps, fit));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, truth.a, fit.coef.a);
  TEST_ASSERT_FLOAT_WITHIN(0.003f, truth.b_ms, fit.coef.b_ms);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, truth.b_s, fit.coef.b_s);
  TEST_ASSERT_FLOAT_WITHIN(0.002f, truth.b_m, fit.coef.b_m);
}

void test_rejects_bad_table() {
  WinProbFit fit;
  const uint8_t junk[] = {'X', 'P', 5, 1, 0, 0, 0, 0, 0, 0, 0};
  TEST_ASSERT_FALSE(winProbTrain(junk, sizeof junk, 0.005f, fit));
  const uint8_t empty[] = {'W', 'P', 5, 1};
  TEST_ASSERT_FALSE(winProbTrain(empty, sizeof empty, 0.005f, fit));
  TEST_ASSERT_FALSE(winProbTrain(nullptr, 0, 0.005f, fit));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_refit_matches_python);
  RUN_TEST(test_recovers_known_coefficients);
  RUN_TEST(test_rejects_bad_table);
  return UNITY_END();
}
