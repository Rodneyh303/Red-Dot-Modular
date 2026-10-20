// test_resolver_dir_bridge.cpp — Resolver STEP 4's BYTE-IDENTICAL gate (DIRECTION).
//
// PURPOSE: the LAST hand-rolled delegation shape — BOTH direction pushes
// (MonsoonExpanderManager's poly laneDirVPending_ block + the mono
// laneDirPending_ field arm) now route through
// SandsModifierResolver<DirPolicy> via redDot::resolveDirItem: the ownership
// selection, the [0,3] end-clamp and the (int)lround moved to the header
// (SandsModifierResolver.hpp); the manager keeps only the store reads. This
// test is the PERMANENT PIN: the selection space is TINY, so it is EXHAUSTIVE
// (not statistical) — a full grid over delegated × ownDir × globalDir — plus
// a seeded fuzz over the FULL Inputs shape. BOTH layers are pinned: the final
// INT (equality — the value both pending stores receive) AND the PRE-ROUND
// float (same BITS, via redDot::resolveDirValue — the layer the lround
// consumes), so neither the algebra nor the round can silently drift.
//
// WHAT THIS BRIDGES — and what it does not: the MOVED algebra only (selection
// / [0,3] clamp / lround / cast). The ORACLE below is the OLD POLY LINE,
// duplicated VERBATIM from the pre-step-4 MonsoonExpanderManager.cpp (tests
// may duplicate; production may not) — with one naming note: the old ternary's
// true-branch was "store flag set → the voice's OWN getLaneDir", and the store
// flag (getMacroOwn) is EAST-owns (the East UI's owner switch writes 1 = East
// owns), so delegated-to-Macro is the flag's COMPLEMENT at the poly call
// site. The mono line's field arms (macroGlobal ? getGlobalDir : getLaneDir)
// reduce to the SAME oracle shape with delegated = macroGlobal
// (monoDirAuthority sets it only on the Macro-owns arm). The mono param arm
// is a SOURCE-TYPE dispatch (which module owns the value), not a Macro/East
// selection — it stays call-site and is NOT bridged.
//
// The full-Inputs fuzz additionally pins the concept-required members
// (scaleCv identity, addCv/addSend = v+c/v+s) against the skeleton's
// documented canonical arithmetic — direction's PRODUCTION wiring never
// attaches CV/send (ownCv/eastCv disconnected, macroSend = 0), and the fuzz
// proves the policy does not DEPEND on those defaults: the delegated branch
// must ignore every other Input (PURE SELECTION — td structurally ignored).
//
// Header-only, Rack-free; registered in test/run_all.sh as
// "test_resolver_dir_bridge|" (the resolver-bridge compile shape; step 1's
// spread bridge is the template).

#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "SandsModifierResolver.hpp"    // the skeleton + DirPolicy + resolveDirItem/resolveDirValue

static int g_pass = 0, g_fail = 0;
#define SUITE(n) std::cout << "\n\033[1;34m[" << (n) << "]\033[0m\n"
#define TEST(desc, ...) do { try { __VA_ARGS__; \
    std::cout << "  \033[32mok\033[0m  " << desc << "\n"; ++g_pass; } \
    catch (const std::exception& e) { \
    std::cout << "  \033[31mFAIL\033[0m " << desc << "  — " << e.what() << "\n"; ++g_fail; } } while(0)

// ── compile-time pin: the concept BITES ──────────────────────────────────────
static_assert(redDot::SandsModifierPolicy<redDot::DirPolicy>,
              "DirPolicy must satisfy the policy concept");

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

// ══════════════════════════════════════════════════════════════════════════════
// THE ORACLE — the OLD poly line, duplicated VERBATIM from the pre-step-4
// MonsoonExpanderManager.cpp (the algebra between the ownership read and the
// pending-store write), with the store reads factored to inputs. This copy is
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

// The pre-round float of the old line (the layer the lround consumed):
//   dirVal = <Macro owns ? Macro's globalDir : the voice's own laneDir>;
//   dv     = (int)std::lround(math::clamp(dirVal, 0.f, 3.f));
static float value(bool delegated, float ownDir, float globalDir) {
    const float dirVal = delegated ? globalDir : ownDir;
    return clamp(dirVal, 0.f, 3.f);
}

