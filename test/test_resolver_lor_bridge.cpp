// test_resolver_lor_bridge.cpp — Resolver STEP 3's BYTE-IDENTICAL gate (LOR).
//
// PURPOSE: MonsoonExpanderManager's combineLOR — the design doc's "near-verbatim
// DUPLICATE (15 send/tap refs). CORRECT but duplicated" — now routes through
// SandsModifierResolver<LorPolicy> via redDot::resolveLorItem: the ownership
// ternary, the P9b tap-lerp send blend, the end-clamp and the (int) truncation
// moved to the header (SandsModifierResolver.hpp); the manager lambda keeps
// only the port/store reads. This test is the PERMANENT PIN: a seeded,
// deterministic fuzz over the moved algebra's inputs that asserts the
// RESOLVER-ROUTED result is BIT-IDENTICAL to the OLD code — duplicated below
// VERBATIM from the pre-step-3 MonsoonExpanderManager.cpp (tests may
// duplicate; production may not). BOTH layers are pinned: the final INT
// (equality — the value every call site stores) AND the PRE-CAST float
// (same BITS, via redDot::resolveLorValue — the layer the (int) truncation
// consumes), so neither the algebra nor the cast can silently drift.
//
// WHAT THIS BRIDGES — and what it does not: the MOVED algebra only (ternary /
// tap lerp / send blend / end-clamp / cast). eastLorVal does NOT move (its
// CV-span math is interleaved with module port reads) and is NOT bridged — it
// stays manager-verbatim under the plugin-build gate, the same way the
// spread/warp wiring lived under step 1/2's plugin builds. The 15 call sites
// keep their (lo,hi) literals; both windows (1,16) and (0,15) are exercised
// here. The shared tap-lerp contract itself is pinned by test_send_tap
// (unchanged); this bridge pins that LOR's resolver use of it reproduces the
// old direct StraitsSandsMacroVisual::tappedDelta call bit-exactly (the
// skeleton's lerp and the Visual's are the same expression,
// pre + (post−pre)·tap).
//
// Case space (deterministic, cycling — not statistical): all 4
// ownerEast×macroPresent combos (incl. the no-Macro fallback), eastVal ∈ {lo,
// hi, mid, random-in-window}, macroBase ∈ {random, lo, hi}, macroCvDelta ∈
// {+0, −0, ±random, ±large out-of-window}, send ∈ {0, 1, mid, random}, tap ∈
// {0, 1, mid, random}, both windows. Coverage counters are asserted at the
// end so the gate cannot silently rot.
//
// Header-only, Rack-free; registered in test/run_all.sh as
// "test_resolver_lor_bridge|" (the resolver-bridge compile shape).

#include <cstdint>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "SandsModifierResolver.hpp"    // the skeleton + LorPolicy + resolveLorItem/resolveLorValue

static int g_pass = 0, g_fail = 0;
#define SUITE(n) std::cout << "\n\033[1;34m[" << (n) << "]\033[0m\n"
#define TEST(desc, ...) do { try { __VA_ARGS__; \
    std::cout << "  \033[32mok\033[0m  " << desc << "\n"; ++g_pass; } \
    catch (const std::exception& e) { \
    std::cout << "  \033[31mFAIL\033[0m " << desc << "  — " << e.what() << "\n"; ++g_fail; } } while(0)

// ── compile-time pin: the concept BITES ──────────────────────────────────────
static_assert(redDot::SandsModifierPolicy<redDot::LorPolicy>,
              "LorPolicy must satisfy the policy concept");

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
    std::ostringstream s; s << "0x" << std::hex << r << std::dec << " (" << f << "f)";
    return s.str();
}

// One factored combineLOR invocation: the port parts (eastLorVal's result, the
// send/tap store reads, the Visual's published pre/post endpoints) become plain
// inputs; (lo,hi) is the per-item window every call site passes literally
// (LEN {1,16}, OFF/ROT {0,15}).
struct LorCase {
    bool  ownerEast = false, macroPresent = false;
    float eastVal = 0.f;                 // the CALLER-computed eastLorVal result
    float macroBase = 0.f, macroCvDelta = 0.f;
    float send = 0.f, pre = 0.f, post = 0.f, tap = 1.f;
    float lo = 1.f, hi = 16.f;
};

