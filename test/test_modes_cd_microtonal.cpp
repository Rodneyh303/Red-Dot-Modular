// test_modes_cd_microtonal.cpp — the MICROTONAL half of the Mode C/D quantiser, on the REAL primitives.
//
// Why this exists: test_modes_bcd.cpp covers C/D step/gate/legato/rest semantics well, but against a
// 12-TET *mirror* of the quantise logic (weights[12]) — it never exercises the non-default tuning path.
// The MODES_C_D pre-release worry was precisely that a custom tuning (Sikit detune / Micro-24) might
// leave C/D quantising to 12-TET. This test drives the ACTUAL functions the plugin's quantise/degreeOf
// delegate to under a custom tuning — dotModular::TuningTable::degreeVolts + nearestDegree — so the
// microtonal path has real (non-mirror) coverage.
//
// SCOPE (honest): this covers the tuning-table PRIMITIVES C/D snap through. It does NOT drive the full
// SequencerEngine::quantize composition (enabled-mask skip + fader-weighted capture radius + octave ±1
// register preservation) — that method carries engine state and is a Rack RUNTIME-verify item
// (MODES_C_D checklist). What is closed here: "does the custom tuning actually reach the voltage/degree
// math, and diverge from 12-TET where it should".
//
// Pure C++/no Rack (TuningTable.hpp is stdlib-only), builds standalone. Add to run_all.sh TESTS.
//
// Build (standalone):
//   g++ -std=c++17 -Isrc test/test_modes_cd_microtonal.cpp -o /tmp/cdm && /tmp/cdm

#include <cmath>
#include <iostream>

#include "tuning/TuningTable.hpp"

using dotModular::TuningTable;

#define SUITE(n) std::cout << "\n\033[1;34m[" << (n) << "]\033[0m\n"
#define TEST(desc, ...) do { try { __VA_ARGS__; \
    std::cout << "  \033[32mok\033[0m  " << desc << "\n"; ++g_pass; } \
    catch (const std::exception& e) { \
    std::cout << "  \033[31mFAIL\033[0m " << desc << "  — " << e.what() << "\n"; ++g_fail; } } while(0)
#define EXPECT(e) do { if(!(e)) throw std::runtime_error("EXPECT(" #e ") failed"); } while(0)

static int g_pass = 0, g_fail = 0;

// The naive 12-TET degree guess Mode C/D uses ONLY on the isDefault12TET fast path.
static int round12(float frac) { return (int)std::lround((double)frac * 12.0); }

int main() {
    // ── default = 12-TET: the fast path is active ────────────────────────────────────────────────
    SUITE("default: isDefault12TET latched true; degreeVolts == legacy sem/12");
    {
        TuningTable t;                              // ctor resets to equal 12-TET
        TEST("isDefault12TET true at construction", EXPECT(t.isDefault12TET));
        TEST("N == 12", EXPECT(t.N == 12));
        TEST("degreeVolts(7) == 7/12 (bit path proven in test_TuningTable)", {
            EXPECT(std::fabs(t.degreeVolts(7) - 7.f / 12.f) < 1e-6f);
        });
    }

    // ── detuned-12 (a Sikit .scl that isn't equal-tempered) ──────────────────────────────────────
    SUITE("detuned 12 (Sikit): cents path activates; C/D snap uses the detune, not 100·d");
    {
        TuningTable t;
        t.cents[5] = 490.f;                         // degree 5 pulled 10 cents flat of equal 500
        t.recomputeDefaultFlag();
        TEST("isDefault12TET flips false on any detune", EXPECT(!t.isDefault12TET));
        TEST("degreeVolts(5) reflects 490c, not the 12-TET 500c", {
            EXPECT(std::fabs(t.degreeVolts(5) - 490.f / 1200.f) < 1e-6f);
            EXPECT(std::fabs(t.degreeVolts(5) - 5.f / 12.f) > 1e-4f);   // genuinely different
        });
        TEST("nearestDegree names the detuned degree for a pitch sitting on it", {
            EXPECT(t.nearestDegree(490.f / 1200.f) == 5);
        });
    }

    // ── genuinely non-12 tuning (24-EDO stand-in for a Micro / quarter-tone maqam) ─────────────────
    SUITE("24-EDO: degreeVolts is microtonal and nearestDegree DIVERGES from the 12-TET guess");
    {
        TuningTable t;
        t.N = 24;
        for (int i = 0; i < 24; ++i) t.cents[i] = (float)i * 50.f;   // quarter-tone grid
        t.recomputeDefaultFlag();
        TEST("isDefault12TET false when N != 12", EXPECT(!t.isDefault12TET));
        TEST("degreeVolts(1) == 50c = quarter tone (≠ 12-TET 100c)", {
            EXPECT(std::fabs(t.degreeVolts(1) - 50.f / 1200.f) < 1e-6f);
            EXPECT(std::fabs(t.degreeVolts(1) - 1.f / 12.f) > 1e-4f);
        });
        TEST("DIVERGENCE: a 150-cent pitch → degree 3 (exact) under 24-EDO, but round12 guesses 2", {
            float frac = 150.f / 1200.f;            // exactly quarter-tone degree 3
            EXPECT(t.nearestDegree(frac) == 3);     // true nearest under the real tuning
            EXPECT(round12(frac) != 3);             // the 12-TET fast-path guess would be wrong here
        });
        TEST("degreeVolts(3) reconstructs the 150c pitch", {
            EXPECT(std::fabs(t.degreeVolts(3) - 150.f / 1200.f) < 1e-6f);
        });
    }

    // ── contract pins (guard against well-meaning future 'fixes') ──────────────────────────────────
    SUITE("primitive contracts C/D relies on");
    {
        TuningTable t;
        t.N = 24;
        for (int i = 0; i < 24; ++i) t.cents[i] = (float)i * 50.f;
        t.recomputeDefaultFlag();
        TEST("nearestDegree does NOT filter by enabled[] — mask-skip is quantize()'s job, not this", {
            t.enabled[3] = false;                   // disable the exact-nearest degree
            EXPECT(t.nearestDegree(150.f / 1200.f) == 3);   // still returned: contract is 'closest', period
        });
        TEST("degreeVolts clamps a negative index to degree 0", {
            EXPECT(std::fabs(t.degreeVolts(-4) - t.degreeVolts(0)) < 1e-9f);
        });
        TEST("degreeVolts clamps an over-range index to MAXN-1", {
            EXPECT(std::fabs(t.degreeVolts(TuningTable::MAXN + 9) - t.degreeVolts(TuningTable::MAXN - 1)) < 1e-9f);
        });
        TEST("isDefault12TET stays false while N != 12 even if the first 12 look equal", {
            TuningTable u; u.N = 24;
            for (int i = 0; i < 24; ++i) u.cents[i] = (i < 12) ? (float)i * 100.f : 0.f;
            u.recomputeDefaultFlag();
            EXPECT(!u.isDefault12TET);
        });
    }

    std::cout << "\n-----\n" << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail == 0 ? 0 : 1;
}
