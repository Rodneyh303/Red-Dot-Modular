// test_resolver_warp_bridge.cpp — Resolver STEP 2's BYTE-IDENTICAL gate (warp).
//
// PURPOSE: WarpBank's mix/effective members now route through
// SandsModifierResolver<WarpPolicy> (the hand-rolled `if (delegated)`
// hand-copy in WarpF.hpp is DELETED; the definitions live out-of-line in
// SandsModifierResolver.hpp). This test is the PERMANENT PIN: a seeded,
// deterministic fuzz over banks that asserts the RESOLVER-ROUTED bank is
// BIT-IDENTICAL to the OLD code — duplicated below VERBATIM from the
// pre-step-2 src/dsp/WarpF.hpp (tests may duplicate; production may not):
// the 2-arg mix, the 4-line delegated mix, and the effectiveF/effectiveW
// selection. Same BITS, not |a-b|<eps: memcmp on the IEEE754 words for
// warpedValue (double) and effectiveW (float); effectiveF pinned by variant
// ALTERNATIVE + value equality. Step 1's test_resolver_spread_bridge is the
// template; steps 3-4 (LOR/direction) extend the same pattern.
//
// DORMANT-GENERAL-FORM note: WarpPolicy::applyDelegated is {mb.f,
// clamp01(mb.w + td)} with td ≡ 0 today (no warp send/tap plumbing exists —
// resolveWarpCell wires no pre/post deltas), so the resolver's output is PURE
// SELECTION and this pin proves it stayed bit-identical. When warp grows
// CV/send/tap terms, the wiring changes and this oracle gets EXTENDED (the
// frozen copy below documents what must not silently drift until then).
//
// Case space (deterministic, cycling — not statistical): own/global (F, w)
// pairs over a 12-entry roster spanning EVERY alternative (Identity, all 8
// parametrics, 2 DrawnWarp pool refs) × w ∈ {0, 1, mid, random} ×
// u ∈ {0, 1, 1e-9, 1-1e-9, random} × delegated ∈ {false, true}, across all
// 112 (voice,lane) cells. Coverage counters are asserted at the end so the
// gate cannot silently rot.
//
// Header-only, Rack-free; registered in test/run_all.sh as
// "test_resolver_warp_bridge|" (the test_warp_f_bank compile shape).

#include <cstdint>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <variant>

#include "WarpF.hpp"                    // the bank (its last line pulls the resolver)
#include "SandsModifierResolver.hpp"    // the skeleton + WarpPolicy + the bank's definitions

using dotModular::WarpBank;
using redDot::WarpPolicy;
using redDot::WarpCell;
using Resolver = redDot::SandsModifierResolver<WarpPolicy>;

static int g_pass = 0, g_fail = 0;
#define SUITE(n) std::cout << "\n\033[1;34m[" << (n) << "]\033[0m\n"
#define TEST(desc, ...) do { try { __VA_ARGS__; \
    std::cout << "  \033[32mok\033[0m  " << desc << "\n"; ++g_pass; } \
    catch (const std::exception& e) { \
    std::cout << "  \033[31mFAIL\033[0m " << desc << "  — " << e.what() << "\n"; ++g_fail; } } while(0)

// ── compile-time pins: the concept BITES; the resolved cell is a flat value ───
static_assert(redDot::SandsModifierPolicy<WarpPolicy>,
              "WarpPolicy must satisfy the policy concept");
static_assert(std::is_trivially_copyable_v<WarpCell>,
              "WarpCell must be trivially copyable — it rides the same flat-bank "
              "contract as WarpF (WarpF trivial + float depth).");

