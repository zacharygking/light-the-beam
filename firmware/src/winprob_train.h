// Trains the win-probability model on the device (or on the host in the test bench).
// Same weighted Newton-Raphson fit as tools/train_winprob.py, on the same binned table:
//   header  'W' 'P' bucketSecs version(1)
//   records int8 homeMargin, uint16 timeBucket, uint16 wins, uint16 losses   (little-endian, 7 bytes)
// Header-only, no allocation, no dependencies beyond <math.h>. Per-cell work is done in float
// (the ESP32 has a single-precision FPU; doubles are software), block sums land in double so the
// 4x4 normal equations stay accurate over 35k cells.
#pragma once
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include "winprob.h"

namespace kings {

struct WinProbFit {
  WinProbCoef coef;
  int iterations = 0;
  uint32_t cells = 0;
  uint32_t plays = 0;
  double maxStep = 0;   // largest coefficient change in the final iteration
};

namespace detail {
inline uint16_t rd16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }

// Gaussian elimination with partial pivoting, in place. Returns false if singular.
inline bool solve4(double A[4][4], double b[4], double x[4]) {
  double M[4][5];
  for (int i = 0; i < 4; ++i) { for (int j = 0; j < 4; ++j) M[i][j] = A[i][j]; M[i][4] = b[i]; }
  for (int c = 0; c < 4; ++c) {
    int piv = c;
    for (int r = c + 1; r < 4; ++r) if (fabs(M[r][c]) > fabs(M[piv][c])) piv = r;
    if (fabs(M[piv][c]) < 1e-300) return false;
    if (piv != c) for (int j = 0; j < 5; ++j) { double t = M[c][j]; M[c][j] = M[piv][j]; M[piv][j] = t; }
    for (int r = c + 1; r < 4; ++r) {
      double k = M[r][c] / M[c][c];
      for (int j = c; j < 5; ++j) M[r][j] -= k * M[c][j];
    }
  }
  for (int r = 3; r >= 0; --r) {
    double acc = M[r][4];
    for (int j = r + 1; j < 4; ++j) acc -= M[r][j] * x[j];
    x[r] = acc / M[r][r];
  }
  return true;
}
}  // namespace detail

// Fits a, b_ms, b_s, b_m for the given eps. Returns false on a malformed table or a singular step.
inline bool winProbTrain(const uint8_t* table, size_t len, float eps, WinProbFit& out,
                         int maxIters = 12, double tol = 1e-7) {
  if (!table || len < 4 || table[0] != 'W' || table[1] != 'P' || table[3] != 1) return false;
  const float bucketSecs = table[2];
  const size_t n = (len - 4) / 7;
  if (n == 0) return false;
  const uint8_t* rec = table + 4;

  double coef[4] = {0, 0, 0, 0};
  out.cells = (uint32_t)n;
  out.plays = 0;
  for (size_t i = 0; i < n; ++i) out.plays += detail::rd16(rec + i * 7 + 3) + detail::rd16(rec + i * 7 + 5);

  const double ridge = 1e-6;
  const int BLOCK = 64;
  int it = 0;
  for (; it < maxIters; ++it) {
    double g[4] = {0, 0, 0, 0};
    double H[4][4] = {{0}};
    const float c0 = (float)coef[0], c1 = (float)coef[1], c2 = (float)coef[2], c3 = (float)coef[3];
    for (size_t start = 0; start < n; start += BLOCK) {
      float bg[4] = {0, 0, 0, 0};
      float bH[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};   // upper triangle of the symmetric 4x4
      size_t end = start + BLOCK < n ? start + BLOCK : n;
      for (size_t i = start; i < end; ++i) {
        const uint8_t* r = rec + i * 7;
        const float margin = (float)(int8_t)r[0];
        const float frac = detail::rd16(r + 1) * bucketSecs / WINPROB_REG_SECS;
        const float wins = detail::rd16(r + 3);
        const float cnt = wins + detail::rd16(r + 5);
        const float s = sqrtf(frac + eps);
        const float x[4] = {1.0f, margin / s, s, margin};
        float z = c0 * x[0] + c1 * x[1] + c2 * x[2] + c3 * x[3];
        if (z > 30.0f) z = 30.0f;
        if (z < -30.0f) z = -30.0f;
        const float p = 1.0f / (1.0f + expf(-z));
        const float w = cnt * p * (1.0f - p);
        const float d = wins - cnt * p;
        int k = 0;
        for (int a = 0; a < 4; ++a) {
          bg[a] += x[a] * d;
          const float xw = x[a] * w;
          for (int b = a; b < 4; ++b) bH[k++] += xw * x[b];
        }
      }
      int k = 0;
      for (int a = 0; a < 4; ++a) {
        g[a] += bg[a];
        for (int b = a; b < 4; ++b) { H[a][b] += bH[k]; if (a != b) H[b][a] += bH[k]; ++k; }
      }
    }
    for (int a = 0; a < 4; ++a) { H[a][a] += ridge; g[a] -= ridge * coef[a]; }
    double step[4];
    if (!detail::solve4(H, g, step)) return false;
    double maxStep = 0;
    for (int a = 0; a < 4; ++a) { coef[a] += step[a]; if (fabs(step[a]) > maxStep) maxStep = fabs(step[a]); }
    out.maxStep = maxStep;
    if (maxStep < tol) { ++it; break; }
  }
  out.iterations = it;
  out.coef = {(float)coef[0], (float)coef[1], (float)coef[2], (float)coef[3], eps};
  return true;
}

}  // namespace kings
