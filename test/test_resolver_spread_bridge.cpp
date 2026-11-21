// test_resolver_spread_bridge.cpp — Resolver STEP 1's BYTE-IDENTICAL gate.
//
// PURPOSE: SandsModifierResolver<SpreadPolicy> was extracted from SpreadResolver
// (the ONE tested delegation authority) as the FIRST policy of the shared
// per-(voice,lane) skeleton. This test is the PERMANENT PIN: a seeded,
// deterministic fuzz over 120,000 Inputs that asserts BOTH the new resolver AND
// the API-compat shell (SpreadResolver::effective) return floats BIT-IDENTICAL
// to the OLD inline formula — duplicated below VERBATIM from the pre-extraction
// src/dsp/SpreadResolver.hpp (tests may duplicate; production may not). Same
// BITS, not |a-b|<eps: memcmp on the IEEE754 words, so even a -0.0/+0.0 or
// last-ulp drift fails. Steps 2-4 (warp/LOR/direction) extend this same
// pattern for their policies — this file is the template.
//
// Coverage is DETERMINISTIC, not statistical: the four branch booleans
// (delegated, macroPresent, ownCv.connected, eastCv.connected) CYCLE through
// all 16 combinations via the case index's low bits, and macroTap cycles
// {0, 0.25, 0.5, 1, random} per 16-case block. Rails-and-beyond (base/macroBase
// ±2, full-depth ±atten) inject every 8th case; LARGE-THEN-OPPOSITE end-clamp
// pairs (intermediate overshoots ±1, net lands inside — the discriminator vs
// per-term clamping) inject every 16th. Coverage counters are asserted at the
// end so the gate cannot silently rot.
//
// Pure floats/bools in → floats out; header-only, builds and runs OUTSIDE Rack
// (no SDK needed). Registered in test/run_all.sh as "test_resolver_spread_bridge|".

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <type_traits>
#include "SpreadResolver.hpp"          // the compat shell (includes the authority)
#include "SandsModifierResolver.hpp"

using redDot::SpreadResolver;
using redDot::SpreadPolicy;
using redDot::CvTerm;
using Resolver = redDot::SandsModifierResolver<SpreadPolicy>;

static int g_pass = 0, g_fail = 0;
#define SUITE(n) std::cout << "\n\033[1;34m[" << (n) << "]\033[0m\n"
#define TEST(desc, ...) do { try { __VA_ARGS__; \
    std::cout << "  \033[32mok\033[0m  " << desc << "\n"; ++g_pass; } \
    catch (const std::exception& e) { \
    std::cout << "  \033[31mFAIL\033[0m " << desc << "  — " << e.what() << "\n"; ++g_fail; } } while(0)

// ── compile-time pins: the shell IS the resolver; the concept BITES ──────────
static_assert(std::is_same_v<SpreadResolver::Inputs, Resolver::Inputs>,
              "the shell's Inputs must BE the resolver's (an alias, not a copy)");
static_assert(std::is_same_v<SpreadResolver::CvTerm, CvTerm>,
              "the shell's CvTerm must BE the shared one (an alias, not a copy)");
static_assert(redDot::SandsModifierPolicy<SpreadPolicy>,
              "SpreadPolicy must satisfy the policy concept");
// A half-implemented policy (missing scaleCv/addCv/addSend/applyDelegated) must
// FAIL the concept — the located-error enforcement, not a comment. If this
// static_assert ever fires, the concept stopped biting.
struct BrokenPolicy {
    using Value = float;
    struct Extra {};
    static float constrain(float v, const Extra&) { return v; }
};
static_assert(!redDot::SandsModifierPolicy<BrokenPolicy>,
              "the concept must reject an incomplete policy");

// ── bit-identity (the whole point: same BITS, not close-enough) ──────────────
static bool sameBits(float a, float b) {
    static_assert(sizeof(float) == sizeof(uint32_t), "IEEE754 single expected");
    uint32_t ra, rb;
    std::memcpy(&ra, &a, sizeof ra);
    std::memcpy(&rb, &b, sizeof rb);
    return ra == rb;
}
static std::string bits(float f) {
    uint32_t r; std::memcpy(&r, &f, sizeof r);
    std::ostringstream s; s << "0x" << std::hex << r << " (" << f << "f)";
    return s.str();
}

