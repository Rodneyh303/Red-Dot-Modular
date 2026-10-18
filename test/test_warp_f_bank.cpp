// test_warp_f_bank.cpp — the per-voice warp-F bank (CC_PROMPT_02, Phase 3 step 1).
// Compile: see test/run_all.sh (header-only, no companion sources).
//
// The warp bank (docs/design/CC_PROMPT_02_lanedist_START.md — the authority): a
// value-type std::variant "WarpF" with an IDENTITY default, so the warp mix
//     warped(u) = (1-w)*u + w*F.invCDF(u)
// is BRANCHLESS and a default-constructed bank is all-uniform (no warp). One
// implementation per curve (the existing Distribution subclass / MonotoneCurve —
// the wrappers only forward), a flat 112-bank like spread[112], trivially
// copyable, JSON-persistable with identity+w=0 defaults (old patches unchanged).
//
// Battery (sections land with their CC_PROMPT_02 steps; test-first, red→green):
//   S1. DEFAULT: default WarpF is IdentityWarp (alternative 0); memcpy round-trip.
//   S2. IDENTITY EXACT: invCDF(u)==u, cdf(x)==x, pdf==1 exactly, dense grid.
//   S3. SINGLE-IMPL AGREEMENT: every parametric alternative forwards BIT-IDENTICALLY
//       to the corresponding Distribution subclass (invCDF/cdf/pdf/typeName), and
//       lowerWarp(d) reproduces the wrapper (the authored→bank lowering seam).
//   S4. BANK + MIX: default bank all-identity (warpedValue==u for ANY w); mix
//       endpoints — w=0 is BIT-EXACT u in float, w=1 is exactly invCDF(F,u);
//       delegation selects Macro's global default vs the voice's own bank.
//   S5. DRAWN POOL: DrawnWarp forwards to the pooled MonotoneCurve (the same
//       methods PchipDistribution calls); dedup + capacity.
//   S6. PERSISTENCE: mixed bank save→load survives exactly (types, params, w,
//       pool curves, global slice); missing keys ⟹ all-identity + w=0.
//   S7. MONOTONE (Sklar-safety, the load-bearing invariant): the four VERIFIED
//       steep cases (identity, power k=5, kuma (5,25.8) — the conc=5 steepest the
//       editor allows — revpow theta=5) at w ∈ {0, 0.5, 1} are STRICTLY increasing
//       over a 10k grid, computed double AND stored float32 (matching warped_).
//
// Pure header-only, Rack-free (jansson via the test/jansson.h shim) — modelled on
// test_distribution.cpp; the bank contract is driven directly, the way test_send_tap
// drives MacroTapBank (the store lives in Monsoon's editor slice; the plugin build
// compile-checks that wiring).

#include <cmath>
#include <cstdio>
#include <cstring>
#include <type_traits>
#include <variant>

#include "Distribution.hpp"
#include "WarpF.hpp"

static int g_pass = 0, g_fail = 0;

static void check(bool ok, const char* what) {
    if (ok) { ++g_pass; return; }
    std::printf("  FAIL: %s\n", what);
    ++g_fail;
}
// EXACT (bit) equality — the single-implementation and no-op guarantees are exact by
// construction (same code path / IEEE 0·x+u identities), so nearness would under-test.
static void checkEq(double got, double want, const char* what) {
    if (got == want) { ++g_pass; return; }
    std::printf("  FAIL: %s (got %.17g, want %.17g)\n", what, got, want);
    ++g_fail;
}

// ── static contract: the flat-bank guarantees, proven at compile time ───────────────────────────
static_assert(std::is_trivially_copyable_v<dotModular::WarpF>,
              "WarpF must be trivially copyable: it sits in a flat 112-bank like spread[112] and "
              "must be memcpy-persistable. A polymorphic holder (vptr) or a vector-backed "
              "MonotoneCurve inline would break this.");
static_assert(std::is_same_v<std::variant_alternative_t<0, dotModular::WarpF>, dotModular::IdentityWarp>,
              "IdentityWarp must be alternative 0 — the default-constructed variant is the no-warp case.");