// ── bit-identity (the whole point: same BITS, not close-enough) ──────────────
static bool sameBits(double a, double b) {
    static_assert(sizeof(double) == sizeof(uint64_t), "IEEE754 double expected");
    uint64_t ra, rb;
    std::memcpy(&ra, &a, sizeof ra);
    std::memcpy(&rb, &b, sizeof rb);
    return ra == rb;
}
static bool sameBits(float a, float b) {
    static_assert(sizeof(float) == sizeof(uint32_t), "IEEE754 single expected");
    uint32_t ra, rb;
    std::memcpy(&ra, &a, sizeof ra);
    std::memcpy(&rb, &b, sizeof rb);
    return ra == rb;
}
static std::string bits(double d) {
    uint64_t r; std::memcpy(&r, &d, sizeof r);
    std::ostringstream s; s << "0x" << std::hex << r << std::dec << " (" << d << ")";
    return s.str();
}
static std::string bits(float f) {
    uint32_t r; std::memcpy(&r, &f, sizeof r);
    std::ostringstream s; s << "0x" << std::hex << r << std::dec << " (" << f << "f)";
    return s.str();
}

// ══════════════════════════════════════════════════════════════════════════════
// THE ORACLE — the OLD WarpBank, duplicated VERBATIM from the pre-step-2
// WarpF.hpp: the 2-arg mix, the 4-line delegated branch, effectiveF/effectiveW.
// This copy is FROZEN — it must NOT be "improved" to call the new code; its
// whole value is being independent of it. (invCDF is the curve library —
// single-impl, pinned by test_warp_f_bank S3/S5; the DELEGATION ALGEBRA + MIX
// below is what this bridge pins. The old members called the bank's own
// invCDF forwarder, which passes this->pool — the same thing done here.)
// ══════════════════════════════════════════════════════════════════════════════
namespace oracle {

static double warpedValue(const WarpBank& b, int v, int lane, double u) {
    const int i = WarpBank::index(v, lane);
    return (1.0 - b.warpW[i]) * u + b.warpW[i] * dotModular::invCDF(b.warpF[i], u, b.pool);
}

static double warpedValue(const WarpBank& b, int v, int lane, double u, bool delegated) {
    if (delegated) {
        const double w = b.globalWarpW[lane];
        return (1.0 - w) * u + w * dotModular::invCDF(b.globalWarpF[lane], u, b.pool);
    }
    return warpedValue(b, v, lane, u);
}

static const dotModular::WarpF& effectiveF(const WarpBank& b, int v, int lane, bool delegated) {
    return delegated ? b.globalWarpF[lane] : b.warpF[WarpBank::index(v, lane)];
}
static float effectiveW(const WarpBank& b, int v, int lane, bool delegated) {
    return delegated ? b.globalWarpW[lane] : b.warpW[WarpBank::index(v, lane)];
}

} // namespace oracle

// THE BRIDGE CHECK: the resolver-routed bank vs the oracle —
//   (a) warpedValue(4-arg) BIT-IDENTICAL to the old delegated mix,
//   (a') warpedValue(2-arg) BIT-IDENTICAL to the old own-slot mix,
//   (b) the resolver's resolved (f, w) == the old effectiveF/effectiveW
//       selection (w same BITS; f same ALTERNATIVE + value).
// Throws with full repro data on any drift.
static void expectBridge(long caseIdx, const WarpBank& b, int v, int lane, double u, bool delegated) {
    const double wantMix = oracle::warpedValue(b, v, lane, u, delegated);
    const double gotMix  = b.warpedValue(v, lane, u, delegated);
    const bool okMix = sameBits(wantMix, gotMix);

    const bool okTwo = sameBits(oracle::warpedValue(b, v, lane, u), b.warpedValue(v, lane, u));

    const dotModular::WarpF& wantF = oracle::effectiveF(b, v, lane, delegated);
    const dotModular::WarpF  gotF  = b.effectiveF(v, lane, delegated);
    const bool okF = wantF.index() == gotF.index() && gotF == wantF;

    const float wantW = oracle::effectiveW(b, v, lane, delegated);
    const float gotW  = b.effectiveW(v, lane, delegated);
    const bool okW = sameBits(wantW, gotW);

    if (okMix && okTwo && okF && okW) return;
    std::ostringstream s;
    s << "case " << caseIdx << " (v=" << v << " lane=" << lane << " u=" << u
      << " delegated=" << delegated << "): "
      << "mix " << (okMix ? "ok" : "BITS DIFFER") << " (" << bits(gotMix) << " vs " << bits(wantMix) << ")"
      << ", 2-arg " << (okTwo ? "ok" : "BITS DIFFER")
      << ", effectiveF " << (okF ? "ok" : "DIFFERS")
      << ", effectiveW " << (okW ? "ok" : "BITS DIFFER") << " (" << bits(gotW) << " vs " << bits(wantW) << ")"
      << " | own {w " << b.warpW[WarpBank::index(v, lane)] << ", "
      << dotModular::warpTypeName(b.warpF[WarpBank::index(v, lane)]) << "}"
      << ", global {w " << b.globalWarpW[lane] << ", "
      << dotModular::warpTypeName(b.globalWarpF[lane]) << "}";
    throw std::runtime_error(s.str());
}

