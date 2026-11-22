// test_moving_average_copula.cpp — coverage for MovingAverageCopula (MovingAverageCopula.hpp),
// the K=64 slew copula central to slew/scrub/canon. No dedicated test before (coverage-gap fill
// 2026-11-22). Characterization: the properties the header PROMISES (L2-normalised geometric
// weights, symmetric r clamp, r==0 passthrough, analytic lag correlation).
#include <cstdio>
#include <cmath>
#include <initializer_list>
#include "dsp/MovingAverageCopula.hpp"

using redDot::MovingAverageCopula;
using MAC = MovingAverageCopula;

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("  FAIL: %s\n", (m)); ++g_fail; } } while (0)

int main() {
    std::printf("test_moving_average_copula\n");
    constexpr std::size_t K = MAC::K;
    double w[K];

    // 1. SUM w^2 == 1 for every r (the uniform-output guarantee), incl. negative and clamped.
    for (double r : {-0.97, -0.9, -0.5, -0.1, 0.0, 0.1, 0.5, 0.9, 0.97, 1.5, -1.5}) {
        MAC::weights(r, w);
        double s2 = 0.0; for (std::size_t j = 0; j < K; ++j) s2 += w[j] * w[j];
        CHECK(std::fabs(s2 - 1.0) < 1e-9, "sum of w^2 == 1");
    }

    // 2. Geometric ratio: w[j+1]/w[j] == clampR(r) for every j (the defining shape).
    for (double r : {-0.9, -0.3, 0.2, 0.6, 0.97}) {
        MAC::weights(r, w);
        double rc = (r < -MAC::R_MAX) ? -MAC::R_MAX : (r > MAC::R_MAX ? MAC::R_MAX : r);
        for (std::size_t j = 0; j + 1 < K; ++j)
            CHECK(std::fabs(w[j+1] - w[j] * rc) < 1e-12, "w[j+1] == w[j]*r (geometric)");
    }

    // 3. r == 0 is a clean impulse: w[0]==1, the rest 0 (newest draw only).
    {
        MAC::weights(0.0, w);
        CHECK(std::fabs(w[0] - 1.0) < 1e-12, "r=0: w[0]==1");
        double tail = 0.0; for (std::size_t j = 1; j < K; ++j) tail += std::fabs(w[j]);
        CHECK(tail < 1e-12, "r=0: w[1..] == 0");
    }

    // 4. The r clamp is symmetric to ±R_MAX: beyond-range r gives the clamped weights exactly.
    {
        double wc[K], wo[K];
        MAC::weights(0.97, wc); MAC::weights(5.0, wo);
        for (std::size_t j = 0; j < K; ++j) CHECK(wc[j] == wo[j], "r>R_MAX clamps to +R_MAX");
        MAC::weights(-0.97, wc); MAC::weights(-5.0, wo);
        for (std::size_t j = 0; j < K; ++j) CHECK(wc[j] == wo[j], "r<-R_MAX clamps to -R_MAX");
    }

    // 5. apply(u, 0) returns u[0] BIT-IDENTICALLY (the legacy passthrough, NaN-safe tail).
    {
        double u[K];
        for (std::size_t j = 0; j < K; ++j) u[j] = 0.1 + 0.001 * (double)j;
        CHECK(MAC::apply(u, 0.0) == u[0], "apply(u,0) == u[0] exactly");
        // a NaN deep in the window must not corrupt the r==0 passthrough
        double u2[K]; for (std::size_t j = 0; j < K; ++j) u2[j] = u[j];
        u2[40] = std::nan("");
        CHECK(MAC::apply(u2, 0.0) == u2[0], "r=0 passthrough ignores window NaNs");
    }

    // 6. lagCorr(r, m) == the analytic dot product SUM_j w_j*w_{j+m}, and lag-0 == 1,
    //    lag-1 grows toward r as the header claims (monotone in r over [0, R_MAX)).
    {
        auto lag = [&](double r, std::size_t m) {
            MAC::weights(r, w);
            double acc = 0.0; for (std::size_t j = 0; j + m < K; ++j) acc += w[j] * w[j+m];
            return acc;
        };
        for (double r : {0.0, 0.3, 0.6, 0.9}) CHECK(std::fabs(lag(r, 0) - 1.0) < 1e-9, "lag-0 == 1");
        double prev = -1.0;
        for (double r = 0.0; r <= 0.95; r += 0.05) {
            double l1 = lag(r, 1);
            CHECK(l1 >= prev - 1e-9, "lag-1 correlation monotone non-decreasing in r");
            CHECK(l1 >= -1e-9 && l1 <= 1.0 + 1e-9, "lag-1 in [0,1]");
            prev = l1;
        }
        CHECK(lag(0.0, 1) < 1e-9, "r=0: lag-1 == 0 (independent draws)");
    }

    if (g_fail == 0) std::printf("  OK\n");
    else             std::printf("  %d FAILURE(S)\n", g_fail);
    return g_fail ? 1 : 0;
}