// Drive one (warp, Distribution) pair through all three forwarders + typeName + lowering.
// Every value must be BIT-identical: the wrapper constructs the subclass and calls it —
// there is no second formula, so any difference is a forwarding bug, not tolerance.
static void agreePair(const dotModular::WarpF& f, const dotModular::Distribution& d, const char* what) {
    const int N = 2001;
    bool inv = true, cdf = true, pdf = true;
    const double edge[7] = {-0.1, 0.0, 0.25, 0.5, 0.75, 1.0, 1.1};   // clamps + endpoints
    for (int i = 0; i < N; ++i) {
        const double u = (double)i / (double)(N - 1);
        if (dotModular::invCDF(f, u, nullptr) != d.invCDF(u)) inv = false;
        if (dotModular::cdf  (f, u, nullptr) != d.cdf  (u)) cdf = false;
        if (dotModular::pdf  (f, u, nullptr) != d.pdf  (u)) pdf = false;
    }
    for (double x : edge) {
        if (dotModular::invCDF(f, x, nullptr) != d.invCDF(x)) inv = false;
        if (dotModular::cdf  (f, x, nullptr) != d.cdf  (x)) cdf = false;
        if (dotModular::pdf  (f, x, nullptr) != d.pdf  (x)) pdf = false;
    }
    char buf[160];
    std::snprintf(buf, sizeof buf, "%s: invCDF == subclass (dense grid + clamps, exact)", what);
    check(inv, buf);
    std::snprintf(buf, sizeof buf, "%s: cdf == subclass (dense grid + clamps, exact)", what);
    check(cdf, buf);
    std::snprintf(buf, sizeof buf, "%s: pdf == subclass (dense grid + clamps, exact)", what);
    check(pdf, buf);
    std::snprintf(buf, sizeof buf, "%s: typeName == subclass tag", what);
    check(std::strcmp(dotModular::warpTypeName(f), d.typeName()) == 0, buf);
    std::snprintf(buf, sizeof buf, "%s: lowerWarp(d) reproduces the wrapper (lowering seam)", what);
    check(dotModular::lowerWarp(d) == f, buf);
}