// ══════════════════════════════════════════════════════════════════════════════
// THE ORACLE — the OLD SpreadResolver, duplicated VERBATIM from the
// pre-extraction header: CvTerm with the ×2 STILL INSIDE contribution(), the
// 5-line effective(), tappedDelta, clampSpread. This copy is FROZEN — it must
// NOT be "improved" to call the new code; its whole value is being independent
// of it. (kBipolarSpan is read through the shell's alias — same 2.f, single
// definition in SpreadPolicy.)
// ══════════════════════════════════════════════════════════════════════════════
namespace oracle {

struct CvTerm {
    bool  connected = false;
    float unitVoltage = 0.f;
    float atten       = 0.f;
    float contribution() const { return connected ? unitVoltage * atten * SpreadResolver::kBipolarSpan : 0.f; }
};

struct Inputs {
    float base = 0.f;
    bool  delegated = false;
    bool  macroPresent = false;
    CvTerm ownCv;
    CvTerm eastCv;
    float macroBase = 0.f;
    float macroSend = 0.f;
    float macroPreDelta  = 0.f;
    float macroPostDelta = 0.f;
    float macroTap       = 1.f;
};

static float tappedDelta(const Inputs& in) {
    return in.macroPreDelta + (in.macroPostDelta - in.macroPreDelta) * in.macroTap;
}

static float clampSpread(float v) { return std::max(-1.f, std::min(1.f, v)); }

static float effective(const Inputs& in) {
    if (in.delegated) {
        return clampSpread(in.macroBase + tappedDelta(in));
    }
    float eff = in.base;
    if (in.ownCv.connected)  eff += in.ownCv.contribution();
    if (in.eastCv.connected) eff += in.eastCv.contribution();
    if (in.macroPresent)     eff += tappedDelta(in) * in.macroSend;
    return clampSpread(eff);
}

} // namespace oracle

// ── one field bundle → BOTH input shapes (identical values, by construction) ──
struct Bundle {
    bool  delegated = false, macroPresent = false;
    bool  ownConn = false, eastConn = false;
    float base = 0.f, ownV = 0.f, ownA = 0.f, eastV = 0.f, eastA = 0.f;
    float macroBase = 0.f, macroSend = 0.f;
    float pre = 0.f, post = 0.f, tap = 1.f;
};

static oracle::Inputs toOracle(const Bundle& b) {
    oracle::Inputs o;
    o.delegated = b.delegated;  o.macroPresent = b.macroPresent;
    o.ownCv  = {b.ownConn,  b.ownV,  b.ownA};
    o.eastCv = {b.eastConn, b.eastV, b.eastA};
    o.base = b.base;  o.macroBase = b.macroBase;  o.macroSend = b.macroSend;
    o.macroPreDelta = b.pre;  o.macroPostDelta = b.post;  o.macroTap = b.tap;
    return o;
}
static Resolver::Inputs toProduction(const Bundle& b) {
    Resolver::Inputs n;   // extra{} default-constructs (spread's Extra is empty)
    n.delegated = b.delegated;  n.macroPresent = b.macroPresent;
    n.ownCv  = {b.ownConn,  b.ownV,  b.ownA};
    n.eastCv = {b.eastConn, b.eastV, b.eastA};
    n.base = b.base;  n.macroBase = b.macroBase;  n.macroSend = b.macroSend;
    n.macroPreDelta = b.pre;  n.macroPostDelta = b.post;  n.macroTap = b.tap;
    return n;
}

static std::string dump(const Bundle& b) {
    std::ostringstream s;
    s << "delegated=" << b.delegated << " macroPresent=" << b.macroPresent
      << " ownCv={conn " << b.ownConn << ", uV " << b.ownV << ", att " << b.ownA << "}"
      << " eastCv={conn " << b.eastConn << ", uV " << b.eastV << ", att " << b.eastA << "}"
      << " base=" << b.base << " macroBase=" << b.macroBase << " macroSend=" << b.macroSend
      << " pre=" << b.pre << " post=" << b.post << " tap=" << b.tap;
    return s.str();
}