// The NEW side (the header helpers under test), factored for the anchors.
static int resolveI(const LorCase& c) {
    return redDot::resolveLorItem(c.ownerEast, c.macroPresent, c.eastVal,
                                  c.macroBase, c.macroCvDelta, c.send,
                                  c.pre, c.post, c.tap, c.lo, c.hi);
}
static float resolveF(const LorCase& c) {
    return redDot::resolveLorValue(c.ownerEast, c.macroPresent, c.eastVal,
                                   c.macroBase, c.macroCvDelta, c.send,
                                   c.pre, c.post, c.tap, c.lo, c.hi);
}

// ══════════════════════════════════════════════════════════════════════════════
// THE ORACLE — the OLD combineLOR body, duplicated VERBATIM from the
// pre-step-3 MonsoonExpanderManager.cpp (the algebra between the ownership
// read and the return), with the port reads factored to inputs. This copy is
// FROZEN — it must NOT be "improved" to call the new code; its whole value is
// being independent of it.
// ══════════════════════════════════════════════════════════════════════════════
namespace oracle {

// rack::math::clamp, VERBATIM (Rack v2 include/math.hpp — the test is
// Rack-free, so the exact definition the old code called is inlined):
//   template <typename T> T clamp(T x, T min, T max) {
//       return std::min(std::max(x, min), max);
//   }
static float clamp(float x, float lo, float hi) {
    return std::min(std::max(x, lo), hi);
}

// The pre-cast float of the old body (the layer the (int) cast consumed).
static float value(const LorCase& c) {
    float base = (c.ownerEast || !c.macroPresent) ? c.eastVal
                       : (c.macroBase + c.macroCvDelta);
    float blend = 0.f;
    if (c.macroPresent && c.ownerEast) {
        float send = c.send;        // factored: mmOwn ? mmOwn->getMacroSend(slot, lane, item) : 0.f
        const float tap = c.tap;    // factored: mmOwn ? mmOwn->getMacroTap(slot, lane, (item == 3 ? 1 : 0)) : 1.f
        // old: blend = macroVis->tappedDelta(tap, lane, item) * send;
        //      tappedDelta's body is pre + (post - pre) * tap (bit-identical to the
        //      resolver's lerp) — inlined over the factored published endpoints.
        blend = (c.pre + (c.post - c.pre) * tap) * send;
    }
    return clamp(base + blend, c.lo, c.hi);
}

// The old final int — (int) is TRUNCATION toward zero, after the clamp.
static int combine(const LorCase& c) { return (int)value(c); }

} // namespace oracle

// THE BRIDGE CHECK: the resolver-routed result vs the oracle —
//   (a) the final int EQUAL (the value the 15 call sites store), and
//   (b) the PRE-CAST float same-BITS (the algebra layer under the cast).
// Throws with full repro data on any drift.
static void expectBridge(long caseIdx, const LorCase& c, const char* tag = "fuzz") {
    const int   wantI = oracle::combine(c);
    const float wantF = oracle::value(c);
    const int   gotI  = resolveI(c);
    const float gotF  = resolveF(c);
    if (gotI == wantI && sameBits(gotF, wantF)) return;
    std::ostringstream s;
    s << tag << " case " << caseIdx
      << " (ownerEast=" << c.ownerEast << " macroPresent=" << c.macroPresent
      << " east=" << c.eastVal << " mb=" << c.macroBase << " mcd=" << c.macroCvDelta
      << " send=" << c.send << " pre=" << c.pre << " post=" << c.post
      << " tap=" << c.tap << " window=(" << c.lo << "," << c.hi << ")): "
      << "int " << gotI << " vs " << wantI
      << ", float " << bits(gotF) << " vs " << bits(wantF);
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
};

static constexpr uint64_t kSeed = 0x10C5EED5EED5EEDull;   // "lor-seed" — fixed, printed
static constexpr long    kCases = 120000;                 // ≥100k per the gate spec