// ── deterministic RNG (splitmix64 — integer-only, identical on every platform) ──
struct Rng {
    uint64_t s;
    explicit Rng(uint64_t seed) : s(seed) {}
    uint64_t nextU64() {
        uint64_t z = (s += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }
    float uniformF(float lo, float hi) {   // 24-bit grid in [lo, hi); deterministic
        return lo + (hi - lo) * (float)(nextU64() >> 40) * (1.0f / 16777216.0f);
    }
    double uniform01() {                   // 53-bit grid in [0, 1); deterministic
        return (double)(nextU64() >> 11) * (1.0 / 9007199254740992.0);
    }
};

static constexpr uint64_t kSeed = 0x7A2E5EED5EED5EEDull;   // "warp-seed" — fixed, printed
static constexpr long    kCases = 120000;                  // ≥100k per the gate spec

int main() {
    SUITE("Resolved cell surface (the policy + the default state)");
    TEST("WarpCell default-constructs to the no-warp state (IdentityF, w=0)", {
        const WarpCell c{};
        if (!std::holds_alternative<dotModular::IdentityWarp>(c.f))
            throw std::runtime_error("default WarpCell.f is not IdentityWarp");
        if (!sameBits(c.w, 0.f))
            throw std::runtime_error("default WarpCell.w is not 0.f");
    });
    TEST("default bank: warpedValue == u BIT-EXACT for every u, both arities + both delegated", {
        WarpBank bank;   // all-Identity, w=0, globals identity@0 — the 02-shipped state
        for (int i = 0; i <= 200; ++i) {
            const double u = (double)i / 200.0;
            if (!sameBits(bank.warpedValue(0, 0, u), u)
                || !sameBits(bank.warpedValue(15, 6, u), u)
                || !sameBits(bank.warpedValue(15, 6, u, false), u)
                || !sameBits(bank.warpedValue(15, 6, u, true), u))   // global identity@0 too
                throw std::runtime_error("default bank mix is not bit-exactly u");
        }
        if (bank.effectiveW(3, 2, false) != 0.f || bank.effectiveW(3, 2, true) != 0.f)
            throw std::runtime_error("default resolved depth is not 0");
        if (!std::holds_alternative<dotModular::IdentityWarp>(bank.effectiveF(3, 2, false))
            || !std::holds_alternative<dotModular::IdentityWarp>(bank.effectiveF(3, 2, true)))
            throw std::runtime_error("default resolved curve is not IdentityWarp");
    });

    SUITE("Named anchors — the resolver-routed bank vs the old formulas (exact)");
    TEST("test_warp_f_bank S4c literals: Power(5)@0.7 own vs RevPower(5)@0.25 global @ u=0.42", {
        WarpBank bank;
        bank.warpF[WarpBank::index(2, 3)] = dotModular::PowerWarp{5.0};
        bank.warpW[WarpBank::index(2, 3)] = 0.7f;
        bank.globalWarpF[3] = dotModular::ReversedPowerWarp{5.0};
        bank.globalWarpW[3] = 0.25f;
        expectBridge(0, bank, 2, 3, 0.42, false);
        expectBridge(1, bank, 2, 3, 0.42, true);
        if (!std::holds_alternative<dotModular::PowerWarp>(bank.effectiveF(2, 3, false))
            || !std::holds_alternative<dotModular::ReversedPowerWarp>(bank.effectiveF(2, 3, true)))
            throw std::runtime_error("resolved curve does not select own vs global");
        if (bank.effectiveW(2, 3, false) != 0.7f || bank.effectiveW(2, 3, true) != 0.25f)
            throw std::runtime_error("resolved depth does not select own vs global");
    });
    TEST("w=1 endpoints: own slot == invCDF exactly; delegated global == invCDF exactly", {
        WarpBank bank;
        bank.warpF[WarpBank::index(4, 1)] = dotModular::KumaraswamyWarp{5.0, 25.8};
        bank.warpW[WarpBank::index(4, 1)] = 1.f;
        bank.globalWarpF[1] = dotModular::TriangularWarp{0.3};
        bank.globalWarpW[1] = 1.f;
        for (int i = 0; i <= 500; ++i) {
            const double u = (double)i / 500.0;
            if (!sameBits(bank.warpedValue(4, 1, u, false),
                          dotModular::invCDF(dotModular::KumaraswamyWarp{5.0, 25.8}, u, nullptr)))
                throw std::runtime_error("own w=1 is not exactly invCDF");
            if (!sameBits(bank.warpedValue(4, 1, u, true),
                          dotModular::invCDF(dotModular::TriangularWarp{0.3}, u, nullptr)))
                throw std::runtime_error("delegated global w=1 is not exactly invCDF");
        }
    });
    TEST("drawn curve via the pool: the resolver-routed mix == the pooled curve's own evalInverse", {
        WarpBank bank;
        const dotModular::MonotoneCurve cA =
            dotModular::MonotoneCurve::fromDistribution(dotModular::KumaraswamyDistribution(2, 3), 9);
        const int p0 = bank.putCurve(cA);
        bank.warpF[WarpBank::index(5, 2)] = dotModular::DrawnWarp{(uint16_t)p0};
        bank.warpW[WarpBank::index(5, 2)] = 1.f;
        bank.globalWarpF[2] = dotModular::DrawnWarp{(uint16_t)p0};
        bank.globalWarpW[2] = 1.f;
        for (int i = 0; i <= 500; ++i) {
            const double u = (double)i / 500.0;
            if (!sameBits(bank.warpedValue(5, 2, u, false), cA.evalInverse(u))
                || !sameBits(bank.warpedValue(5, 2, u, true), cA.evalInverse(u)))
                throw std::runtime_error("drawn slot at w=1 is not exactly the curve's inverse");
        }
        expectBridge(2, bank, 5, 2, 0.37, false);
        expectBridge(3, bank, 5, 2, 0.37, true);
    });

    SUITE("Seeded deterministic fuzz — the resolver-routed bank vs the OLD mix, same BITS");
    TEST("120000 cases: 12-F roster (own+global) x w{0,1,mid,rand} x u{edges+rand} x delegated x all 112 cells", {
        std::cout << "      seed: 0x" << std::hex << kSeed << std::dec
                  << "  cases: " << kCases << "\n";
        WarpBank bank;
        // Two distinct pool curves for the DrawnWarp roster entries (set-time, via
        // the bank's own commit path — putCurve dedups and owns poolUsed).
        const dotModular::MonotoneCurve cA =
            dotModular::MonotoneCurve::fromDistribution(dotModular::KumaraswamyDistribution(2, 3), 9);
        const dotModular::MonotoneCurve cB =
            dotModular::MonotoneCurve::fromDistribution(dotModular::PowerDistribution(0.4), 9);
        const int p0 = bank.putCurve(cA);
        const int p1 = bank.putCurve(cB);
        if (p0 != 0 || p1 != 1)
            throw std::runtime_error("pool setup: expected slots 0 and 1");

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
            dotModular::DrawnWarp{(uint16_t)p0},              // drawn via the pool
            dotModular::DrawnWarp{(uint16_t)p1},
        };
        const int R = (int)(sizeof roster / sizeof roster[0]);

        Rng rng(kSeed);
        long wBucket[4] = {0, 0, 0, 0};
        long rosterOwn[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
        long rosterGlob[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
        long delegatedCount = 0, uEdgeCount = 0;
        bool cellSeen[WarpBank::N] = {};
        long cellsSeen = 0;
        const double uEdges[4] = {0.0, 1.0, 1e-9, 1.0 - 1e-9};

        for (long i = 0; i < kCases; ++i) {
            // Cycle through every (voice,lane) cell (i%112 walks the whole bank).
            const int v = (int)(i % 16);
            const int lane = (int)((i / 16) % 7);
            if (!cellSeen[v * 7 + lane]) { cellSeen[v * 7 + lane] = true; ++cellsSeen; }

            // Own vs global roster entries at different phases (mostly distinct).
            const int ownR  = (int)(i % R);
            const int globR = (int)((i / R) % R);
            ++rosterOwn[ownR]; ++rosterGlob[globR];

            // w ∈ {0, 1, mid, random} — cycles per 16-case block; the mid bucket
            // uses distinct own/global mids so the pair isn't degenerate.
            const int ws = (int)((i >> 4) % 4);
            float wOwn, wGlob;
            switch (ws) {
                case 0:  wOwn = 0.f;  wGlob = 0.f;  break;
                case 1:  wOwn = 1.f;  wGlob = 1.f;  break;
                case 2:  wOwn = 0.5f; wGlob = 0.25f; break;
                default: wOwn = rng.uniformF(0.f, 1.f); wGlob = rng.uniformF(0.f, 1.f); break;
            }
            ++wBucket[ws];

            bank.warpF[WarpBank::index(v, lane)] = roster[ownR];
            bank.warpW[WarpBank::index(v, lane)] = wOwn;
            bank.globalWarpF[lane] = roster[globR];
            bank.globalWarpW[lane] = wGlob;

            // u: random in [0,1), with the four edges injected every 8th case.
            const bool edge = (i % 8) == 5;
            const double u = edge ? uEdges[(i >> 3) & 3] : rng.uniform01();
            if (edge) ++uEdgeCount;

            const bool delegated = (i & 1) != 0;
            if (delegated) ++delegatedCount;

            expectBridge(i, bank, v, lane, u, delegated);   // throws with the full dump
        }

        // Coverage MUST be total — the gate cannot silently rot.
        if (cellsSeen != WarpBank::N)
            throw std::runtime_error("not every (voice,lane) cell was exercised");
        for (int k = 0; k < 4; ++k)
            if (wBucket[k] <= 0) throw std::runtime_error("w bucket never exercised");
        for (int k = 0; k < R; ++k)
            if (rosterOwn[k] <= 0 || rosterGlob[k] <= 0)
                throw std::runtime_error("roster entry never exercised (own or global)");
        if (delegatedCount <= 0 || uEdgeCount <= 0)
            throw std::runtime_error("delegated / u-edge cases never exercised");
        std::cout << "      coverage: 112/112 cells, 4/4 w-buckets, 12/12 roster entries "
                  << "(own + global), u-edges:" << uEdgeCount
                  << ", delegated:" << delegatedCount << "\n";
        std::cout << "      fuzz cases: " << kCases << " (each checks the 4-arg mix, "
                  << "the 2-arg mix, effectiveF, effectiveW — " << kCases * 2
                  << " resolver resolutions)\n";
    });

    std::cout << "\n\033[1m" << g_pass << " passed, " << g_fail << " failed\033[0m\n";
    return g_fail ? 1 : 0;
}