// THE BRIDGE CHECK: oracle vs new resolver vs compat shell — all three must
// agree BIT for BIT (effective AND tappedDelta). Throws with full repro data.
static void expectBridge(long caseIdx, const Bundle& b, const char* what) {
    const oracle::Inputs o = toOracle(b);
    const Resolver::Inputs n = toProduction(b);
    const float want   = oracle::effective(o);
    const float gotNew = Resolver::effective(n);
    const float gotShl = SpreadResolver::effective(n);   // the compat shell
    const bool okNew = sameBits(want, gotNew);
    const bool okShl = sameBits(want, gotShl);
    const bool okTap = sameBits(oracle::tappedDelta(o), Resolver::tappedDelta(n));
    if (okNew && okShl && okTap) return;
    std::ostringstream s;
    s << what << " case " << caseIdx << ": "
      << "resolver " << (okNew ? "ok" : "BITS DIFFER")
      << ", shell " << (okShl ? "ok" : "BITS DIFFER")
      << ", tappedDelta " << (okTap ? "ok" : "BITS DIFFER")
      << " — oracle " << bits(want)
      << ", resolver " << bits(gotNew)
      << ", shell " << bits(gotShl)
      << " | inputs: " << dump(b);
    throw std::runtime_error(s.str());
}

// Literal anchor (only used with dyadic-exact arithmetic): the shell's output
// must equal the hand value bit for bit — defense in depth so a corrupted
// ORACLE can't pass by agreeing with a corrupted resolver.
static void expectLiteral(const Bundle& b, float expect, const char* what) {
    const float got = SpreadResolver::effective(toProduction(b));
    if (!sameBits(got, expect)) {
        std::ostringstream s;
        s << what << ": expected " << bits(expect) << ", got " << bits(got);
        throw std::runtime_error(s.str());
    }
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
    float uniform(float lo, float hi) {   // 24-bit grid in [lo, hi); deterministic
        return lo + (hi - lo) * (float)(nextU64() >> 40) * (1.0f / 16777216.0f);
    }
};

static constexpr uint64_t kSeed  = 0x5EED5EED5EED5EEDull;
static constexpr long    kCases  = 120000;   // ≥100k per the gate spec