int main() {
    SUITE("LorPolicy surface (the concept + the cast split)");
    TEST("resolveLorItem == (int)resolveLorValue — the truncation lives in the int wrapper", {
        LorCase c; c.ownerEast = true; c.macroPresent = true;
        c.lo = 1.f; c.hi = 16.f; c.eastVal = 9.f;
        c.pre = -2.f; c.post = 3.f; c.send = 0.75f; c.tap = 0.5f;
        if ((int)resolveF(c) != resolveI(c))
            throw std::runtime_error("resolveLorItem must be the (int) of resolveLorValue");
        expectBridge(0, c, "surface");
    });

    SUITE("Named anchors (a)-(f) — the resolver wiring vs the OLD algebra (exact)");
    TEST("(a) the call-site windows: LEN (1,16) + OFF/ROT (0,15), all 4 ownerEast x macroPresent combos", {
        const float windows[2][2] = {{1.f, 16.f}, {0.f, 15.f}};
        long n = 0;
        for (int w = 0; w < 2; ++w)
            for (int combo = 0; combo < 4; ++combo) {
                LorCase c;
                c.ownerEast    = (combo & 1) != 0;
                c.macroPresent = (combo & 2) != 0;
                c.lo = windows[w][0];  c.hi = windows[w][1];
                c.eastVal      = 0.5f * (c.lo + c.hi);            // mid-window East value
                c.macroBase    = c.lo + 0.3f * (c.hi - c.lo);
                c.macroCvDelta = 0.25f * (c.hi - c.lo);
                c.send = 0.5f;  c.pre = -1.5f;  c.post = 2.25f;  c.tap = 0.75f;
                expectBridge(n++, c, "anchor-a");
            }
    });
    TEST("(b) delegated with macroBase+macroCvDelta outside [lo,hi] clamps (send/tap/pre/post ignored)", {
        LorCase c; c.ownerEast = false; c.macroPresent = true;
        c.eastVal = 3.f;                                   // ignored on the delegated path
        c.pre = 99.f; c.post = -99.f; c.send = 1.f; c.tap = 0.25f;   // must be IGNORED too
        long n = 0;
        const struct { float lo, hi, mb, mcd; int want; } rows[] = {
            { 1.f, 16.f, 20.f,  7.f, 16 },   // 27 -> 16
            { 0.f, 15.f, 18.f,  6.f, 15 },   // 24 -> 15
            { 1.f, 16.f,  8.f, 50.f, 16 },   // +large out-of-window delta -> 16
            { 0.f, 15.f,  7.f, 50.f, 15 },
            { 1.f, 16.f,  8.f, -50.f,  1 },  // -large out-of-window delta -> 1
            { 0.f, 15.f,  7.f, -50.f,  0 },
            { 1.f, 16.f, -4.f, -2.f,   1 },  // -6 -> 1
            { 0.f, 15.f, -4.f, -2.f,   0 },  // -6 -> 0
        };
        for (const auto& r : rows) {
            c.lo = r.lo; c.hi = r.hi; c.macroBase = r.mb; c.macroCvDelta = r.mcd;
            if (resolveI(c) != r.want) {
                std::ostringstream s;
                s << "delegated clamp (" << r.mb << "+" << r.mcd << ") in (" << r.lo
                  << "," << r.hi << "): expected " << r.want << " got " << resolveI(c);
                throw std::runtime_error(s.str());
            }
            expectBridge(n++, c, "anchor-b");
        }
    });
    TEST("(c) ownerEast && !macroPresent: eastVal passes through untouched, then end-clamps", {
        LorCase c; c.ownerEast = true; c.macroPresent = false;
        c.lo = 0.f; c.hi = 15.f;
        c.eastVal = 21.7f;                                  // above hi -> 15
        if (resolveI(c) != 15) throw std::runtime_error("east 21.7 must clamp to 15");
        expectBridge(0, c, "anchor-c");
        c.eastVal = 7.25f;                                  // in window: UNTOUCHED, then truncated
        if (resolveI(c) != 7) throw std::runtime_error("east 7.25 must resolve to 7");
        if (!sameBits(resolveF(c), 7.25f)) throw std::runtime_error("free no-Macro float must be exactly eastVal");
        expectBridge(1, c, "anchor-c");
        // send/tap/pre/post must be IGNORED without Macro (wild values, same result)
        c.send = 1.f; c.pre = 50.f; c.post = -50.f; c.tap = 0.5f;
        if (resolveI(c) != 7) throw std::runtime_error("no-Macro path must ignore the Macro terms");
        expectBridge(2, c, "anchor-c");
    });
    TEST("(d) pre==post tap-neutrality BIT-EXACT: any tap => the same resolved bits (free + delegated)", {
        const float taps[] = {0.f, 0.25f, 0.5f, 0.75f, 1.f};
        {   // free path, send != 0 so the lerp is LIVE
            LorCase c; c.ownerEast = true; c.macroPresent = true;
            c.lo = 0.f; c.hi = 15.f; c.eastVal = 6.f;
            c.pre = 2.5f; c.post = 2.5f; c.send = 1.f;
            float ref = 0.f; bool have = false; long n = 0;
            for (float t : taps) {
                c.tap = t;
                const float f = resolveF(c);
                if (have && !sameBits(f, ref))
                    throw std::runtime_error("free path: tap changed the resolved bits with pre==post");
                ref = f; have = true;
                expectBridge(n++, c, "anchor-d");
            }
            if (!sameBits(ref, oracle::clamp(6.f + 2.5f * 1.f, 0.f, 15.f)))
                throw std::runtime_error("pre==post lerp did not collapse to pre (6 + 2.5*1)");
        }
        {   // negative-zero endpoint: pre == post == -0.0f is ALSO tap-invariant
            LorCase c; c.ownerEast = true; c.macroPresent = true;
            c.lo = 0.f; c.hi = 15.f; c.eastVal = 6.f;
            c.pre = -0.0f; c.post = -0.0f; c.send = 1.f;
            float ref = 0.f; bool have = false; long n = 100;
            for (float t : taps) {
                c.tap = t;
                const float f = resolveF(c);
                if (have && !sameBits(f, ref))
                    throw std::runtime_error("free path: tap changed the bits with pre==post==-0.0");
                ref = f; have = true;
                expectBridge(n++, c, "anchor-d");
            }
        }
        {   // delegated path: the wiring's neutralized lerp (pre=post=macroCvDelta) —
            // the passed pre/post/send are IGNORED, the result is clamp(mb+mcd) for every tap
            LorCase c; c.ownerEast = false; c.macroPresent = true;
            c.lo = 1.f; c.hi = 16.f; c.eastVal = 3.f;
            c.macroBase = 7.f; c.macroCvDelta = 2.5f;
            c.pre = -40.f; c.post = 40.f; c.send = 1.f;   // wild: must be IGNORED
            float ref = 0.f; bool have = false; long n = 200;
            for (float t : taps) {
                c.tap = t;
                const float f = resolveF(c);
                if (have && !sameBits(f, ref))
                    throw std::runtime_error("delegated path: tap changed the resolved bits");
                ref = f; have = true;
                expectBridge(n++, c, "anchor-d");
            }
            if (!sameBits(ref, oracle::clamp(7.f + 2.5f, 1.f, 16.f)))
                throw std::runtime_error("delegated neutralized lerp did not collapse to mb+mcd");
        }
    });
    TEST("(e) truncation pin: resolved 15.999 -> 15 (round would say 16); -0.5 clamped to 0.0 -> 0", {
        LorCase c; c.ownerEast = true; c.macroPresent = false;
        c.lo = 1.f; c.hi = 16.f; c.eastVal = 15.999f;   // in (1,16): resolves to 15.999
        if (resolveI(c) != 15)
            throw std::runtime_error("(int) of a resolved 15.999 must be 15 — TRUNCATION, not round()16");
        if (!sameBits(resolveF(c), 15.999f))
            throw std::runtime_error("the pre-cast float must be exactly 15.999f (the un-truncated layer)");
        expectBridge(0, c, "anchor-e");
        c.lo = 0.f; c.hi = 15.f; c.eastVal = -0.5f;     // below lo -> clamped to 0.0 -> 0
        if (resolveI(c) != 0)
            throw std::runtime_error("clamp-then-trunc: -0.5 -> 0.0 -> 0 (round(-0.5) would be -1)");
        if (!sameBits(resolveF(c), 0.f))
            throw std::runtime_error("clamped -0.5 must be exactly +0.0f");
        expectBridge(1, c, "anchor-e");
    });
    TEST("(f) tap=0 vs tap=1 with pre!=post, send!=0 => DIFFERENT blends (the pin is not vacuous)", {
        LorCase c; c.ownerEast = true; c.macroPresent = true;
        c.lo = 0.f; c.hi = 15.f; c.eastVal = 7.f;
        c.pre = 0.f; c.post = 8.f; c.send = 1.f;
        c.tap = 0.f;
        const int i0 = resolveI(c); const float f0 = resolveF(c);
        expectBridge(0, c, "anchor-f");
        c.tap = 1.f;
        const int i1 = resolveI(c); const float f1 = resolveF(c);
        expectBridge(1, c, "anchor-f");
        if (sameBits(f0, f1))
            throw std::runtime_error("tap=0 and tap=1 produced the SAME bits — the blend layer is vacuous");
        if (i0 != 7 || i1 != 15)
            throw std::runtime_error("tap endpoints: expected 7 (pre endpoint) vs 15 (post endpoint)");
    });

    SUITE("Seeded deterministic fuzz — the resolver wiring vs the OLD algebra, both layers");
    TEST("120000 cases: 4 combos x both windows x east{lo,hi,mid,rand} x base{rand,lo,hi} x delta{+0,-0,rand,+lg,-lg} x send/tap buckets", {
        std::cout << "      seed: 0x" << std::hex << kSeed << std::dec
                  << "  cases: " << kCases << "\n";
        Rng rng(kSeed);

        long combo[4]  = {0,0,0,0};   // ownerEast x macroPresent (combo[0] = the no-Macro fallback)
        long win[2]    = {0,0};       // (1,16) vs (0,15)
        long eastB[4]  = {0,0,0,0};   // lo / hi / mid / random-in-window
        long baseB[3]  = {0,0,0};     // random / lo / hi
        long cvdB[5]   = {0,0,0,0,0}; // +0 / -0 / +-random / +large / -large
        long sendB[4]  = {0,0,0,0};   // 0 / 1 / mid / random
        long tapB[4]   = {0,0,0,0};   // 0 / 1 / mid / random
        long delegatedLargeClamp = 0; // Macro owns AND |delta| lands out-of-window

        for (long i = 0; i < kCases; ++i) {
            LorCase c;

            // ownership x presence: all 4 combos (period 4)
            c.ownerEast    = (i & 1) != 0;
            c.macroPresent = (i & 2) != 0;
            ++combo[(c.ownerEast ? 2 : 0) + (c.macroPresent ? 1 : 0)];

            // window: both call-site windows, alternating
            const bool lenWin = ((i >> 2) & 1) != 0;
            c.lo = lenWin ? 1.f  : 0.f;
            c.hi = lenWin ? 16.f : 15.f;
            ++win[lenWin ? 0 : 1];

            // eastVal in {lo, hi, mid, random-in-window}
            const int eb = (i >> 3) & 3;
            switch (eb) {
                case 0:  c.eastVal = c.lo; break;
                case 1:  c.eastVal = c.hi; break;
                case 2:  c.eastVal = 0.5f * (c.lo + c.hi); break;
                default: c.eastVal = rng.uniformF(c.lo, c.hi); break;
            }
            ++eastB[eb];

            // macroBase in {random around the window, lo, hi}
            const int bb = (int)((i >> 2) % 3);
            switch (bb) {
                case 0:  c.macroBase = rng.uniformF(c.lo - 5.f, c.hi + 5.f); break;
                case 1:  c.macroBase = c.lo; break;
                default: c.macroBase = c.hi; break;
            }
            ++baseB[bb];

            // macroCvDelta in {+0, -0, +-random, +large out-of-window, -large}
            const int db = (int)((i >> 2) % 5);
            switch (db) {
                case 0:  c.macroCvDelta = +0.0f; break;
                case 1:  c.macroCvDelta = -0.0f; break;
                case 2:  c.macroCvDelta = ((i & 4) != 0) ? rng.uniformF(0.f, 8.f) : -rng.uniformF(0.f, 8.f); break;
                case 3:  c.macroCvDelta =  c.hi + 10.f + rng.uniformF(0.f, 5.f); break;
                default: c.macroCvDelta = -(c.hi + 10.f + rng.uniformF(0.f, 5.f)); break;
            }
            ++cvdB[db];

            // send in {0, 1, mid, random}
            const int sb = (i >> 5) & 3;
            switch (sb) {
                case 0:  c.send = 0.f; break;
                case 1:  c.send = 1.f; break;
                case 2:  c.send = 0.5f; break;
                default: c.send = rng.uniformF(0.f, 1.f); break;
            }
            ++sendB[sb];

            // tap in {0, 1, mid, random}
            const int tb = (i >> 7) & 3;
            switch (tb) {
                case 0:  c.tap = 0.f; break;
                case 1:  c.tap = 1.f; break;
                case 2:  c.tap = 0.5f; break;
                default: c.tap = rng.uniformF(0.f, 1.f); break;
            }
            ++tapB[tb];

            // published endpoints: independent randoms over the +-window span;
            // every 16th case forces pre==post (tap-neutrality under fuzz)
            c.pre  = rng.uniformF(-(c.hi - c.lo), (c.hi - c.lo));
            c.post = rng.uniformF(-(c.hi - c.lo), (c.hi - c.lo));
            if ((i & 15) == 0) c.post = c.pre;

            if (!c.ownerEast && c.macroPresent && (c.macroCvDelta > c.hi || c.macroCvDelta < c.lo))
                ++delegatedLargeClamp;

            expectBridge(i, c);   // throws with the full dump
        }

        // Coverage MUST be total — the gate cannot silently rot.
        for (int k = 0; k < 4; ++k)
            if (combo[k] <= 0) throw std::runtime_error("ownerEast x macroPresent combo never exercised (incl. the no-Macro fallback)");
        for (int k = 0; k < 2; ++k)
            if (win[k] <= 0) throw std::runtime_error("call-site window never exercised");
        for (int k = 0; k < 4; ++k) {
            if (eastB[k] <= 0) throw std::runtime_error("eastVal bucket never exercised");
            if (sendB[k] <= 0) throw std::runtime_error("send bucket never exercised");
            if (tapB[k] <= 0)  throw std::runtime_error("tap bucket never exercised");
        }
        for (int k = 0; k < 3; ++k)
            if (baseB[k] <= 0) throw std::runtime_error("macroBase bucket never exercised");
        for (int k = 0; k < 5; ++k)
            if (cvdB[k] <= 0) throw std::runtime_error("macroCvDelta bucket never exercised");
        if (delegatedLargeClamp <= 0)
            throw std::runtime_error("+-large-delta delegated-clamp case never exercised");

        std::cout << "      coverage: combos E/M " << combo[0] << "/" << combo[1] << "/"
                  << combo[2] << "/" << combo[3] << " (no-Macro fallback first)"
                  << ", windows (1,16):" << win[0] << " (0,15):" << win[1]
                  << ", east {lo,hi,mid,rand} " << eastB[0] << "/" << eastB[1] << "/" << eastB[2] << "/" << eastB[3]
                  << ", base {rand,lo,hi} " << baseB[0] << "/" << baseB[1] << "/" << baseB[2]
                  << ", delta {+0,-0,rand,+lg,-lg} " << cvdB[0] << "/" << cvdB[1] << "/" << cvdB[2] << "/" << cvdB[3] << "/" << cvdB[4]
                  << ", send {0,1,mid,rand} " << sendB[0] << "/" << sendB[1] << "/" << sendB[2] << "/" << sendB[3]
                  << ", tap {0,1,mid,rand} " << tapB[0] << "/" << tapB[1] << "/" << tapB[2] << "/" << tapB[3]
                  << ", delegated-large-clamp " << delegatedLargeClamp << "\n";
        std::cout << "      fuzz cases: " << kCases << " (each pins the int AND the pre-cast float, same BITS)\n";
    });

    std::cout << "\n\033[1m" << g_pass << " passed, " << g_fail << " failed\033[0m\n";
    return g_fail ? 1 : 0;
}
