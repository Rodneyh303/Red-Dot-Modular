#include "test_stubs.hpp"
/**
 * test_no_preference_fallback.cpp
 * ------------------------------------------------------------------
 * THE FALLBACK-LADDER RULING (Rodney, 2026-11-28) — one degenerate rule for the
 * whole quantiser/generative family:
 *
 *   ALL FADERS DOWN = NO PREFERENCE = EQUAL FADERS OVER THE GOVERNING SCALE SET.
 *
 * The substitution happens at the single point where weights enter each path
 * (NoPreferenceFallbackCore.hpp); everything downstream — the pairwise weighted
 * partition for the quantiser, warp/distribution distortion of the pick draw r
 * for generated notes — operates unchanged on the substituted uniform weights.
 * Uniform weights through the pairwise partition ARE exact nearest-note snapping
 * (equal weights => midpoint boundaries), so "snap to the nearest note of the
 * selected scale" needs no second algorithm.
 *
 * Also pinned here: THE OCTAVE-WRAP MEMO (same ruling thread). The in-range remap
 * that folds a margin winner back into [vLo,vHi] records its fold in
 * QuantResult::wrapDelta (0 mid-range; +1 below-range folded up; -1 above-range
 * folded down). The both-external row keeps the folded absolute answer; the two
 * MIXED rows apply the fold's inverse, so "just below the bottom C" sounds as the
 * B just BELOW the generated register (not the B at its top), and a generated
 * degree seats in the octave the external CV actually pointed at.
 *
 * Suites:
 *   S1  NoPreferenceFallbackCore helpers (weighted regime byte-identity, uniform
 *       fallback over members, FB6 empty-membership defensive rung).
 *   S2  FB1: uniform weights == exact nearest-member snap, and weight-scale
 *       invariance (1.0 vs 0.37 uniform: bit-identical picks).
 *   S3  Engine quantiser fallback: rebuildScaleCache(all-zero, members) fills the
 *       fallback set; quantizeResolve snaps to the nearest member (scale-snap
 *       REPLACES the old raw passthrough — the retired behaviour).
 *   S4  Generative: substituted uniform == explicit equal faders for EVERY draw r
 *       (pick proportionality), which is also the "warp still distorts" proof —
 *       warp only changes which r arrives, and the mapping is identical.
 *       Monotone-C degenerate drone is dead.
 *   S5  Wrap memo in the core: mid-range always 0; bottom-edge fold-up = +1 with
 *       the folded volts; top-edge fold-down = -1.
 *   S6  Mixed rows consume the memo (voicePitch): same generated terms, CV at the
 *       bottom edge vs mid-range differ by exactly -1 V in BOTH mixed rows; the
 *       both-external row keeps the folded absolute answer.
 *   S7  ORD: degree order never implies pitch order — permuting (positions,
 *       weights) leaves the quantised volts bit-identical (index maps through
 *       the permutation). A Micro tuning may be non-monotone in degree index.
 *
 * Compile: see test/run_all.sh ("test_no_preference_fallback|$SE $GS $PE").
 */
#include "PatternEngine.hpp"
#include "SequencerEngine.hpp"
#include "NoPreferenceFallbackCore.hpp"
#include "QuantiserPartitionCore.hpp"
#include "VoiceResolver.hpp"
#include <iostream>
#include <string>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <stdexcept>

static int s_pass = 0, s_fail = 0;
#define SUITE(n) do{std::cout<<"\n\033[1;34m["<<(n)<<"]\033[0m\n";}while(0)
#define TEST(desc,...) do{ \
    bool _ok=true; std::string _msg; \
    try{__VA_ARGS__;}catch(const std::exception& _e){_ok=false;_msg=_e.what();} \
    if(_ok){++s_pass;std::cout<<"  \033[32mPASS\033[0m "<<(desc)<<"\n";} \
    else{++s_fail;std::cout<<"  \033[31mFAIL\033[0m "<<(desc); \
         if(!_msg.empty())std::cout<<" -- "<<_msg;std::cout<<"\n";} \
}while(0)
#define EXPECT(e) do{if(!(e))throw std::runtime_error("EXPECT(" #e ") failed");}while(0)

static bool biteq(float a, float b){ uint32_t x,y; std::memcpy(&x,&a,4); std::memcpy(&y,&b,4); return x==y; }