int main() {
    std::printf("[S1] DEFAULT: default WarpF is IdentityWarp; memcpy round-trip\n");
    {
        dotModular::WarpF f;   // default-constructed — the bank's zero state
        check(std::holds_alternative<dotModular::IdentityWarp>(f), "default WarpF holds IdentityWarp");
        check(f.index() == 0, "IdentityWarp is alternative 0 (index 0)");
        check(std::strcmp(dotModular::warpTypeName(f), "identity") == 0, "identity typeName is \"identity\"");

        // The flat-bank contract: a WarpF must survive a raw memcpy (the bank is a plain
        // array; persistence/copy paths may move it as bytes).
        const dotModular::WarpF src = dotModular::PowerWarp{5.0};
        unsigned char bytes[sizeof(dotModular::WarpF)];
        std::memcpy(bytes, &src, sizeof bytes);
        dotModular::WarpF dst;
        std::memcpy(&dst, bytes, sizeof bytes);
        check(dst == src, "WarpF memcpy round-trip preserves alternative + params");
    }

    std::printf("[S2] IDENTITY EXACT: invCDF(u)==u, cdf(x)==x, pdf==1 (10k grid, exact)\n");
    {
        const dotModular::IdentityWarp id;
        const dotModular::WarpF f = id;
        bool inv = true, cdf = true, pdf = true;
        const int N = 10000;
        for (int i = 0; i < N; ++i) {
            const double u = (double)i / (double)(N - 1);
            if (id.invCDF(u) != u) inv = false;
            if (id.cdf(u)   != u) cdf = false;
            if (id.pdf(u)   != 1.0) pdf = false;
            if (dotModular::invCDF(f, u, nullptr) != u) inv = false;
        }
        check(inv, "invCDF(identity,u) == u exactly over 10k grid (member + forwarder)");
        check(cdf, "cdf(identity,x) == x exactly over 10k grid");
        check(pdf, "pdf(identity,x) == 1.0 exactly over 10k grid");
    }

    std::printf("[S3] SINGLE-IMPL AGREEMENT: the variant forwards to the ONE subclass body\n");
    {
        // Every parametric alternative, several parameterizations each (mirroring
        // test_distribution's instances; incl. the steep editor extremes and the
        // truncated-normal degenerates).
        agreePair(dotModular::TriangularWarp{0.3},  dotModular::TriangularDistribution(0.3),  "triangular mode=0.3");
        agreePair(dotModular::TriangularWarp{0.0},  dotModular::TriangularDistribution(0.0),  "triangular mode=0 (rising ramp)");
        agreePair(dotModular::TriangularWarp{1.0},  dotModular::TriangularDistribution(1.0),  "triangular mode=1 (falling ramp)");
        agreePair(dotModular::PowerWarp{0.2},       dotModular::PowerDistribution(0.2),       "power k=0.2 (mass at 0)");
        agreePair(dotModular::PowerWarp{1.0},       dotModular::PowerDistribution(1.0),       "power k=1 (uniform)");
        agreePair(dotModular::PowerWarp{5.0},       dotModular::PowerDistribution(5.0),       "power k=5 (steep near 0)");
        agreePair(dotModular::ReversedPowerWarp{0.2}, dotModular::ReversedPowerDistribution(0.2), "reversed-power theta=0.2 (mass at 1)");
        agreePair(dotModular::ReversedPowerWarp{5.0}, dotModular::ReversedPowerDistribution(5.0), "reversed-power theta=5 (steep near 1)");
        agreePair(dotModular::KumaraswamyWarp{1.0, 1.0},  dotModular::KumaraswamyDistribution(1.0, 1.0),  "kumaraswamy (1,1) (uniform)");
        agreePair(dotModular::KumaraswamyWarp{2.0, 3.0},  dotModular::KumaraswamyDistribution(2.0, 3.0),  "kumaraswamy (2,3)");
        agreePair(dotModular::KumaraswamyWarp{5.0, 25.8}, dotModular::KumaraswamyDistribution(5.0, 25.8), "kumaraswamy (5,25.8) — the conc=5 steepest");
        agreePair(dotModular::MobiusWarp{0.25},     dotModular::MobiusDistribution(0.25),     "mobius a=0.25 (tilt to 0)");
        agreePair(dotModular::MobiusWarp{1.0},      dotModular::MobiusDistribution(1.0),      "mobius a=1 (identity)");
        agreePair(dotModular::MobiusWarp{4.0},      dotModular::MobiusDistribution(4.0),      "mobius a=4 (tilt to 1)");
        agreePair(dotModular::LogitSigmoidWarp{1.0, 0.0},  dotModular::LogitSigmoidDistribution(1.0, 0.0),  "logit-sigmoid (1,0) (identity)");
        agreePair(dotModular::LogitSigmoidWarp{2.0, -0.5}, dotModular::LogitSigmoidDistribution(2.0, -0.5), "logit-sigmoid (2,-0.5)");
        agreePair(dotModular::LogitSigmoidWarp{0.5, 1.0},  dotModular::LogitSigmoidDistribution(0.5, 1.0),  "logit-sigmoid (0.5,1)");
        agreePair(dotModular::LogitNormalWarp{0.0, 1.0},   dotModular::LogitNormalDistribution(0.0, 1.0),   "logit-normal (0,1)");
        agreePair(dotModular::LogitNormalWarp{0.5, 0.4},   dotModular::LogitNormalDistribution(0.5, 0.4),   "logit-normal (0.5,0.4)");
        agreePair(dotModular::LogitNormalWarp{-1.0, 2.0},  dotModular::LogitNormalDistribution(-1.0, 2.0),  "logit-normal (-1,2)");
        agreePair(dotModular::TruncNormalWarp{0.5, 0.2},   dotModular::TruncatedNormalDistribution(0.5, 0.2), "truncated-normal (0.5,0.2)");
        agreePair(dotModular::TruncNormalWarp{0.3, 0.1},   dotModular::TruncatedNormalDistribution(0.3, 0.1), "truncated-normal (0.3,0.1)");
        agreePair(dotModular::TruncNormalWarp{-2.0, 0.5},  dotModular::TruncatedNormalDistribution(-2.0, 0.5), "truncated-normal (-2,0.5) degenerate left");
        agreePair(dotModular::TruncNormalWarp{2.0, 0.5},   dotModular::TruncatedNormalDistribution(2.0, 0.5),  "truncated-normal (2,0.5) degenerate right");

        // The drawn case is NOT lowerWarp's business (a Pchip needs a POOL slot — bank
        // level state); it lowers to the safe default, identity.
        check(std::holds_alternative<dotModular::IdentityWarp>(
                  dotModular::lowerWarp(dotModular::PchipDistribution())),
              "lowerWarp(drawn) → IdentityWarp (drawn is lowered at the bank/pool level)");
    }

    std::printf("%s: %d passed, %d failed\n", (g_fail == 0 ? "ALL OK" : "FAILURES"), g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