int main() {
    SUITE("Shell surface (names + constants keep resolving)");
    TEST("kSpreadLanes==4; kBipolarSpan is SpreadPolicy's 2.f, same bits", {
        if (SpreadResolver::kSpreadLanes != 4)
            throw std::runtime_error("kSpreadLanes drifted");
        if (!sameBits(SpreadResolver::kBipolarSpan, SpreadPolicy::kBipolarSpan)
            || !sameBits(SpreadResolver::kBipolarSpan, 2.f))
            throw std::runtime_error("kBipolarSpan is not the policy's single 2.f definition");
        if (!sameBits(SpreadResolver::clampSpread(1.5f), 1.f)
            || !sameBits(SpreadResolver::clampSpread(-1.5f), -1.f))
            throw std::runtime_error("clampSpread delegate drifted");
    });

    SUITE("Named anchors — literal outputs + oracle parity (dyadic-exact values)");
    TEST("base passes through unchanged, bit for bit", {
        Bundle b; b.base = 0.4f;
        expectBridge(0, b, "base only");
        expectLiteral(b, 0.4f, "base only");
    });
    TEST("full-depth CV (+1V unit, att 1) adds the full span reach → exactly 1.f", {
        Bundle b; b.ownConn = true; b.ownV = 0.5f; b.ownA = 1.f;
        expectBridge(0, b, "full-depth own CV");
        expectLiteral(b, 1.f, "full-depth own CV");
    });
    TEST("END-CLAMP: 0.5 +1.0 −1.0 → intermediate 1.5 overshoots, net 0.5 kept (per-term clamp would give 0.0)", {
        Bundle p; p.base = 0.5f; p.ownConn = true;  p.ownV = 0.5f; p.ownA = 1.f;   // +1.0
        p.eastConn = true; p.eastV = 0.5f; p.eastA = -1.f;                          // −1.0
        expectBridge(0, p, "end-clamp pair (+)");
        expectLiteral(p, 0.5f, "end-clamp pair (+)");
        Bundle m; m.base = -0.5f; m.ownConn = true; m.ownV = 0.5f; m.ownA = -1.f;   // −1.0
        m.eastConn = true; m.eastV = 0.5f; m.eastA = 1.f;                           // +1.0
        expectBridge(0, m, "end-clamp pair (−)");
        expectLiteral(m, -0.5f, "end-clamp pair (−)");
    });
    TEST("delegated: clamp(macroBase + tappedDelta), own base/CV ignored", {
        Bundle b; b.delegated = true; b.base = 0.9f; b.ownConn = true; b.ownV = 0.5f; b.ownA = 1.f;
        b.macroBase = 0.3f; b.pre = 0.2f; b.post = 0.2f; b.tap = 0.25f;   // td=0.2 → 0.5
        expectBridge(0, b, "delegated");
        expectLiteral(b, 0.5f, "delegated");
    });
    TEST("sum beyond ±1 clamps to exactly ±1", {
        Bundle hi; hi.base = 0.8f; hi.ownConn = true; hi.ownV = 0.5f; hi.ownA = 1.f;    // → 1.8
        expectBridge(0, hi, "clamp +");
        expectLiteral(hi, 1.f, "clamp +");
        Bundle lo; lo.base = -0.8f; lo.ownConn = true; lo.ownV = 0.5f; lo.ownA = -1.f;  // → −1.8
        expectBridge(0, lo, "clamp −");
        expectLiteral(lo, -1.f, "clamp −");
    });
    TEST("macro send adds tappedDelta·send when present; absent macro adds nothing", {
        Bundle m; m.base = 0.2f; m.macroPresent = true; m.pre = 0.5f; m.post = 0.5f; m.macroSend = 0.4f;
        expectBridge(0, m, "macro send present");          // 0.2 + 0.5·0.4 = 0.4
        expectLiteral(m, 0.4f, "macro send present");
        Bundle a; a.base = 0.2f; a.macroPresent = false; a.pre = 0.5f; a.post = 0.5f; a.macroSend = 0.4f;
        expectBridge(0, a, "macro absent");                // deltas set but skipped
        expectLiteral(a, 0.2f, "macro absent");
    });
    TEST("composite: base + ownCv + eastCv + tappedSend → exactly 0.5f", {
        Bundle b; b.base = 0.1f;
        b.ownConn = true;  b.ownV = 0.05f; b.ownA = 1.f;    // +0.1
        b.eastConn = true; b.eastV = 0.05f; b.eastA = 1.f;  // +0.1
        b.macroPresent = true; b.pre = 0.4f; b.post = 0.4f; b.macroSend = 0.5f;  // +0.2
        expectBridge(0, b, "composite");
        expectLiteral(b, 0.5f, "composite");
    });

    SUITE("Seeded deterministic fuzz — oracle vs resolver vs shell, same BITS");
    TEST("120000 cases: every branch combo, tap bucket, rails-and-beyond, end-clamp pairs", {
        std::cout << "      seed: 0x" << std::hex << kSeed << std::dec
                  << "  cases: " << kCases << "\n";
        Rng rng(kSeed);
        long combos[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
        long tapBucket[5] = {0,0,0,0,0};
        long endClampPos = 0, endClampNeg = 0, railBeyond = 0, delegatedCount = 0;
        for (long i = 0; i < kCases; ++i) {
            Bundle b;
            // Branch booleans CYCLE via the case index's low bits → all 16
            // combinations are hit deterministically (16 per 16 cases).
            b.delegated    = (i & 1) != 0;
            b.macroPresent = (i & 2) != 0;
            b.ownConn      = (i & 4) != 0;
            b.eastConn     = (i & 8) != 0;
            ++combos[(b.delegated ? 8 : 0) | (b.macroPresent ? 4 : 0)
                     | (b.ownConn ? 2 : 0) | (b.eastConn ? 1 : 0)];
            if (b.delegated) ++delegatedCount;

            // macroTap ∈ {0, 0.25, 0.5, 1, random} — cycles per 16-case block.
            const int tapSel = (int)((i >> 4) % 5);
            switch (tapSel) {
                case 0:  b.tap = 0.f;   break;
                case 1:  b.tap = 0.25f; break;
                case 2:  b.tap = 0.5f;  break;
                case 3:  b.tap = 1.f;   break;
                default: b.tap = rng.uniform(0.f, 1.f); break;
            }
            ++tapBucket[tapSel];

            // Values: random in-range, with RAILS-AND-BEYOND every 8th case
            // (base/macroBase/post ±2, full-depth ±atten) ...
            const bool rails = (i % 8) == 5;
            if (rails) ++railBeyond;
            if ((i % 16) == 11) {
                // ... and a LARGE-THEN-OPPOSITE end-clamp PAIR every 16th: the
                // intermediate overshoots ±1, the net lands INSIDE — end-clamp
                // keeps the net; a per-term clamp would pin and lose it.
                const bool pos = ((i >> 4) & 1) != 0;
                b.base = pos ? 0.9f : -0.9f;
                b.ownConn = true;   b.ownV = 1.f;  b.ownA = pos ? 0.8f : -0.8f;    // ±1.6
                b.eastConn = true; b.eastV = 1.f;  b.eastA = pos ? -0.9f : 0.9f;  // ∓1.8
            } else {
                b.base  = rails ? ((i & 16) ? 2.f : -2.f) : rng.uniform(-2.f, 2.f);
                b.ownV  = rails ? 1.f : rng.uniform(-1.2f, 1.2f);
                b.ownA  = rails ? ((i & 32) ? 1.f : -1.f) : rng.uniform(-1.f, 1.f);
                b.eastV = rng.uniform(-1.2f, 1.2f);
                b.eastA = rng.uniform(-1.f, 1.f);
            }
            b.macroBase = rails ? ((i & 64) ? 2.f : -2.f) : rng.uniform(-2.f, 2.f);
            // send contract is 0..1; every 64th case goes BEYOND for robustness
            // (oracle and resolver see identical inputs either way).
            b.macroSend = ((i % 64) == 33) ? rng.uniform(-0.5f, 1.5f) : rng.uniform(0.f, 1.f);
            b.pre  = rng.uniform(-2.f, 2.f);
            b.post = rails ? ((i & 128) ? 2.f : -2.f) : rng.uniform(-2.f, 2.f);

            expectBridge(i, b, "fuzz");   // throws with the full dump on any bit drift

            // Coverage accounting (free branch): did the PRE-clamp sum overshoot?
            if (!b.delegated) {
                float sum = b.base;
                if (b.ownConn)  sum += b.ownV * b.ownA * 2.f;
                if (b.eastConn) sum += b.eastV * b.eastA * 2.f;
                if (b.macroPresent) sum += (b.pre + (b.post - b.pre) * b.tap) * b.macroSend;
                if (sum > 1.f)  ++endClampPos;
                if (sum < -1.f) ++endClampNeg;
            }
        }
        // Coverage MUST be total — the gate cannot silently rot.
        for (int c = 0; c < 16; ++c)
            if (combos[c] <= 0) throw std::runtime_error("branch combination never exercised");
        for (int t = 0; t < 5; ++t)
            if (tapBucket[t] <= 0) throw std::runtime_error("tap bucket never exercised");
        if (endClampPos <= 0 || endClampNeg <= 0)
            throw std::runtime_error("no overshoot-beyond-±1 case exercised");
        if (railBeyond <= 0 || delegatedCount <= 0)
            throw std::runtime_error("rails-and-beyond / delegated cases never exercised");
        std::cout << "      coverage: 16/16 branch combos, 5/5 tap buckets, "
                  << "end-clamp +:" << endClampPos << " −:" << endClampNeg
                  << ", rails:" << railBeyond << ", delegated:" << delegatedCount << "\n";
    });

    std::cout << "\n\033[1m" << g_pass << " passed, " << g_fail << " failed\033[0m\n";
    return g_fail ? 1 : 0;
}
