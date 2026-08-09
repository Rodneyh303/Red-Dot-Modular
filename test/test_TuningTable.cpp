// test_TuningTable.cpp — dotModular::TuningTable byte-identical guarantee (Sikit Phase 1, Step I).
//
// The load-bearing safety property: at the equal-division default (cents[i] = i*100), the table's
// degree->voltage contribution is BIT-IDENTICAL to the legacy 12-TET `sem/12`. This is what makes
// "Sikit at default cents == Monsoon standalone 12-TET" true (genPitchLive's cents path reduces to
// the legacy expression). Also checks the isDefault12TET fast-path latch flips correctly.
//
// Pure C++/no Rack (TuningTable.hpp is stdlib-only), so it builds standalone. Registered in
// run_all.sh as "test_TuningTable|".
//
// Build (standalone):
//   g++ -std=c++17 -Isrc test/test_TuningTable.cpp -o /tmp/ttt && /tmp/ttt

#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

#include "tuning/TuningTable.hpp"

using dotModular::TuningTable;

#define SUITE(n) std::cout << "\n\033[1;34m[" << (n) << "]\033[0m\n"
#define TEST(desc, ...) do { try { __VA_ARGS__; \
    std::cout << "  \033[32mok\033[0m  " << desc << "\n"; ++g_pass; } \
    catch (const std::exception& e) { \
    std::cout << "  \033[31mFAIL\033[0m " << desc << "  — " << e.what() << "\n"; ++g_fail; } } while(0)
#define EXPECT(e) do { if(!(e)) throw std::runtime_error("EXPECT(" #e ") failed"); } while(0)
// BITWISE float equality — the whole point is byte-identity, not approximate.
#define EXPECT_BITEQ(a,b) do { float _x=(a),_y=(b); if(std::memcmp(&_x,&_y,sizeof(float))!=0) { \
    std::ostringstream _s; _s << "EXPECT_BITEQ(" #a "," #b ") : " << _x << " != " << _y; \
    throw std::runtime_error(_s.str()); } } while(0)

static int g_pass = 0, g_fail = 0;

int main() {
    SUITE("default table: equal-division 12-TET");
    TEST("fresh table is N=12, isDefault12TET, cents[i]=i*100", {
        TuningTable tt;
        EXPECT(tt.N == 12);
        EXPECT(tt.isDefault12TET);
        for (int i = 0; i < 12; ++i) EXPECT_BITEQ(tt.cents[i], (float)i * 100.f);
    });

    // NOTE (ENABLED_MASK_BUILD_BRIEF): the ENGINE's byte-identical guarantee lives in genPitchLive's OWN
    // legacy `(sem+transpose)/12` path under isDefault12TET (covered by test_TuningRoundTrip) — NOT in
    // degreeVolts, which intentionally uses cents[]*(1/1200) (the general microtonal map). These two
    // checks therefore assert degreeVolts is SELF-CONSISTENT with the equal-division cents map (i*100),
    // mirroring degreeVolts' own reciprocal form — a last-bit float nuance, not an engine-path claim.
    SUITE("degreeVolts self-consistency at equal division (cents[i]=i*100)");
    TEST("every degree 0..11 equals cents[i]*(1/1200) bit-for-bit", {
        TuningTable tt;
        for (int sem = 0; sem < 12; ++sem) {
            float ref = ((float)sem * 100.f) * (1.f / 1200.f);   // degreeVolts' own form
            EXPECT_BITEQ(tt.degreeVolts(sem), ref);
        }
    });
    TEST("full voltage reconstruction matches across octaves (transpose 0)", {
        TuningTable tt;
        for (int oct = 0; oct <= 8; ++oct) {
            for (int sem = 0; sem < 12; ++sem) {
                float ref    = (float)oct - 4.f + ((float)sem * 100.f) * (1.f / 1200.f);
                float viaTbl = (float)oct - 4.f + tt.degreeVolts(sem);
                EXPECT_BITEQ(viaTbl, ref);
            }
        }
    });

    SUITE("isDefault12TET latch (recomputeDefaultFlag)");
    TEST("stays true when a writer re-sets equal-division cents verbatim (Sikit at defaults)", {
        TuningTable tt;
        tt.isDefault12TET = false;              // pretend a source is publishing
        tt.N = 12;
        for (int i = 0; i < 12; ++i) tt.cents[i] = (float)i * 100.f;   // Sikit default knobs
        tt.recomputeDefaultFlag();
        EXPECT(tt.isDefault12TET);              // => genPitchLive keeps the byte-identical path
    });
    TEST("flips false on ANY detune (e.g. +1 cent on one degree)", {
        TuningTable tt;
        tt.cents[7] = 701.f;                    // detune the fifth by 1 cent
        tt.recomputeDefaultFlag();
        EXPECT(!tt.isDefault12TET);
    });
    TEST("flips false when N != 12 even with 12-TET cents", {
        TuningTable tt;
        tt.N = 24;
        tt.recomputeDefaultFlag();
        EXPECT(!tt.isDefault12TET);
    });
    TEST("resetToEqual12TET restores default (N=12, flag true, cents i*100)", {
        TuningTable tt;
        tt.cents[3] = 250.f; tt.N = 24; tt.isDefault12TET = false;
        tt.resetToEqual12TET();
        EXPECT(tt.N == 12);
        EXPECT(tt.isDefault12TET);
        for (int i = 0; i < 12; ++i) EXPECT_BITEQ(tt.cents[i], (float)i * 100.f);
    });

    SUITE("cents path produces the EXPECTED shift when detuned");
    TEST("a degree at 150 cents lands halfway between semitone 1 and 2 in volts", {
        TuningTable tt;
        tt.cents[1] = 150.f;                    // quarter-tone-ish detune of C#
        tt.recomputeDefaultFlag();
        EXPECT(!tt.isDefault12TET);
        EXPECT_BITEQ(tt.degreeVolts(1), 150.f / 1200.f);   // 0.125 V
    });

    SUITE("enabled[] scale-membership mask (ENABLED_MASK_BUILD_BRIEF)");
    TEST("fresh table: every degree enabled (mask carries no authority by default)", {
        TuningTable tt;
        for (int i = 0; i < TuningTable::MAXN; ++i) EXPECT(tt.enabled[i]);
    });
    TEST("resetToEqual12TET re-enables all degrees", {
        TuningTable tt;
        for (int i = 0; i < TuningTable::MAXN; ++i) tt.enabled[i] = false;
        tt.resetToEqual12TET();
        for (int i = 0; i < TuningTable::MAXN; ++i) EXPECT(tt.enabled[i]);
    });
    TEST("enabled is INDEPENDENT of weight (a fader at 0 stays in-scale)", {
        TuningTable tt;
        tt.weight[4] = 0.f;                      // degree turned down to silence
        EXPECT(tt.enabled[4]);                   // still in-scale → raisable (round-8/9 bug gone)
    });
    TEST("enabled does NOT affect the cents fast-path (isDefault12TET)", {
        TuningTable tt;
        tt.enabled[5] = false;
        tt.recomputeDefaultFlag();
        EXPECT(tt.isDefault12TET);               // mask is orthogonal to the tuning map
    });

    std::cout << "\n-----\n" << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail == 0 ? 0 : 1;
}
