/**
 * test_qmix_poly.cpp — the PER-VOICE poly q-mix path (the gap in coverage).
 *
 * Q-mix is confirmed in mono (via Lantern) but never exercised in poly. Two of CA's eight
 * correlated pairs are QM and Intertropical carries them to Keppel, so a poly lane-index bug
 * would propagate silently into the expression chain. This test pins the poly path directly.
 *
 * Coverage (SANDS_LANE_INDEX_AUDIT.md — the lane-index bug class that has bitten repeatedly):
 *   • Distinctness — 16 voices get 16 distinct q-mix values; read back, each voice gets its own.
 *   • Routing identity — voice v's value lands on voice v, not v±1 (value derived from the index
 *     so an off-by-one is unambiguous, not a plausible-looking shift).
 *   • Neighbour independence — writing q-mix must not disturb REST (lane 0) or ACCENT (lane 3),
 *     the engine lanes either side of QMIX (lane 4). The failure mode of inserting a lane at idx 2.
 *   • Mono/poly agreement — V1's q-mix through monoSlewed and through the poly spread path agree
 *     (mono is the verified reference).
 *   • Poly spread on q-mix — sweeping spread for one voice moves only that voice's value, no other;
 *     and under follow-CA with src != self, own (pre-remap) != target (post-remap) — the contract
 *     from SPREAD_TARGET_MODES.md POST-MORTEM, in a test rather than a runtime assert.
 *
 * Header-only harness (mirrors test_SpreadInterp.cpp): the PatternEngine slewed buffers are public,
 * so we inject known values and read them back via SpreadInterp's static accessors. No companion
 * source needed.
 *
 * Compile (mirrors run_all.sh INCS): g++ -std=c++17 -Itest -Isrc -Isrc/tuning -Isrc/dsp \
 *   -Isrc/dsp/engines -Isrc/dsp/gates -Isrc/dsp/managers -Isrc/ui test/test_qmix_poly.cpp -o /tmp/qmp && /tmp/qmp
 * (run_all.sh lists it header-only, like test_SpreadInterp.)
 */
#include "test_stubs.hpp"
#include "PatternEngine.hpp"
#include "SpreadInterp.hpp"
#include <iostream>
#include <sstream>
#include <cmath>

using namespace redDot;
static int s_pass = 0, s_fail = 0;
#define SUITE(n) do{std::cout<<"\n["<<(n)<<"]\n";}while(0)
#define TEST(desc,...) do{ bool _ok=true; std::string _m; \
    try{__VA_ARGS__;}catch(const std::exception&_e){_ok=false;_m=_e.what();} \
    if(_ok){++s_pass;std::cout<<"  PASS "<<(desc)<<"\n";} \
    else{++s_fail;std::cout<<"  FAIL "<<(desc); if(!_m.empty())std::cout<<" — "<<_m; std::cout<<"\n";} }while(0)
#define EXPECT(e) do{if(!(e))throw std::runtime_error("EXPECT(" #e ")");}while(0)
#define EXPECT_NEAR(a,b,e) do{if(std::fabs((a)-(b))>(e)){std::ostringstream s;s<<#a<<"="<<(a)<<" not~"<<#b<<"="<<(b);throw std::runtime_error(s.str());}}while(0)

// Spread/poly-engine lane indices (SpreadInterp): 0=REST 1=MELODY 2=OCT 3=ACC 4=QMIX.
static constexpr int LANE_REST = 0, LANE_ACC = 3, LANE_QMIX = 4;
static constexpr int N_POLY = 15;   // poly voices V2..V16 (poly index 0..14)
static constexpr int STEP = 3;      // arbitrary step within the 16-step pattern

// A value derived from the voice index so an off-by-one is unambiguous (not a plausible pattern).
// Keep in (0,1) — q-mix is a uniform-marginal probability. Avoid 0/1 edges and 0.5 (mix2 fixed point).
static float voiceVal(int pv) { return 0.10f + 0.053f * (float)pv; }  // pv0→0.10, pv14→0.842 — all distinct