// The old final int — lround (half away from zero) AFTER the clamp.
static int combine(bool delegated, float ownDir, float globalDir) {
    return (int)std::lround(value(delegated, ownDir, globalDir));
}

} // namespace oracle

// THE BRIDGE CHECK (the simplified wiring shape — what both pushes pass):
//   (a) the final int EQUAL (the value both pending stores receive), and
//   (b) the PRE-ROUND float same-BITS (the algebra layer under the lround).
// Throws with full repro data on any drift.
static void expectBridge(long caseIdx, bool delegated, float ownDir, float globalDir,
                         const char* tag = "fuzz") {
    const int   wantI = oracle::combine(delegated, ownDir, globalDir);
    const float wantF = oracle::value(delegated, ownDir, globalDir);
    const int   gotI  = redDot::resolveDirItem(delegated, ownDir, globalDir);
    const float gotF  = redDot::resolveDirValue(delegated, ownDir, globalDir);
    if (gotI == wantI && sameBits(gotF, wantF)) return;
    std::ostringstream s;
    s << tag << " case " << caseIdx
      << " (delegated=" << delegated << " ownDir=" << ownDir << " globalDir=" << globalDir
      << "): int " << gotI << " vs " << wantI
      << ", float " << bits(gotF) << " vs " << bits(wantF);
    throw std::runtime_error(s.str());
}

// ── the FULL Inputs shape (the fuzz's second layer) ──────────────────────────
// The production wiring sets delegated/base/macroBase only; the fuzz randomizes
// EVERY field and pins effective() against the skeleton's documented canonical
// arithmetic — proving the policy does not DEPEND on the wiring's defaults.
struct DirCase {
    bool  delegated = false;
    float base = 0.f, macroBase = 0.f;
    bool  ownCvConn = false;  float ownU = 0.f, ownA = 0.f;
    bool  eastCvConn = false; float eastU = 0.f, eastA = 0.f;
    bool  macroPresent = false;
    float macroSend = 0.f, pre = 0.f, post = 0.f, tap = 1.f;
};

static float resolveFull(const DirCase& c) {
    redDot::SandsModifierResolver<redDot::DirPolicy>::Inputs in;
    in.delegated       = c.delegated;
    in.base            = c.base;
    in.macroBase       = c.macroBase;
    in.ownCv           = {c.ownCvConn, c.ownU, c.ownA};
    in.eastCv          = {c.eastCvConn, c.eastU, c.eastA};
    in.macroPresent    = c.macroPresent;
    in.macroSend       = c.macroSend;
    in.macroPreDelta   = c.pre;
    in.macroPostDelta  = c.post;
    in.macroTap        = c.tap;
    return redDot::SandsModifierResolver<redDot::DirPolicy>::effective(in);
}

// The canonical-arithmetic oracle (the header's documented contract with
// DirPolicy's ops: scaleCv identity, addCv/addSend = v+c/v+s, end-clamp-once,
// skip-if-absent, and PURE SELECTION on the delegated branch — td ignored):
static float oracleFull(const DirCase& c) {
    if (c.delegated) return oracle::clamp(c.macroBase, 0.f, 3.f);
    float v = c.base;
    if (c.ownCvConn)  v = v + (c.ownU * c.ownA);
    if (c.eastCvConn) v = v + (c.eastU * c.eastA);
    if (c.macroPresent)
        v = v + ((c.pre + (c.post - c.pre) * c.tap) * c.macroSend);
    return oracle::clamp(v, 0.f, 3.f);
}

