// test_moving_average_copula.cpp — the NEGATIVE-r / anti-correlation path of the slew copula.
//
// SCOPE NOTE: test_GaussianCopula.cpp already covers the POSITIVE-r copula (SUM w^2 == 1 over
// r in {0..0.97,1.5}, apply(u,0)==u[0] bitwise, KS uniformity). It does NOT exercise r < 0 —
// the "anti-correlation / hocket" path the header flags as a real feature (bipolar slew knob,
// -1 = anti-correlated). This test fills THAT gap: the symmetric clamp, the geometric ratio
// (incl. the alternating sign), and that lag-1 correlation goes NEGATIVE for r < 0. The couple
// of positive-r assertions here are kept only as the comparison baseline for the symmetric cases.
#include <cstdio>
#include <cmath>
#include <initializer_list>
#include "dsp/MovingAverageCopula.hpp"

using redDot::MovingAverageCopula;
using MAC = MovingAverageCopula;

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("  FAIL: %s\n", (m)); ++g_fail; } } while (0)

static double clampR(double r) { return r < -MAC::R_MAX ? -MAC::R_MAX : (r > MAC::R_MAX ? MAC::R_MAX : r); }

int main() {
    std::printf("test_moving_average_copula\n");
    constexpr std::size_t K = MAC::K;
    double w[K];

    // 1. SUM w^2 == 1 holds for NEGATIVE r too (the uniform-output guarantee is sign-agnostic).
    for (double r : {-0.97, -0.9, -0.5, -0.1}) {
        MAC::weights(r, w);
        double s2 = 0.0; for (std::size_t j = 0; j < K; ++j) s2 += w[j] * w[j];
        CHECK(std::fabs(s2 - 1.0) < 1e-9, "neg-r: sum w^2 == 1");
    }

    // 2. Geometric ratio with the ALTERNATING SIGN: w[j+1] == w[j]*r (r<0 -> signs alternate).
    for (double r : {-0.9, -0.5, -0.2}) {
        MAC::weights(r, w);
        for (std::size_t j = 0; j + 1 < K; ++j)
            CHECK(std::fabs(w[j+1] - w[j] * r) < 1e-12, "neg-r: w[j+1]==w[j]*r (alternating)");
        // odd taps negative, even taps positive (w[0] > 0 by construction)
        CHECK(w[0] > 0.0 && w[1] < 0.0 && w[2] > 0.0, "neg-r: signs alternate from a positive w[0]");
    }

    // 3. The r clamp is SYMMETRIC: r < -R_MAX gives exactly the -R_MAX weights.
    {
        double wc[K], wo[K];
        MAC::weights(-0.97, wc); MAC::weights(-5.0, wo);
        for (std::size_t j = 0; j < K; ++j) CHECK(wc[j] == wo[j], "r<-R_MAX clamps to -R_MAX");
    }

    // 4. ANTI-CORRELATION: lag-1 correlation SUM_j w_j*w_{j+1} is NEGATIVE for r<0 and
    //    POSITIVE for r>0 (the hocket vs smooth distinction); magnitude grows with |r|.
    {
        auto lag1 = [&](double r) {
            MAC::weights(r, w);
            double acc = 0.0; for (std::size_t j = 0; j + 1 < K; ++j) acc += w[j] * w[j+1];
            return acc;
        };
        CHECK(lag1(-0.5) < 0.0, "r<0: lag-1 correlation is negative (anti-correlated)");
        CHECK(lag1(-0.9) < lag1(-0.5), "neg lag-1 grows (more negative) with |r|");
        CHECK(lag1(0.5)  > 0.0, "r>0: lag-1 positive (baseline)");
        CHECK(std::fabs(lag1(0.5) + lag1(-0.5)) < 1e-9, "lag-1 is odd in r (sign flips, magnitude equal)");
    }

    // 5. apply() with r<0 stays finite and in a sane range (the combine path on alternating weights).
    {
        double u[K];
        for (std::size_t j = 0; j < K; ++j) u[j] = 0.5 + 0.2 * std::sin(0.3 * (double)j);
        double out = MAC::apply(u, -0.6);
        CHECK(std::isfinite(out), "apply(u, r<0) is finite");
        CHECK(out >= 0.0 && out <= 1.0, "apply(u, r<0) in [0,1] (still a uniform readout)");
    }

    (void)clampR;
    if (g_fail == 0) std::printf("  OK\n");
    else             std::printf("  %d FAILURE(S)\n", g_fail);
    return g_fail ? 1 : 0;
}