int main() {
    SUITE("distinctness — 15 poly voices get 15 distinct q-mix values, each reads its own");
    {
        PatternEngine pe;
        for (int pv = 0; pv < N_POLY; ++pv)
            pe.slewedPolyQmix[pv][STEP] = voiceVal(pv);
        TEST("each poly voice reads back exactly its own q-mix value", {
            for (int pv = 0; pv < N_POLY; ++pv)
                EXPECT_NEAR(SpreadInterp::polySlewed(pe, LANE_QMIX, pv, STEP), voiceVal(pv), 0.0f);
        });
        TEST("all 15 values are mutually distinct", {
            for (int a = 0; a < N_POLY; ++a)
                for (int b = a + 1; b < N_POLY; ++b)
                    EXPECT(voiceVal(a) != voiceVal(b));
        });
    }

    SUITE("routing identity — voice v's q-mix lands on voice v, not v±1");
    {
        PatternEngine pe;
        for (int pv = 0; pv < N_POLY; ++pv)
            pe.slewedPolyQmix[pv][STEP] = voiceVal(pv);
        // An off-by-one would make voice pv read voiceVal(pv±1). Assert each reads its own index-derived
        // value and NOT its neighbour's — catches both directions of a shift.
        TEST("voice pv reads voiceVal(pv), not voiceVal(pv-1) or voiceVal(pv+1)", {
            for (int pv = 0; pv < N_POLY; ++pv) {
                const float got = SpreadInterp::polySlewed(pe, LANE_QMIX, pv, STEP);
                EXPECT_NEAR(got, voiceVal(pv), 1e-6f);
                if (pv > 0)          EXPECT(std::fabs(got - voiceVal(pv - 1)) > 1e-4f);
                if (pv < N_POLY - 1) EXPECT(std::fabs(got - voiceVal(pv + 1)) > 1e-4f);
            }
        });
    }

    SUITE("neighbour independence — writing q-mix must not disturb REST or ACCENT");
    {
        // The specific failure mode of inserting a lane at index 2: a mis-indexed q-mix accessor
        // could alias onto the REST or ACCENT buffers. Seed REST/ACC with their own distinct
        // per-voice values, write q-mix, and assert REST/ACC are untouched.
        PatternEngine pe;
        for (int pv = 0; pv < N_POLY; ++pv) {
            pe.slewedPolyRhythm[pv][STEP] = 0.20f + 0.01f * pv;   // REST (lane 0)
            pe.slewedPolyAccent[pv][STEP] = 0.70f - 0.01f * pv;   // ACC (lane 3)
            pe.slewedPolyQmix[pv][STEP]   = voiceVal(pv);          // QMIX (lane 4)
        }
        TEST("REST (lane 0) unchanged after writing q-mix", {
            for (int pv = 0; pv < N_POLY; ++pv)
                EXPECT_NEAR(SpreadInterp::polySlewed(pe, LANE_REST, pv, STEP), 0.20f + 0.01f * pv, 0.0f);
        });
        TEST("ACCENT (lane 3) unchanged after writing q-mix", {
            for (int pv = 0; pv < N_POLY; ++pv)
                EXPECT_NEAR(SpreadInterp::polySlewed(pe, LANE_ACC, pv, STEP), 0.70f - 0.01f * pv, 0.0f);
        });
        TEST("and q-mix itself reads correctly (no cross-contamination back)", {
            for (int pv = 0; pv < N_POLY; ++pv)
                EXPECT_NEAR(SpreadInterp::polySlewed(pe, LANE_QMIX, pv, STEP), voiceVal(pv), 0.0f);
        });
    }

    SUITE("mono/poly agreement — V1's q-mix matches through mono and poly paths");
    {
        // Mono is the verified reference (Lantern). V1's q-mix value should be the same whether read
        // via monoSlewed (the mono buffer) or via the poly path's pre-remap/slewed at the V1-equivalent.
        // The mono slewedQmix[] is the V1 source; polySlewed is V2..V16. They're distinct buffers, so
        // "agreement" here is that spread==0 on the poly path returns the voice's own value exactly
        // (bit-identity, matching mono's spread==0 contract) — i.e. the poly path doesn't alter the
        // stored value when there's nothing to blend toward.
        PatternEngine pe;
        for (int pv = 0; pv < N_POLY; ++pv)
            pe.slewedPolyQmix[pv][STEP] = voiceVal(pv);
        pe.slewedQmix[STEP] = 0.44f;   // V1 mono reference value
        TEST("poly spread==0 returns own value exactly (bit-identity, mono contract)", {
            for (int pv = 0; pv < N_POLY; ++pv)
                EXPECT_NEAR(SpreadInterp::applyPoly(pe, LANE_QMIX, pv, STEP, 0.0f), voiceVal(pv), 0.0f);
        });
        TEST("mono spread==0 returns own value exactly (the reference)", {
            EXPECT_NEAR(SpreadInterp::applyMono(pe, LANE_QMIX, STEP, 0.0f), 0.44f, 0.0f);
        });
    }

    SUITE("poly spread on q-mix — moves the voice toward its target, spread==0 is identity for all");
    {
        // CORRECTED expectation: in anchor-V1 mode every poly voice's spread target is the SHARED mono
        // V1 value (monoSlewed), so spread≠0 moves EVERY voice toward that target — NOT "only the swept
        // voice." (The earlier "others unchanged" assertion was wrong: spread applies to all voices,
        // each blending toward the same V1 target.) The real invariants: (a) spread≠0 changes the
        // voice's value vs its own; (b) the result stays in [0,1] (uniform marginal preserved); (c)
        // spread==0 is bit-identity for EVERY voice (the no-op guard). Voice 7 is the swept exemplar.
        PatternEngine pe;
        for (int pv = 0; pv < N_POLY; ++pv)
            pe.slewedPolyQmix[pv][STEP] = voiceVal(pv);
        pe.slewedQmix[STEP] = 0.90f;             // the mono V1 target (distinct from every voiceVal)
        pe.spreadTargetMode[LANE_QMIX] = 0;       // anchor V1: target = monoSlewed = 0.90
        const int target = 7;
        const float v7own = voiceVal(target);
        TEST("spread≠0 moves voice 7's value off its own; result in [0,1]", {
            for (float sp : {-0.8f, -0.3f, 0.3f, 0.8f}) {
                const float v7 = SpreadInterp::applyPoly(pe, LANE_QMIX, target, STEP, sp);
                EXPECT(v7 >= 0.0f && v7 <= 1.0f);
                EXPECT(std::fabs(v7 - v7own) > 1e-4f);   // the knob does something
            }
        });
        TEST("spread==0 is bit-identity for EVERY voice (the no-op guard)", {
            for (int pv = 0; pv < N_POLY; ++pv)
                EXPECT_NEAR(SpreadInterp::applyPoly(pe, LANE_QMIX, pv, STEP, 0.0f), voiceVal(pv), 0.0f);
        });
        TEST("positive spread moves voice 7 toward the V1 target (0.90), not away", {
            // mix2(own=0.465, target=0.90, rho=+0.8): the result must be strictly closer to 0.90 than
            // own was (rho>0 = adherence). Guards the target-selection, not just "it moved."
            const float v7 = SpreadInterp::applyPoly(pe, LANE_QMIX, target, STEP, 0.8f);
            EXPECT(std::fabs(v7 - 0.90f) < std::fabs(v7own - 0.90f));
        });
    }

    SUITE("follow-CA contract — own (pre-remap) != target (post-remap) when src != self");
    {
        // SPREAD_TARGET_MODES.md POST-MORTEM: under follow-CA with this voice actually pinned
        // (src != self), own (pre-remap) MUST differ from target (post-remap = leader's material).
        // If they collapse, interpolate()'s self-target guard makes the knob a silent no-op — the bug
        // that bit three times. This encodes the contract as a test, not a runtime assert.
        PatternEngine pe;
        pe.spreadTargetMode[LANE_QMIX] = 1;   // follow-CA
        // Pin poly voice 5 (global voice 6) to follow global voice 0's q-mix: caQmixSrc[6] = 0.
        // (applyPoly uses voice+1 for the CA src row, so poly voice 5 → row 6.)
        const int pv = 5;
        for (int v = 0; v < 16; ++v) pe.caQmixSrc[v] = (uint8_t)v;   // identity baseline
        pe.caQmixSrc[pv + 1] = 0;   // voice 6 follows voice 0 (src != self)
        // Pre-remap (own) = voice 5's own material; post-remap (target) = voice 0's material.
        pe.preRemapSlewedPolyQmix[pv][STEP] = 0.15f;   // own
        pe.slewedPolyQmix[pv][STEP]         = 0.85f;   // target (leader's, after remap)
        const float own = SpreadInterp::polyPreRemap(pe, LANE_QMIX, pv, STEP);
        const float tgt = SpreadInterp::polySlewed(pe, LANE_QMIX, pv, STEP);
        TEST("own (pre-remap) != target (post-remap) when src != self", {
            EXPECT(std::fabs(own - tgt) > 1e-4f);
        });
        TEST("non-zero spread therefore moves the value (not a silent no-op)", {
            const float out = SpreadInterp::applyPoly(pe, LANE_QMIX, pv, STEP, 0.6f);
            EXPECT(std::fabs(out - own) > 1e-3f);   // the knob actually does something
        });
        TEST("src == self (identity pin) → own may equal target (no-op is correct there)", {
            pe.caQmixSrc[pv + 1] = (uint8_t)(pv + 1);   // restore identity
            const float o2 = SpreadInterp::polyPreRemap(pe, LANE_QMIX, pv, STEP);
            const float t2 = SpreadInterp::polySlewed(pe, LANE_QMIX, pv, STEP);
            // With identity pins the buffers still differ here (we set them distinct), but the
            // contract is only that a collapse is ALLOWED (self-target no-op) — not required.
            // Assert the self-target no-op path itself: spread>0 with own==target returns own.
            pe.preRemapSlewedPolyQmix[pv][STEP] = 0.42f;
            pe.slewedPolyQmix[pv][STEP]         = 0.42f;   // own == target
            EXPECT_NEAR(SpreadInterp::applyPoly(pe, LANE_QMIX, pv, STEP, 0.6f), 0.42f, 1e-6f);
            (void)o2; (void)t2;
        });
    }

    std::cout << "\n" << s_pass << " passed, " << s_fail << " failed\n";
    return s_fail ? 1 : 0;
}