static void expectFull(long caseIdx, const DirCase& c) {
    const float got  = resolveFull(c);
    const float want = oracleFull(c);
    if (sameBits(got, want)) return;
    std::ostringstream s;
    s << "full-inputs case " << caseIdx
      << " (delegated=" << c.delegated << " base=" << c.base << " macroBase=" << c.macroBase
      << " ownCv=(" << c.ownCvConn << "," << c.ownU << "," << c.ownA << ")"
      << " eastCv=(" << c.eastCvConn << "," << c.eastU << "," << c.eastA << ")"
      << " mp=" << c.macroPresent << " send=" << c.macroSend
      << " pre/post/tap=" << c.pre << "/" << c.post << "/" << c.tap << "): "
      << bits(got) << " vs " << bits(want);
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

static constexpr uint64_t kSeed = 0x0D1E5EED5EED5EEDull;   // "dir-seed" — fixed, printed

// The exhaustive grid: delegated ∈ {false,true} × ownDir/globalDir over the
// rounding/clamping edges — all pairs (2 × 11 × 11 = 242).
static const float kGrid[] = {
    0.f, 1.f, 2.f, 3.f,          // the four authored LaneDir values
    0.5f, 1.5f, 2.5f,            // lround half-cases (half away from zero)
    3.5f, -0.5f,                 // out-of-window (above / below)
    0.4999999f, 2.5000001f,      // just-under-half / just-over-half
};
static constexpr int kGridN = (int)(sizeof kGrid / sizeof kGrid[0]);

int main() {
    SUITE("DirPolicy surface (the concept + the round/cast split)");
    TEST("resolveDirItem == (int)lround(resolveDirValue) — the round+cast lives in the int wrapper", {
        if (redDot::resolveDirItem(false, 1.7f, 2.9f)
                != (int)std::lround(redDot::resolveDirValue(false, 1.7f, 2.9f)))
            throw std::runtime_error("resolveDirItem must be the (int)lround of resolveDirValue");
        expectBridge(0, false, 1.7f, 2.9f, "surface");
        expectBridge(1, true, 1.7f, 2.9f, "surface");
    });

    SUITE("EXHAUSTIVE grid — delegated x ownDir x globalDir, int + same-BITS pre-round float");
    TEST("242 cases: 2 x 11 x 11 (authored values, half-cases, out-of-window, near-half)", {
        long n = 0;
        for (int d = 0; d < 2; ++d)
            for (int i = 0; i < kGridN; ++i)
                for (int j = 0; j < kGridN; ++j)
                    expectBridge(n++, d != 0, kGrid[i], kGrid[j], "grid");
        if (n != 242) {
            std::ostringstream s; s << "grid size changed: " << n << " (expected 242)";
            throw std::runtime_error(s.str());
        }
        std::cout << "      grid cases: " << n << " (each pins the int AND the pre-round float, same BITS)\n";
    });

    SUITE("Named anchors — rounding boundaries, clamps, pure selection");
    TEST("lround half-cases: 2.5 -> 3 (half away from zero); 0.5 -> 1; 1.5 -> 2", {
        if (redDot::resolveDirItem(false, 2.5f, 0.f) != 3)
            throw std::runtime_error("own 2.5 must round to 3 (lround rounds half away from zero)");
        if (redDot::resolveDirItem(false, 0.5f, 0.f) != 1)
            throw std::runtime_error("own 0.5 must round to 1");
        if (redDot::resolveDirItem(false, 1.5f, 0.f) != 2)
            throw std::runtime_error("own 1.5 must round to 2");
        // and the just-off-half neighbours land on the expected sides
        if (redDot::resolveDirItem(false, 0.4999999f, 0.f) != 0)
            throw std::runtime_error("own 0.4999999 must round to 0");
        if (redDot::resolveDirItem(false, 2.5000001f, 0.f) != 3)
            throw std::runtime_error("own 2.5000001 must round to 3");
        expectBridge(0, false, 2.5f, 0.f, "anchor-half");
        expectBridge(1, false, 2.5000001f, 0.f, "anchor-half");
    });
    TEST("out-of-window clamps: -0.5 -> 0, 3.5 -> 3 (both arms; pre-round float is exactly the rail)", {
        if (redDot::resolveDirItem(false, -0.5f, 0.f) != 0)
            throw std::runtime_error("own -0.5 must clamp to 0");
        if (!sameBits(redDot::resolveDirValue(false, -0.5f, 0.f), 0.f))
            throw std::runtime_error("clamped -0.5 must be exactly +0.0f");
        if (redDot::resolveDirItem(false, 3.5f, 0.f) != 3)
            throw std::runtime_error("own 3.5 must clamp to 3");
        if (!sameBits(redDot::resolveDirValue(false, 3.5f, 0.f), 3.f))
            throw std::runtime_error("clamped 3.5 must be exactly 3.0f");
        // the delegated arm clamps Macro's global the same way
        if (redDot::resolveDirItem(true, 0.f, -0.5f) != 0)
            throw std::runtime_error("global -0.5 must clamp to 0");
        if (redDot::resolveDirItem(true, 0.f, 3.5f) != 3)
            throw std::runtime_error("global 3.5 must clamp to 3");
        expectBridge(0, false, -0.5f, 0.f, "anchor-clamp");
        expectBridge(1, false, 3.5f, 0.f, "anchor-clamp");
        expectBridge(2, true, 0.f, -0.5f, "anchor-clamp");
        expectBridge(3, true, 0.f, 3.5f, "anchor-clamp");
    });
    TEST("delegated ignores ownDir ENTIRELY (pure selection); free arm mirrors it for globalDir", {
        // any ownDir with delegated=true => the SAME result (int AND bits)
        const float wild[] = {-7.f, 42.25f, 0.1234567f, 2.9999f};
        const int   refI   = redDot::resolveDirItem(true, 0.f, 1.f);
        const float refF   = redDot::resolveDirValue(true, 0.f, 1.f);
        long n = 0;
        for (float o : kGrid) {
            if (redDot::resolveDirItem(true, o, 1.f) != refI
                || !sameBits(redDot::resolveDirValue(true, o, 1.f), refF)) {
                std::ostringstream s;
                s << "delegated=true with ownDir=" << o << " changed the result (ownDir must be ignored)";
                throw std::runtime_error(s.str());
            }
            ++n;
        }
        for (float o : wild) {
            if (redDot::resolveDirItem(true, o, 1.f) != refI
                || !sameBits(redDot::resolveDirValue(true, o, 1.f), refF)) {
                std::ostringstream s;
                s << "delegated=true with wild ownDir=" << o << " changed the result";
                throw std::runtime_error(s.str());
            }
            ++n;
        }
        // the mirror: the free arm ignores globalDir entirely
        const int   ref2I = redDot::resolveDirItem(false, 2.f, 0.f);
        const float ref2F = redDot::resolveDirValue(false, 2.f, 0.f);
        for (float g : kGrid) {
            if (redDot::resolveDirItem(false, 2.f, g) != ref2I
                || !sameBits(redDot::resolveDirValue(false, 2.f, g), ref2F)) {
                std::ostringstream s;
                s << "delegated=false with globalDir=" << g << " changed the result (globalDir must be ignored)";
                throw std::runtime_error(s.str());
            }
            ++n;
        }
        std::cout << "      pure-selection cells: " << n << " (all collapsed to one result per arm)\n";
    });

    SUITE("Seeded deterministic fuzz — the FULL Inputs shape vs the canonical arithmetic");
    TEST("20000 cases: the simplified bridge (int + bits) AND effective() vs the canonical oracle", {
        const long kCases = 20000;
        std::cout << "      seed: 0x" << std::hex << kSeed << std::dec
                  << "  cases: " << kCases << "\n";
        Rng rng(kSeed);

        long delegT = 0, delegF = 0;
        long ownCvC = 0, eastCvC = 0, bothCv = 0;
        long mpT = 0, mpF = 0, sendPos = 0;
        long outLo = 0, outHi = 0;         // base or macroBase outside [0,3]
        long delegWildOthers = 0;          // delegated && (cv attached || macroPresent)

        for (long i = 0; i < kCases; ++i) {
            DirCase c;
            c.delegated    = (i & 1) != 0;
            c.macroPresent = (i & 2) != 0;
            (c.delegated ? delegT : delegF)++;
            (c.macroPresent ? mpT : mpF)++;

            c.ownCvConn  = ((i >> 2) & 1) != 0;
            c.eastCvConn = ((i >> 3) & 1) != 0;
            if (c.ownCvConn)  ++ownCvC;
            if (c.eastCvConn) ++eastCvC;
            if (c.ownCvConn && c.eastCvConn) ++bothCv;

            // base / macroBase buckets: in-window random / 0 / 3 / below / above
            auto pickBase = [&rng](int sel) -> float {
                switch (sel) {
                    case 0:  return rng.uniformF(0.f, 3.f);
                    case 1:  return 0.f;
                    case 2:  return 3.f;
                    case 3:  return rng.uniformF(-4.f, -0.001f);   // out, below
                    default: return rng.uniformF(3.001f, 8.f);     // out, above
                }
            };
            c.base      = pickBase((int)(i % 5));
            c.macroBase = pickBase((int)((i >> 2) % 5));
            if (c.base < 0.f || c.macroBase < 0.f) ++outLo;
            if (c.base > 3.f || c.macroBase > 3.f) ++outHi;

            c.ownU  = rng.uniformF(-10.f, 10.f);   c.ownA  = rng.uniformF(-1.f, 1.f);
            c.eastU = rng.uniformF(-10.f, 10.f);   c.eastA = rng.uniformF(-1.f, 1.f);

            const int sb = (i >> 4) & 3;
            switch (sb) {
                case 0:  c.macroSend = 0.f; break;
                case 1:  c.macroSend = 1.f; break;
                case 2:  c.macroSend = 0.5f; break;
                default: c.macroSend = rng.uniformF(0.f, 1.f); break;
            }
            if (c.macroPresent && c.macroSend > 0.f) ++sendPos;

            c.pre  = rng.uniformF(-5.f, 5.f);
            c.post = rng.uniformF(-5.f, 5.f);
            if ((i & 15) == 0) c.post = c.pre;    // tap-neutrality sprinkled in
            const int tb = (i >> 6) & 3;
            switch (tb) {
                case 0:  c.tap = 0.f; break;
                case 1:  c.tap = 1.f; break;
                case 2:  c.tap = 0.5f; break;
                default: c.tap = rng.uniformF(0.f, 1.f); break;
            }

            if (c.delegated && (c.ownCvConn || c.eastCvConn || c.macroPresent)) ++delegWildOthers;

            // (a) the simplified shape both pushes pass — int + pre-round bits
            expectBridge(i, c.delegated, c.base, c.macroBase);
            // (b) the FULL Inputs shape vs the canonical arithmetic (same BITS)
            expectFull(i, c);
        }

        // Coverage MUST be total — the gate cannot silently rot.
        if (delegT <= 0 || delegF <= 0) throw std::runtime_error("delegated arm never exercised");
        if (ownCvC <= 0 || eastCvC <= 0 || bothCv <= 0) throw std::runtime_error("CV attachment never exercised");
        if (mpT <= 0 || mpF <= 0) throw std::runtime_error("macroPresent arm never exercised");
        if (sendPos <= 0) throw std::runtime_error("live send never exercised");
        if (outLo <= 0 || outHi <= 0) throw std::runtime_error("out-of-window base never exercised");
        if (delegWildOthers <= 0)
            throw std::runtime_error("delegated-with-attached-terms never exercised (the must-not-depend-on-defaults pin)");

        std::cout << "      coverage: delegated " << delegF << "/" << delegT
                  << ", ownCv/eastCv/both " << ownCvC << "/" << eastCvC << "/" << bothCv
                  << ", macroPresent " << mpF << "/" << mpT
                  << ", live send " << sendPos
                  << ", out-of-window lo/hi " << outLo << "/" << outHi
                  << ", delegated-with-attached-terms " << delegWildOthers << "\n";
        std::cout << "      fuzz cases: " << kCases
                  << " (each pins the simplified bridge int+bits AND the full-Inputs canonical form, same BITS)\n";
    });

    std::cout << "\n\033[1m" << g_pass << " passed, " << g_fail << " failed\033[0m\n";
    return g_fail ? 1 : 0;
}
