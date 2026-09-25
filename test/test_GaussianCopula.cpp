// test_GaussianCopula.cpp — GaussianCopula.hpp + MovingAverageCopula.hpp
// Compile: see test/run_all.sh (header-only, no companion sources).
//
// Covers the four properties the design rests on:
//   1. Phi / PhiInv round-trip accuracy (the copula is only as good as these).
//   2. r == 0 is BIT-IDENTICAL to the legacy draw — no migration, no behaviour change.
//   3. The output marginal stays EXACTLY uniform for every r (this is the whole point:
//      linear averaging concentrates toward 0.5; see UNIFORM_MARGINALS_COPULA_PLAN.md).
//   4. Empirical lag-m correlation matches the ANALYTIC dot product of the weights.
// Plus: weights are L2-normalised, mix2 endpoints, determinism/statelessness.
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <algorithm>
#include <vector>
#include "GaussianCopula.hpp"
#include "MovingAverageCopula.hpp"

using namespace redDot;
static int failures = 0;

static void check(bool ok, const char* what) {
    if (!ok) { std::printf("  FAIL: %s\n", what); ++failures; }
}
static void checkNear(double got, double want, double tol, const char* what) {
    if (!(std::fabs(got - want) <= tol)) {
        std::printf("  FAIL: %s (got %.10f, want %.10f, tol %g)\n", what, got, want, tol);
        ++failures;
    }
}

// Deterministic uniforms for the test (not Philox — we only need reproducible iid values).
struct Lcg {
    uint64_t s;
    explicit Lcg(uint64_t seed) : s(seed) {}
    double next() {
        s = s * 6364136223846793005ULL + 1442695040888963407ULL;
        return ((s >> 11) + 0.5) * (1.0 / 9007199254740992.0);   // (0,1)
    }
};

static void testPhiRoundTrip() {
    std::printf("Phi/PhiInv round trip\n");
    const double ps[] = {1e-9, 1e-6, 0.001, 0.02, 0.25, 0.5, 0.75, 0.98, 0.999, 1 - 1e-6, 1 - 1e-9};
    for (double p : ps) checkNear(copula::Phi(copula::PhiInv(p)), p, 1e-12, "Phi(PhiInv(p)) == p");
    checkNear(copula::PhiInv(0.5), 0.0, 1e-12, "PhiInv(0.5) == 0");
    checkNear(copula::Phi(0.0), 0.5, 1e-15, "Phi(0) == 0.5");
    check(std::isfinite(copula::PhiInv(0.0)) && std::isfinite(copula::PhiInv(1.0)),
          "PhiInv clamps the 0 and 1 endpoints to finite values");
}

static void testWeightsNormalised() {
    std::printf("weights: L2-normalised\n");
    for (double r : {0.0, 0.1, 0.5, 0.9, 0.97, 1.5 /*clamped*/}) {
        double w[MovingAverageCopula::K];
        MovingAverageCopula::weights(r, w);
        double s2 = 0.0;
        for (double x : w) s2 += x * x;
        checkNear(s2, 1.0, 1e-12, "SUM w^2 == 1");
        check(w[0] > 0.0, "newest tap positive");
    }
}

static void testLegacyIdentity() {
    std::printf("r == 0 is bit-identical to the legacy draw\n");
    Lcg g(12345);
    std::vector<double> u(MovingAverageCopula::K);
    for (int t = 0; t < 1000; ++t) {
        for (auto& x : u) x = g.next();
        const double out = MovingAverageCopula::apply(u.data(), 0.0);
        check(out == u[0], "apply(u, 0) returns u[0] bitwise");
    }
}

// Kolmogorov-Smirnov against U(0,1). NOTE: KS assumes INDEPENDENT samples, but consecutive outputs
// are autocorrelated by construction (that is the feature), so the series must be THINNED first —
// take every K-th sample, beyond the window length, so the thinned samples share no source draws.
// Without thinning the effective sample size is far below N and KS over-rejects at high r.
// The point is to catch CONCENTRATION (a linear average would fail this enormously).
static double ksUniform(std::vector<double> xs) {
    std::sort(xs.begin(), xs.end());
    const double n = (double)xs.size();
    double d = 0.0;
    for (std::size_t i = 0; i < xs.size(); ++i) {
        const double lo = (double)i / n, hi = (double)(i + 1) / n;
        d = std::max(d, std::max(xs[i] - lo, hi - xs[i]));
    }
    return d;
}