// Brute-force nearest-instance oracle over [vLo-1, vHi+1] instances of the member
// positions (ties -> lower instance, mirroring the partition walk's <= boundary),
// folded into range the same way the core folds.
static float bruteNearestMember(const float* pos, int m, float vIn, float vLo, float vHi) {
    float best = 1e9f, bestC = 0.f; float bestPos = 0.f;
    for (int o = (int)std::floor(vLo) - 1; o <= (int)std::floor(vHi) + 1; ++o) {
        for (int i = 0; i < m; ++i) {
            const float c = (float)o + pos[i];
            if (c < vLo - 1.f || c > vHi + 1.f) continue;
            const float d = std::fabs(vIn - c);
            if (d < best - 1e-7f || (std::fabs(d - best) <= 1e-7f && c < bestC)) {
                best = d; bestC = c; bestPos = pos[i];
            }
        }
    }
    float v = bestC; int guard = 0;
    while (v < vLo && guard++ < 4) v += 1.f;
    while (v > vHi && guard++ < 4) v -= 1.f;
    (void)bestPos;
    return v;
}

int main() {
    std::cout << "No-preference fallback + octave-wrap memo (fallback-ladder ruling)\n";

    // ════ S1: core helpers ═══════════════════════════════════════════════════
    SUITE("S1. NoPreferenceFallbackCore helpers");
    {
        // Weighted regime: byte-identical to the legacy w>0 filter.
        TEST("weighted regime == legacy filter (list, weights, count)", {
            const float w[6] = {0.f, 0.3f, 0.f, 1.f, 0.0f, 0.75f};
            int list[6]; float out[6];
            const int m = redDot::buildActiveDegreeSet(w, nullptr, 6, list, out);
            EXPECT(m == 3);
            EXPECT(list[0] == 1 && biteq(out[0], 0.3f));
            EXPECT(list[1] == 3 && biteq(out[1], 1.f));
            EXPECT(list[2] == 5 && biteq(out[2], 0.75f));
        });
        TEST("all-zero + members -> uniform 1 over exactly the members", {
            const float w[12] = {};
            bool members[12] = {};
            const int maj[7] = {0,2,4,5,7,9,11};
            for (int i = 0; i < 7; ++i) members[maj[i]] = true;
            int list[12]; float out[12];
            const int m = redDot::buildActiveDegreeSet(w, members, 12, list, out);
            EXPECT(m == 7);
            for (int i = 0; i < 7; ++i) { EXPECT(list[i] == maj[i]); EXPECT(biteq(out[i], 1.f)); }
        });
        TEST("all-zero + null members -> all n, uniform", {
            const float w[5] = {};
            int list[5]; float out[5];
            const int m = redDot::buildActiveDegreeSet(w, nullptr, 5, list, out);
            EXPECT(m == 5);
            for (int i = 0; i < 5; ++i) { EXPECT(list[i] == i); EXPECT(biteq(out[i], 1.f)); }
        });
        TEST("FB6: all-zero + ALL-FALSE members -> all n (quantiser always has a target)", {
            const float w[4] = {};
            const bool members[4] = {false,false,false,false};
            int list[4]; float out[4];
            const int m = redDot::buildActiveDegreeSet(w, members, 4, list, out);
            EXPECT(m == 4);
            for (int i = 0; i < 4; ++i) EXPECT(list[i] == i && biteq(out[i], 1.f));
        });
        TEST("substituteUniformWeights: any weight up -> untouched, returns false", {
            float w[4] = {0.f, 0.001f, 0.f, 0.f};
            const bool members[4] = {true,true,true,true};
            EXPECT(!redDot::substituteUniformWeights(w, members, 4));
            EXPECT(biteq(w[1], 0.001f) && biteq(w[0], 0.f));
        });
        TEST("substituteUniformWeights: all-zero -> members 1, non-members 0, returns true", {
            float w[4] = {};
            const bool members[4] = {true,false,true,false};
            EXPECT(redDot::substituteUniformWeights(w, members, 4));
            EXPECT(biteq(w[0],1.f) && biteq(w[1],0.f) && biteq(w[2],1.f) && biteq(w[3],0.f));
        });
        TEST("substituteUniformWeights FB6: all-false members -> all 1", {
            float w[3] = {};
            const bool members[3] = {false,false,false};
            EXPECT(redDot::substituteUniformWeights(w, members, 3));
            EXPECT(biteq(w[0],1.f) && biteq(w[1],1.f) && biteq(w[2],1.f));
        });
    }

    // ════ S2: FB1 — uniform == exact nearest-member snap ═══════════════════════
    SUITE("S2. FB1: uniform weights ARE nearest-note snapping (one algorithm)");
    {
        const int maj[7] = {0,2,4,5,7,9,11};
        float pos[7]; for (int i = 0; i < 7; ++i) pos[i] = maj[i] / 12.f;
        float w1[7], wC[7];
        for (int i = 0; i < 7; ++i) { w1[i] = 1.f; wC[i] = 0.37f; }

        TEST("uniform-1 == uniform-0.37, bit-identical across a 2000-pt sweep (scale invariance)", {
            for (int k = 0; k <= 2000; ++k) {
                const float v = 5.f * (float)k / 2000.f;
                const auto a = redDot::weightedQuantize(pos, w1, 7, v, 0.f, 5.f);
                const auto b = redDot::weightedQuantize(pos, wC, 7, v, 0.f, 5.f);
                EXPECT(a.index == b.index && a.octave == b.octave && biteq(a.volts, b.volts));
            }
        });
        TEST("uniform == brute-force nearest member (off-boundary sweep)", {
            for (int k = 0; k <= 1999; ++k) {
                // off-midpoint offsets: avoid landing within 1e-3 of any midpoint seam
                const float v = 5.f * ((float)k + 0.137f) / 2000.f;
                const auto a = redDot::weightedQuantize(pos, w1, 7, v, 0.f, 5.f);
                const float o = bruteNearestMember(pos, 7, v, 0.f, 5.f);
                EXPECT(std::fabs(a.volts - o) < 1e-4f);
            }
        });
    }

    // ════ S3: engine quantiser fallback — scale-snap replaces raw passthrough ══
    SUITE("S3. rebuildScaleCache fallback set + quantizeResolve scale-snap");
    {
        SequencerEngine eng;
        eng.pe.tuning.resetToEqual12TET();
        const int maj[7] = {0,2,4,5,7,9,11};
        bool members[12] = {};
        for (int i = 0; i < 7; ++i) members[maj[i]] = true;
        const float zeros[12] = {};

        TEST("all-zero weights + members -> fallback set = members, uniform weight 1", {
            eng.rebuildScaleCache(zeros, 12, members);
            EXPECT(eng.activeSemiCount == 7);
            for (int i = 0; i < 7; ++i) {
                EXPECT(eng.activeSemiList[i] == maj[i]);
                EXPECT(biteq(eng.activeSemiWeight[i], 1.f));
            }
        });
        TEST("quantizeResolve snaps to the NEAREST MAJOR note (retired: raw passthrough)", {
            float majPos[7]; for (int i = 0; i < 7; ++i) majPos[i] = maj[i] / 12.f;
            bool sawNonInput = false;
            for (int k = 0; k <= 999; ++k) {
                const float v = 5.f * ((float)k + 0.291f) / 1000.f;
                const auto q = eng.quantizeResolve(v);
                // output is a member degree...
                bool isMember = false;
                for (int i = 0; i < 7; ++i) isMember = isMember || (q.semitone == maj[i]);
                EXPECT(isMember);
                // ...at the brute-force nearest member voltage
                const float o = bruteNearestMember(majPos, 7, v, 0.f, 5.f);
                EXPECT(std::fabs(q.volts - o) < 1e-4f);
                if (std::fabs(q.volts - v) > 1e-3f) sawNonInput = true;
            }
            EXPECT(sawNonInput);   // it QUANTISES — raw passthrough would echo v
        });
        TEST("all-zero + no members (nullptr) -> chromatic snap over all 12", {
            eng.rebuildScaleCache(zeros, 12, nullptr);
            EXPECT(eng.activeSemiCount == 12);
        });
        TEST("weights up + members -> weighted regime untouched (byte-identical filter)", {
            float w[12] = {};
            w[0] = 0.25f; w[7] = 1.f;
            eng.rebuildScaleCache(w, 12, members);
            EXPECT(eng.activeSemiCount == 2);
            EXPECT(eng.activeSemiList[0] == 0 && biteq(eng.activeSemiWeight[0], 0.25f));
            EXPECT(eng.activeSemiList[1] == 7 && biteq(eng.activeSemiWeight[1], 1.f));
        });
        TEST("defensive cold path still clamps (fresh engine, count forced 0)", {
            eng.activeSemiCount = 0;
            const auto q = eng.quantizeResolve(3.3f);
            EXPECT(std::fabs(q.volts - 3.3f) < 1e-6f);
            eng.rebuildScaleCache(zeros, 12, members);   // restore
        });
    }

    // ════ S4: generative — substituted uniform == explicit equal faders ════════
    SUITE("S4. generative: all-down == equal faders for EVERY draw r (warp-proof)");
    {
        SequencerEngine eng;
        eng.pe.tuning.resetToEqual12TET();
        const int maj[7] = {0,2,4,5,7,9,11};
        bool members[12] = {};
        for (int i = 0; i < 7; ++i) members[maj[i]] = true;

        TEST("pickSemitone(substituted) == pickSemitone(explicit 0.7 equal), all r", {
            float wSub[dotModular::TuningTable::MAXN] = {};
            redDot::substituteUniformWeights(wSub, members, 12);
            float wEq[dotModular::TuningTable::MAXN] = {};
            for (int i = 0; i < 7; ++i) wEq[maj[i]] = 0.7f;
            bool sawAll[12] = {};
            for (int k = 0; k <= 5000; ++k) {
                const float r = (float)k / 5000.f;
                const int a = eng.pe.pickSemitone(wSub, 12, r);
                const int b = eng.pe.pickSemitone(wEq, 12, r);
                EXPECT(a == b);
                EXPECT(a >= 0);                        // the pick NEVER fails now
                if (a >= 0 && a < 12) sawAll[a] = true;
            }
            for (int i = 0; i < 12; ++i) EXPECT(sawAll[i] == members[i]);   // exactly the scale sounds
        });
        TEST("monotone-C drone is dead: substituted weights vary the pick with r", {
            float wSub[dotModular::TuningTable::MAXN] = {};
            redDot::substituteUniformWeights(wSub, members, 12);
            EXPECT(eng.pe.pickSemitone(wSub, 12, 0.01f) != eng.pe.pickSemitone(wSub, 12, 0.99f));
        });
    }

    // ════ S5: the octave-wrap memo in the core ═════════════════════════════════
    SUITE("S5. wrapDelta: 0 mid-range; +1 bottom fold-up; -1 top fold-down");
    {
        TEST("mid-range chromatic uniform sweep: wrapDelta always 0 (goldens' regime)", {
            float pos[12], w[12];
            for (int i = 0; i < 12; ++i) { pos[i] = i / 12.f; w[i] = 1.f; }
            for (int k = 0; k <= 2000; ++k) {
                const float v = 5.f * (float)k / 2000.f;
                const auto q = redDot::weightedQuantize(pos, w, 12, v, 0.f, 5.f);
                EXPECT(q.wrapDelta == 0);
            }
        });
        TEST("bottom edge: heavy B wins via the wrap-below instance -> fold-up, wrapDelta=+1", {
            // {B w=1, D w=0.01}: boundary between B@-0.083 and D@0.167 sits at
            // -0.083 + 0.25*(1/1.01) ~= 0.164, so vIn=0.01 belongs to the B below range.
            const float pos[2] = {11.f/12.f, 2.f/12.f};
            const float w[2]   = {1.f, 0.01f};
            const auto q = redDot::weightedQuantize(pos, w, 2, 0.01f, 0.f, 5.f);
            EXPECT(q.index == 0);                          // B
            EXPECT(q.wrapDelta == +1);                     // folded UP into range
            EXPECT(std::fabs(q.volts - 11.f/12.f) < 1e-5f);
            EXPECT(q.octave == 0);
        });
        TEST("top edge: heavy C# wins via the wrap-above instance -> fold-down, wrapDelta=-1", {
            const float pos[2] = {1.f/12.f, 9.f/12.f};
            const float w[2]   = {1.f, 0.01f};
            const auto q = redDot::weightedQuantize(pos, w, 2, 4.99f, 0.f, 5.f);
            EXPECT(q.index == 0);                          // C#
            EXPECT(q.wrapDelta == -1);                     // folded DOWN into range
            EXPECT(std::fabs(q.volts - (4.f + 1.f/12.f)) < 1e-5f);
            EXPECT(q.octave == 4);
        });
        TEST("equal weights never fold (wrap boundary ON the range edge): both edges", {
            float pos[12], w[12];
            for (int i = 0; i < 12; ++i) { pos[i] = i / 12.f; w[i] = 0.5f; }
            const auto a = redDot::weightedQuantize(pos, w, 12, 0.0001f, 0.f, 5.f);
            const auto b = redDot::weightedQuantize(pos, w, 12, 4.9999f, 0.f, 5.f);
            EXPECT(a.wrapDelta == 0 && b.wrapDelta == 0);
        });
    }

    // ════ S6: the mixed rows consume the memo (voicePitch) ═════════════════════
    SUITE("S6. mixed rows: fold's inverse rides the OTHER octave source");
    {
        SequencerEngine eng;
        eng.pe.tuning.resetToEqual12TET();
        eng.quantiserPitchSource = true;
        for (int i = 0; i < SequencerEngine::kVoiceSlots; ++i) eng.quantiserCV[i] = 0.f;

        // Active set: {B w=1, D w=0.01} — vIn=0.01 folds (wrapDelta +1, frac 0.917),
        // vIn=0.50 stays mid-range (wrapDelta 0, SAME degree B, SAME frac 0.917):
        // boundary B/D ~= 0.164 and the next boundary (D@0.167 .. B@0.917) sits at
        // 0.167 + 0.75*(0.01/1.01) ~= 0.174, so 0.50 belongs to the in-range B.
        float w[12] = {};
        w[11] = 1.f; w[2] = 0.01f;
        eng.rebuildScaleCache(w, 12, nullptr);

        PatternInput in{};
        in.octaveLo = 4.f; in.octaveHi = 4.f;              // octTerm = 4-4 = 0, deterministic
        in.semiWeights[3] = 1.f;                           // generated degree: D# (degTerm 3/12)
        const auto mono = dotModular::VoiceResolver::mono();
        const float rs = 0.3f, ro = 0.25f;

        TEST("deg EXT + oct GEN: folded CV sounds ONE OCTAVE BELOW the mid-range CV (B below the register)", {
            int s1 = -1, s2 = -1;
            eng.quantiserCV[0] = 0.01f;                    // wrap case
            const float vWrap = eng.voicePitch(mono, s1, in, rs, ro, false, true);
            eng.quantiserCV[0] = 0.50f;                    // mid-range, same degree+frac
            const float vMid  = eng.voicePitch(mono, s2, in, rs, ro, false, true);
            EXPECT(s1 == 11 && s2 == 11);                  // both picked B
            EXPECT(std::fabs(vMid - (0.f + 11.f/12.f)) < 1e-5f);     // octTerm 0 + frac
            EXPECT(std::fabs((vMid - vWrap) - 1.f) < 1e-5f);         // the memo: exactly -1 octave
            EXPECT(std::fabs(vWrap - (-1.f/12.f)) < 1e-5f);          // B just BELOW the register
        });
        TEST("deg GEN + oct EXT: generated degree seats in the UNFOLDED octave (-1 at the bottom wrap)", {
            int s1 = -1, s2 = -1;
            eng.quantiserCV[0] = 0.01f;
            const float vWrap = eng.voicePitch(mono, s1, in, rs, ro, true, false);
            eng.quantiserCV[0] = 0.50f;
            const float vMid  = eng.voicePitch(mono, s2, in, rs, ro, true, false);
            EXPECT(s1 == 3 && s2 == 3);                    // generated degree names outSem
            EXPECT(std::fabs(vMid - (0.f + 3.f/12.f)) < 1e-5f);      // q.octave 0 + degTerm
            EXPECT(std::fabs((vMid - vWrap) - 1.f) < 1e-5f);         // unfolded octave: -1
        });
        TEST("BOTH-EXTERNAL keeps the FOLDED absolute answer (must live in 0..5)", {
            int s = -1;
            eng.quantiserCV[0] = 0.01f;
            const float v = eng.voicePitch(mono, s, in, rs, ro, false, false);
            EXPECT(s == 11);
            EXPECT(std::fabs(v - 11.f/12.f) < 1e-5f);      // folded B at 0.917, in range
        });
    }

    // ════ S7: ORD — degree order never implies pitch order ═════════════════════
    SUITE("S7. permutation invariance: scrambled tuning == sorted tuning");
    {
        TEST("non-monotone (positions, weights) permutation: volts bit-identical, index maps", {
            // A deliberately scrambled 5-degree micro tuning (pitch order != degree order).
            const float posA[5] = {0.00f, 0.45f, 0.10f, 0.80f, 0.30f};
            const float wA[5]   = {1.0f,  0.20f, 1.0f,  0.50f, 0.90f};
            // Sorted-by-pitch arrangement of the same instrument.
            const int   perm[5] = {0, 2, 4, 1, 3};         // A-index of the k-th sorted degree
            float posB[5], wB[5];
            for (int k = 0; k < 5; ++k) { posB[k] = posA[perm[k]]; wB[k] = wA[perm[k]]; }
            for (int k = 0; k <= 2000; ++k) {
                const float v = 5.f * ((float)k + 0.317f) / 2001.f;
                const auto a = redDot::weightedQuantize(posA, wA, 5, v, 0.f, 5.f);
                const auto b = redDot::weightedQuantize(posB, wB, 5, v, 0.f, 5.f);
                EXPECT(biteq(a.volts, b.volts));
                EXPECT(a.octave == b.octave && a.wrapDelta == b.wrapDelta);
                EXPECT(a.index == perm[b.index]);          // same DEGREE through the permutation
            }
        });
    }

    std::cout << "\n" << (s_fail ? "\033[31m" : "\033[32m")
              << "pass=" << s_pass << " fail=" << s_fail << "\033[0m\n";
    return s_fail ? 1 : 0;
}
