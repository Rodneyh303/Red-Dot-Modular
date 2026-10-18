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
#include <cstdlib>
#include <cstring>
#include <type_traits>
#include <utility>
#include <variant>

#include "Distribution.hpp"
#include "MonotoneCurve.hpp"
#include "WarpF.hpp"

using dotModular::MonotoneCurve;

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

// A curve from explicit valid points via the public JSON path — arbitrary placement is
// only reachable through a .dmcurve-shaped load (the same entry the editor files use);
// test_distribution.cpp's makeCurve pattern.
static MonotoneCurve makeCurve(std::initializer_list<std::pair<double, double>> pts) {
    json_t* root = json_object();
    json_object_set_new(root, "format", json_string("dmcurve"));
    json_object_set_new(root, "version", json_integer(1));
    json_t* jp = json_array();
    for (const auto& p : pts) {
        json_t* pair = json_array();
        json_array_append_new(pair, json_real(p.first));
        json_array_append_new(pair, json_real(p.second));
        json_array_append_new(jp, pair);
    }
    json_object_set_new(root, "points", jp);
    MonotoneCurve c;
    check(c.fromJson(root), "makeCurve: valid curve loads");
    json_decref(root);
    return c;
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

    std::printf("[S4] BANK + MIX: default no-op; endpoints (w=0 bit-exact, w=1 exact); delegation\n");
    {
        // ── S4a DEFAULT: the no-warp guarantee ────────────────────────────────────────────
        dotModular::WarpBank bank;
        bool allId = true, allW0 = true, allGId = true;
        for (int i = 0; i < dotModular::WarpBank::N; ++i) {
            if (!std::holds_alternative<dotModular::IdentityWarp>(bank.warpF[i])) allId = false;
            if (bank.warpW[i] != 0.f) allW0 = false;
        }
        for (int lane = 0; lane < dotModular::WarpBank::LANES; ++lane)
            if (!std::holds_alternative<dotModular::IdentityWarp>(bank.globalWarpF[lane])) allGId = false;
        check(allId, "default bank: all 112 slots IdentityWarp");
        check(allW0, "default bank: all 112 depths w=0");
        check(allGId, "default bank: global default lanes all IdentityWarp");

        // identity F + ANY w ⟹ still exactly u on the STORED FLOAT (the mix runs the same
        // branchless code — identity is not special-cased; this is the no-warp guarantee).
        const int NG = 5001;
        for (float w : {0.f, 0.25f, 0.5f, 1.f}) {
            for (int i = 0; i < dotModular::WarpBank::N; ++i) bank.warpW[i] = w;
            bool ok = true;
            for (int i = 0; i < NG; ++i) {
                const double u = (double)i / (double)(NG - 1);
                if ((float)bank.warpedValue(3, 4, u) != (float)u) ok = false;
                if ((float)bank.warpedValue(0, 0, u) != (float)u) ok = false;
                if ((float)bank.warpedValue(15, 6, u) != (float)u) ok = false;
            }
            char b[96];
            std::snprintf(b, sizeof b, "identity F: warpedValue == u (stored float) at w=%g", (double)w);
            check(ok, b);
        }
        for (int i = 0; i < dotModular::WarpBank::N; ++i) bank.warpW[i] = 0.f;   // restore

        // ── S4b MIX ENDPOINTS ─────────────────────────────────────────────────────────────
        // A roster spanning the palette (incl. the steep editor extremes), driven through
        // slot (v=3, lane=4).
        const dotModular::WarpF roster[] = {
            dotModular::IdentityWarp{},
            dotModular::TriangularWarp{0.3},
            dotModular::PowerWarp{5.0},                       // steep near 0
            dotModular::ReversedPowerWarp{5.0},               // steep near 1
            dotModular::KumaraswamyWarp{5.0, 25.8},           // the conc=5 steepest
            dotModular::MobiusWarp{4.0},
            dotModular::LogitSigmoidWarp{2.0, -0.5},
            dotModular::LogitNormalWarp{0.5, 0.4},
            dotModular::TruncNormalWarp{0.5, 0.2},
            dotModular::TruncNormalWarp{-2.0, 0.5},           // degenerate left
        };
        const int R = (int)(sizeof roster / sizeof roster[0]);
        // w=0 is a BIT-EXACT no-op in the stored float — for EVERY F (CC_PROMPT_02's
        // verified claim; assert EQUALITY, not nearness).
        bool w0exact = true;
        for (int r = 0; r < R; ++r) {
            bank.warpF[dotModular::WarpBank::index(3, 4)] = roster[r];
            bank.warpW[dotModular::WarpBank::index(3, 4)] = 0.f;
            for (int i = 0; i < NG; ++i) {
                const double u = (double)i / (double)(NG - 1);
                if ((float)bank.warpedValue(3, 4, u) != (float)u) { w0exact = false; break; }
            }
        }
        check(w0exact, "w=0: warpedValue == u BIT-EXACT (stored float), every F in the roster");
        // w=1 is EXACTLY the curve: (1-1)*u + 1*F.invCDF(u) == F.invCDF(u), no rounding.
        bool w1exact = true;
        for (int r = 0; r < R; ++r) {
            bank.warpF[dotModular::WarpBank::index(3, 4)] = roster[r];
            bank.warpW[dotModular::WarpBank::index(3, 4)] = 1.f;
            for (int i = 0; i < NG; ++i) {
                const double u = (double)i / (double)(NG - 1);
                if (bank.warpedValue(3, 4, u) != dotModular::invCDF(roster[r], u, nullptr)) { w1exact = false; break; }
            }
        }
        check(w1exact, "w=1: warpedValue == invCDF(F,u) EXACTLY (double), every F in the roster");

        // ── S4c DELEGATION: the owner()/topo pattern — a ceded lane tracks the GLOBAL ────
        bank = dotModular::WarpBank{};   // reset to defaults
        bank.warpF[dotModular::WarpBank::index(2, 3)] = dotModular::PowerWarp{5.0};
        bank.warpW[dotModular::WarpBank::index(2, 3)] = 0.7f;
        bank.globalWarpF[3] = dotModular::ReversedPowerWarp{5.0};
        bank.globalWarpW[3] = 0.25f;
        {
            const double u = 0.42;
            const double ownW = (double)0.7f, globW = (double)0.25f;
            checkEq(bank.warpedValue(2, 3, u, false),
                    (1.0 - ownW) * u + ownW * dotModular::invCDF(dotModular::PowerWarp{5.0}, u, nullptr),
                    "not delegated: the voice's own slot (Power(5) at w=0.7)");
            checkEq(bank.warpedValue(2, 3, u, true),
                    (1.0 - globW) * u + globW * dotModular::invCDF(dotModular::ReversedPowerWarp{5.0}, u, nullptr),
                    "delegated: the GLOBAL default (RevPower(5) at w=0.25)");
            check(std::holds_alternative<dotModular::PowerWarp>(bank.effectiveF(2, 3, false))
               && std::holds_alternative<dotModular::ReversedPowerWarp>(bank.effectiveF(2, 3, true)),
                  "effectiveF selects own slot vs global by the topology-derived flag");
            check(bank.effectiveW(2, 3, false) == 0.7f && bank.effectiveW(2, 3, true) == 0.25f,
                  "effectiveW selects own depth vs global by the topology-derived flag");
        }
    }

    std::printf("[S5] DRAWN POOL: DrawnWarp forwards to the pooled curve; dedup; capacity\n");
    {
        // makeCurve: the file-static helper above (the public .dmcurve JSON path).
        dotModular::WarpBank bank;
        const MonotoneCurve cA = makeCurve({{0.0, 0.0}, {0.3, 0.1}, {0.6, 0.5}, {1.0, 1.0}});
        const MonotoneCurve cB = makeCurve({{0.0, 0.0}, {0.2, 0.4}, {0.7, 0.9}, {1.0, 1.0}});

        const int iA = bank.putCurve(cA);
        check(iA == 0, "first distinct curve takes pool slot 0");
        const int iB = bank.putCurve(cB);
        check(iB == 1, "second distinct curve takes slot 1");
        const int iA2 = bank.putCurve(cA);
        check(iA2 == iA, "re-committing the SAME curve reuses its pool slot (dedup)");
        check(bank.poolSize() == 2, "poolSize counts distinct curves only");

        // Forwarding: DrawnWarp == the curve's OWN methods, exactly (one implementation —
        // the same MonotoneCurve members PchipDistribution calls).
        const dotModular::WarpF fA = dotModular::DrawnWarp{(uint16_t)iA};
        check(std::strcmp(dotModular::warpTypeName(fA), "drawn") == 0, "drawn typeName is \"drawn\"");
        bool dInv = true, dCdf = true, dPdf = true;
        for (int i = 0; i < 2001; ++i) {
            const double u = (double)i / 2000.0;
            if (dotModular::invCDF(fA, u, bank.pool) != cA.evalInverse(u)) dInv = false;
            if (dotModular::cdf  (fA, u, bank.pool) != cA.evalCDF(u))     dCdf = false;
            if (dotModular::pdf  (fA, u, bank.pool) != cA.pdf(u))         dPdf = false;
        }
        check(dInv && dCdf && dPdf, "DrawnWarp invCDF/cdf/pdf == pooled MonotoneCurve's own methods (exact)");

        // Mix over a drawn slot: w=1 is exactly the curve's inverse.
        bank.warpF[dotModular::WarpBank::index(5, 2)] = fA;
        bank.warpW[dotModular::WarpBank::index(5, 2)] = 1.f;
        bool dW1 = true;
        for (int i = 0; i < 2001; ++i) {
            const double u = (double)i / 2000.0;
            if (bank.warpedValue(5, 2, u) != cA.evalInverse(u)) dW1 = false;
        }
        check(dW1, "drawn slot at w=1: warpedValue == curve.evalInverse(u) exactly");

        // fromDistribution — the set-time commit path: parametrics lower, drawn pools.
        const dotModular::WarpF fP = bank.fromDistribution(dotModular::PowerDistribution(2.5));
        check(std::holds_alternative<dotModular::PowerWarp>(fP) &&
              std::get<dotModular::PowerWarp>(fP) == dotModular::PowerWarp{2.5},
              "fromDistribution(parametric) lowers via lowerWarp");
        const dotModular::WarpF fD = bank.fromDistribution(dotModular::PchipDistribution(cB));
        check(std::holds_alternative<dotModular::DrawnWarp>(fD) &&
              std::get<dotModular::DrawnWarp>(fD).idx == (uint16_t)iB,
              "fromDistribution(drawn) reuses the identical curve's pool slot");

        // Capacity: fill with distinct curves; a NEW distinct one is refused (kNoSlot);
        // an already-present one still resolves; the commit path degrades to identity.
        for (int k = 0; bank.poolSize() < (int)dotModular::kWarpPoolCap && k < 100; ++k) {
            const double xk = 0.05 + 0.01 * k;   // distinct x ⟹ distinct point set
            const MonotoneCurve ck = makeCurve({{0.0, 0.0}, {xk, 0.3}, {1.0, 1.0}});
            check(bank.putCurve(ck) >= 0, "fill: distinct curve takes a slot");
        }
        check(bank.poolSize() == (int)dotModular::kWarpPoolCap, "pool filled to the documented capacity (32)");
        check(bank.poolFull(), "poolFull() at capacity");
        const MonotoneCurve cNew = makeCurve({{0.0, 0.0}, {0.42, 0.05}, {0.55, 0.7}, {1.0, 1.0}});
        check(bank.putCurve(cNew) == dotModular::WarpBank::kNoSlot, "NEW distinct curve at capacity refused (kNoSlot)");
        check(bank.putCurve(cA) == iA, "an already-present curve still resolves at capacity");
        check(std::holds_alternative<dotModular::IdentityWarp>(
                  bank.fromDistribution(dotModular::PchipDistribution(cNew))),
              "fromDistribution(drawn) at capacity falls back to IdentityWarp (safe default)");
    }

    std::printf("[S6] PERSISTENCE: mixed bank round-trips exactly; missing keys ⟹ identity + w=0\n");
    {
        using Idx = dotModular::WarpBank;   // readability alias for the index() calls
        dotModular::WarpBank src;
        src.warpF[Idx::index(0, 0)]  = dotModular::TriangularWarp{0.3};      src.warpW[Idx::index(0, 0)]  = 0.25f;
        src.warpF[Idx::index(4, 2)]  = dotModular::KumaraswamyWarp{5.0, 25.8}; src.warpW[Idx::index(4, 2)] = 1.f;
        src.warpF[Idx::index(7, 5)]  = dotModular::LogitNormalWarp{-0.5, 0.8};  src.warpW[Idx::index(7, 5)]  = 0.6f;
        src.warpF[Idx::index(2, 1)]  = dotModular::MobiusWarp{2.5};             src.warpW[Idx::index(2, 1)]  = 0.5f;
        src.warpF[Idx::index(9, 0)]  = dotModular::ReversedPowerWarp{3.0};      src.warpW[Idx::index(9, 0)]  = 0.75f;
        src.warpF[Idx::index(1, 3)]  = dotModular::TruncNormalWarp{0.4, 0.15};  src.warpW[Idx::index(1, 3)]  = 0.1f;
        src.warpF[Idx::index(11, 6)] = dotModular::PowerWarp{0.5};              src.warpW[Idx::index(11, 6)] = 0.9f;
        src.warpF[Idx::index(13, 4)] = dotModular::LogitSigmoidWarp{1.5, 0.3};  src.warpW[Idx::index(13, 4)] = 0.35f;
        const MonotoneCurve c1 = makeCurve({{0.0, 0.0}, {0.25, 0.05}, {0.7, 0.8}, {1.0, 1.0}});
        const int pc1 = src.putCurve(c1);
        src.warpF[Idx::index(5, 0)] = dotModular::DrawnWarp{(uint16_t)pc1};    src.warpW[Idx::index(5, 0)] = 0.8f;
        // a SECOND slot SHARING the same drawn curve (pool dedup is part of the contract)
        src.warpF[Idx::index(6, 0)] = dotModular::DrawnWarp{(uint16_t)pc1};    src.warpW[Idx::index(6, 0)] = 0.2f;
        src.globalWarpF[2] = dotModular::PowerWarp{5.0};                       src.globalWarpW[2] = 0.5f;

        json_t* root = json_object();
        dotModular::WarpBank::serialize(src, root, "editor");
        dotModular::WarpBank dst;
        dst.warpF[0] = dotModular::PowerWarp{9.0};   // pre-poison: parse must RESET, not merge
        dst.warpW[0] = 0.9f;
        dst.poolUsed = 7;                           // pre-poison the pool high-water too
        const bool full = dotModular::WarpBank::parse(root, "editor", dst);
        json_decref(root);

        check(full, "parse reports full arrays for a complete save");
        bool slotsOk = true;
        slotsOk &= dst.warpF[Idx::index(0, 0)]  == dotModular::WarpF{dotModular::TriangularWarp{0.3}};
        slotsOk &= dst.warpW[Idx::index(0, 0)]  == 0.25f;
        slotsOk &= dst.warpF[Idx::index(4, 2)]  == dotModular::WarpF{dotModular::KumaraswamyWarp{5.0, 25.8}};
        slotsOk &= dst.warpW[Idx::index(4, 2)]  == 1.f;
        slotsOk &= dst.warpF[Idx::index(7, 5)]  == dotModular::WarpF{dotModular::LogitNormalWarp{-0.5, 0.8}};
        slotsOk &= dst.warpF[Idx::index(2, 1)]  == dotModular::WarpF{dotModular::MobiusWarp{2.5}};
        slotsOk &= dst.warpF[Idx::index(9, 0)]  == dotModular::WarpF{dotModular::ReversedPowerWarp{3.0}};
        slotsOk &= dst.warpF[Idx::index(1, 3)]  == dotModular::WarpF{dotModular::TruncNormalWarp{0.4, 0.15}};
        slotsOk &= dst.warpF[Idx::index(11, 6)] == dotModular::WarpF{dotModular::PowerWarp{0.5}};
        slotsOk &= dst.warpF[Idx::index(13, 4)] == dotModular::WarpF{dotModular::LogitSigmoidWarp{1.5, 0.3}};
        check(slotsOk, "every parametric slot round-trips exactly (type + params)");
        bool wOk = dst.warpW[Idx::index(7, 5)] == 0.6f && dst.warpW[Idx::index(9, 0)] == 0.75f &&
                   dst.warpW[Idx::index(1, 3)] == 0.1f && dst.warpW[Idx::index(11, 6)] == 0.9f &&
                   dst.warpW[Idx::index(13, 4)] == 0.35f && dst.warpW[Idx::index(5, 0)] == 0.8f &&
                   dst.warpW[Idx::index(6, 0)] == 0.2f;
        check(wOk, "every depth w round-trips exactly (float→json→float)");
        // untouched slots stay identity + w=0 (the default survives a round-trip)
        bool untouched = true;
        for (int i = 0; i < dotModular::WarpBank::N; ++i) {
            if (i == Idx::index(0,0) || i == Idx::index(4,2) || i == Idx::index(7,5) ||
                i == Idx::index(2,1) || i == Idx::index(9,0) || i == Idx::index(1,3) ||
                i == Idx::index(11,6) || i == Idx::index(13,4) || i == Idx::index(5,0) ||
                i == Idx::index(6,0)) continue;
            if (!std::holds_alternative<dotModular::IdentityWarp>(dst.warpF[i])) untouched = false;
            if (dst.warpW[i] != 0.f) untouched = false;
        }
        check(untouched, "untouched slots stay IdentityWarp + w=0 after the round-trip");
        // drawn: BOTH slots keep the ONE pool slot; the curve is point-identical
        const dotModular::DrawnWarp d1 = std::get<dotModular::DrawnWarp>(dst.warpF[Idx::index(5, 0)]);
        const dotModular::DrawnWarp d2 = std::get<dotModular::DrawnWarp>(dst.warpF[Idx::index(6, 0)]);
        check(d1 == d2 && d1.idx == (uint16_t)pc1, "shared drawn slots keep ONE pool slot, keyed by index");
        bool ptsEq = dst.poolSize() == 1;
        if (ptsEq) {
            const auto& a = dst.poolCurve(d1.idx).getPoints();
            const auto& b = c1.getPoints();
            if (a.size() != b.size()) ptsEq = false;
            else for (size_t i = 0; i < a.size(); ++i)
                if (a[i].x != b[i].x || a[i].y != b[i].y) ptsEq = false;
        }
        check(ptsEq, "the pooled MonotoneCurve round-trips point-exact (.dmcurve JSON)");
        check(dst.globalWarpF[2] == dotModular::WarpF{dotModular::PowerWarp{5.0}} &&
              dst.globalWarpW[2] == 0.5f,
              "the global (delegation default) slice round-trips exactly");
        // second generation is byte-identical (lossless — types, params, w, pool, nulls)
        {
            json_t* r1 = json_object();
            dotModular::WarpBank::serialize(src, r1, "editor");
            char* t1 = json_dumps(r1, JSON_COMPACT);
            json_t* r2 = json_object();
            dotModular::WarpBank::serialize(dst, r2, "editor");
            char* t2 = json_dumps(r2, JSON_COMPACT);
            check(t1 && t2 && std::strcmp(t1, t2) == 0,
                  "save→load→save is byte-identical (lossless round-trip)");
            std::free(t1); std::free(t2);
            json_decref(r1); json_decref(r2);
        }

        // NO warp keys at all (a pre-warp patch): everything loads at the defaults.
        json_t* bare = json_object();
        dotModular::WarpBank fresh;
        fresh.warpF[Idx::index(3, 3)] = dotModular::PowerWarp{5.0};
        fresh.warpW[Idx::index(3, 3)] = 1.f;
        const bool fullBare = dotModular::WarpBank::parse(bare, "editor", fresh);
        json_decref(bare);
        check(!fullBare, "a patch with no warp keys reports not-full");
        bool allId = true, allW0 = true;
        for (int i = 0; i < dotModular::WarpBank::N; ++i) {
            if (!std::holds_alternative<dotModular::IdentityWarp>(fresh.warpF[i])) allId = false;
            if (fresh.warpW[i] != 0.f) allW0 = false;
        }
        check(allId && allW0, "bank with NO stored keys loads all-identity + w=0 (old patches unchanged)");
        check(fresh.poolSize() == 0, "no pool entries without a stored pool");

        // SHORT arrays: the present head survives; the missing tail defaults.
        json_t* part = json_object();
        json_t* jw = json_array();
        json_array_append_new(jw, json_real(0.5));                          // 1 of 112
        json_object_set_new(part, "editorWarpW", jw);
        json_t* jf = json_array();
        json_array_append_new(jf, dotModular::warpToJson(dotModular::PowerWarp{2.0}));   // 1 of 112
        json_object_set_new(part, "editorWarpF", jf);
        dotModular::WarpBank partDst;
        const bool fullPart = dotModular::WarpBank::parse(part, "editor", partDst);
        json_decref(part);
        check(!fullPart, "short arrays report not-full");
        check(partDst.warpW[0] == 0.5f &&
              std::holds_alternative<dotModular::PowerWarp>(partDst.warpF[0]) &&
              std::get<dotModular::PowerWarp>(partDst.warpF[0]).k == 2.0,
              "short array: present head entries survive");
        check(partDst.warpW[1] == 0.f &&
              std::holds_alternative<dotModular::IdentityWarp>(partDst.warpF[1]),
              "short array: missing tail defaults (w=0, identity)");

        // A corrupt drawn ref (pool index out of range) loads identity — the safe default.
        json_t* bad = json_object();
        json_t* jf2 = json_array();
        json_t* drawnBad = json_object();
        json_object_set_new(drawnBad, "type", json_string("drawn"));
        json_object_set_new(drawnBad, "pool", json_integer(999));
        json_array_append_new(jf2, drawnBad);
        json_object_set_new(bad, "editorWarpF", jf2);
        dotModular::WarpBank badDst;
        dotModular::WarpBank::parse(bad, "editor", badDst);
        json_decref(bad);
        check(std::holds_alternative<dotModular::IdentityWarp>(badDst.warpF[0]),
              "out-of-range pool ref loads identity (safe default)");
    }

    std::printf("[S7] MONOTONE (Sklar-safety): strictly increasing in double AND stored float32\n");
    {
        // The four VERIFIED cases (CC_PROMPT_02 — pre-computed, not estimated: ZERO
        // inversions AND ZERO ties over a 10,000-pt grid at these w in float32). The
        // conc=5 Kumaraswamy is the STEEPEST curve the editor allows — the binding case
        // (it passes ⟹ the gentler curves do too).
        double ka = 0.0, kb = 0.0;
        dotModular::KumaraswamyDistribution::abFromModeConc(0.5, 5.0, ka, kb);
        check(ka == 5.0 && kb == 25.8, "the conc=5 case is exactly (a=5, b=25.8) via abFromModeConc");

        struct Case { const char* name; dotModular::WarpF f; };
        const Case cases[] = {
            {"IdentityWarp (the default)",                 dotModular::IdentityWarp{}},
            {"PowerWarp(k=5) — steep near 0",              dotModular::PowerWarp{5.0}},
            {"KumaraswamyWarp(5,25.8) — conc=5 steepest",  dotModular::KumaraswamyWarp{ka, kb}},
            {"ReversedPowerWarp(theta=5) — steep near 1",  dotModular::ReversedPowerWarp{5.0}},
        };
        const float ws[3] = {0.f, 0.5f, 1.f};
        const int N = 10000;
        for (const Case& c : cases) {
            for (float w : ws) {
                dotModular::WarpBank bank;
                bank.warpF[dotModular::WarpBank::index(8, 3)] = c.f;   // fixed (v,lane)
                bank.warpW[dotModular::WarpBank::index(8, 3)] = w;
                bool monoD = true, monoF = true;
                double prevD = bank.warpedValue(8, 3, 0.0);
                float  prevF = (float)prevD;
                for (int i = 1; i < N; ++i) {
                    const double u = (double)i / (double)(N - 1);
                    const double d = bank.warpedValue(8, 3, u);
                    const float  f = (float)d;                          // matching warped_
                    if (!(prevD < d)) monoD = false;   // STRICT: no inversions, no ties
                    if (!(prevF < f)) monoF = false;
                    prevD = d; prevF = f;
                }
                char buf[128];
                std::snprintf(buf, sizeof buf, "%s at w=%g: strictly increasing (double)", c.name, (double)w);
                check(monoD, buf);
                std::snprintf(buf, sizeof buf, "%s at w=%g: strictly increasing (stored float32)", c.name, (double)w);
                check(monoF, buf);
            }
        }
    }

    std::printf("%s: %d passed, %d failed\n", (g_fail == 0 ? "ALL OK" : "FAILURES"), g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