static void testUniformMarginal() {
    std::printf("output marginal stays uniform for every r\n");
    const int N = 400000;   // thinned by K below, leaving ~6000 independent samples
    for (double r : {0.0, 0.3, 0.6, 0.9, 0.97}) {
        Lcg g(9001 + (uint64_t)(r * 1000));
        std::vector<double> chain(MovingAverageCopula::K);
        for (auto& x : chain) x = g.next();
        std::vector<double> out;
        out.reserve(N);
        for (int t = 0; t < N; ++t) {
            // slide the window: newest first
            for (std::size_t j = chain.size() - 1; j > 0; --j) chain[j] = chain[j - 1];
            chain[0] = g.next();
            out.push_back(MovingAverageCopula::apply(chain.data(), r));
        }
        std::vector<double> thinned;                       // every K-th: no shared source draws
        for (std::size_t i = 0; i < out.size(); i += MovingAverageCopula::K) thinned.push_back(out[i]);
        const double d = ksUniform(thinned);
        // mean/variance are the blunt check: a linear K-tap average would give var ~ (1/12)/K
        double m = 0.0, v = 0.0;
        for (double x : out) m += x;
        m /= N;
        for (double x : out) v += (x - m) * (x - m);
        v /= (N - 1);
        checkNear(m, 0.5, 0.02, "mean ~ 0.5");
        checkNear(v, 1.0 / 12.0, 0.004, "variance ~ 1/12 (NOT concentrated)");
        const double ksCrit = 1.63 / std::sqrt((double)thinned.size());   // ~1% point
        check(d < ksCrit, "KS distance from U(0,1) within the 1% critical value");
        if (d >= ksCrit)
            std::printf("    r=%.2f KS=%.4f crit=%.4f n=%zu mean=%.4f var=%.5f\n",
                        r, d, ksCrit, thinned.size(), m, v);
    }
}

static void testLagCorrelationMatchesAnalytic() {
    std::printf("empirical lag-m correlation matches analytic weight dot product\n");
    const int N = 200000;
    for (double r : {0.0, 0.5, 0.9}) {
        Lcg g(4242 + (uint64_t)(r * 100));
        std::vector<double> chain(MovingAverageCopula::K);
        for (auto& x : chain) x = g.next();
        std::vector<double> z;
        z.reserve(N);
        for (int t = 0; t < N; ++t) {
            for (std::size_t j = chain.size() - 1; j > 0; --j) chain[j] = chain[j - 1];
            chain[0] = g.next();
            // compare in NORMAL space: the copula's correlation guarantee is on z, and the
            // uniform-space (Spearman) value differs by the usual 6/pi*asin(rho/2) factor.
            z.push_back(copula::PhiInv(MovingAverageCopula::apply(chain.data(), r)));
        }
        for (std::size_t m : {(std::size_t)1, (std::size_t)2, (std::size_t)5}) {
            double mean = 0.0;
            for (double x : z) mean += x;
            mean /= N;
            double num = 0.0, den = 0.0;
            for (std::size_t i = 0; i + m < z.size(); ++i) num += (z[i] - mean) * (z[i + m] - mean);
            for (double x : z) den += (x - mean) * (x - mean);
            const double emp = num / den;
            const double ana = MovingAverageCopula::lagCorr(r, m);
            checkNear(emp, ana, 0.02, "lag correlation == analytic");
            if (std::fabs(emp - ana) > 0.02)
                std::printf("    r=%.2f m=%zu emp=%.4f ana=%.4f\n", r, m, emp, ana);
        }
    }
    // As K grows, lag-1 -> r, so the knob reads as "correlation with the previous draw".
    checkNear(MovingAverageCopula::lagCorr(0.5, 1), 0.5, 0.01, "lag-1 ~ r at r=0.5");
    checkNear(MovingAverageCopula::lagCorr(0.9, 1), 0.9, 0.01, "lag-1 ~ r at r=0.9");
}

static void testMix2Endpoints() {
    std::printf("mix2 (spread) endpoints and mirror\n");
    Lcg g(777);
    for (int t = 0; t < 200; ++t) {
        const double own = g.next(), lead = g.next();
        checkNear(copula::mix2(own, lead, 1.0), lead, 1e-12, "rho=+1 -> leader");
        checkNear(copula::mix2(own, lead, -1.0), 1.0 - lead, 1e-12, "rho=-1 -> complement of leader");
        checkNear(copula::mix2(own, lead, 0.0), own, 1e-9, "rho=0 -> own draw");
        const double mid = copula::mix2(own, lead, 0.5);
        check(mid > 0.0 && mid < 1.0, "rho=0.5 stays in range (no clipping)");
    }
}

static void testDeterministic() {
    std::printf("stateless and deterministic\n");
    Lcg g(31337);
    std::vector<double> u(MovingAverageCopula::K);
    for (auto& x : u) x = g.next();
    const double a = MovingAverageCopula::apply(u.data(), 0.7);
    for (int i = 0; i < 50; ++i)
        check(MovingAverageCopula::apply(u.data(), 0.7) == a, "same window+r -> same output bitwise");
}

int main() {
    std::printf("== GaussianCopula / MovingAverageCopula ==\n");
    testPhiRoundTrip();
    testWeightsNormalised();
    testLegacyIdentity();
    testUniformMarginal();
    testLagCorrelationMatchesAnalytic();
    testMix2Endpoints();
    testDeterministic();
    if (failures == 0) { std::printf("ALL PASS\n"); return 0; }
    std::printf("%d FAILURE(S)\n", failures);
    return 1;
}
