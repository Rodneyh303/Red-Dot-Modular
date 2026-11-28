#include "test_stubs.hpp"
/**
 * test_quantiser_partition.cpp
 * The weighted-partition quantiser core (docs/design/QUANTISER_WEIGHTED_PARTITION.md).
 *
 * THE BUG (Rodney, 2026-11-23): under qmix with a full melody patched, a scale degree whose
 * fader is ~1% still sounds ~half the time. Root cause (verified): the old quantize gives each
 * degree a fixed capture radius = w/12 and, when the CV lands outside every radius, falls back
 * to NEAREST active degree by distance, WEIGHT IGNORED — so a faint low degree wins every CV
 * for which it is merely nearest.
 *
 * This file (a) RED-FIRST reproduces the symptom against a verbatim transcription of the old
 * quantize (the oracle), showing it gives a 1% degree ~50%, while the partition core gives
 * ~1%; (b) pins uniform-weight byte-identity (the goldens' regime); (c) the octave-seam +/-1;
 * (d) single degree, clamp edges, the relative-weight property.
 *
 * Compile: see test/run_all.sh (the registry is the source of truth).
 */
#include "dsp/QuantiserPartitionCore.hpp"
#include <iostream>
#include <sstream>
#include <cmath>
#include <vector>

static int s_pass=0, s_fail=0;
#define SUITE(n) do{std::cout<<"\n\033[1;34m["<<(n)<<"]\033[0m\n";}while(0)
#define TEST(desc,...) do{ \
    bool _ok=true; std::string _msg; \
    try{__VA_ARGS__;}catch(const std::exception& _e){_ok=false;_msg=_e.what();} \
    if(_ok){++s_pass;std::cout<<"  \033[32m✓\033[0m "<<(desc)<<"\n";} \
    else{++s_fail;std::cout<<"  \033[31m✗\033[0m "<<(desc); \
         if(!_msg.empty())std::cout<<" — "<<_msg;std::cout<<"\n";} \
}while(0)
#define EXPECT(e)       do{if(!(e))throw std::runtime_error("EXPECT(" #e ") failed");}while(0)
#define EXPECT_NEAR(a,b,e) do{if(std::fabs((a)-(b))>(e)){std::ostringstream s; \
    s<<#a<<"="<<(a)<<" not near "<<#b<<"="<<(b);throw std::runtime_error(s.str());}}while(0)

// ─────────────────────────────────────────────────────────────────────────────
// THE ORACLE — a VERBATIM transcription of SequencerEngine::quantize (the radius +
// score + weight-ignoring nearest fallback + [0,5] candidate skip). Independent of the
// core, so reproducing/diverging from it is meaningful. Returns the output voltage.
// ─────────────────────────────────────────────────────────────────────────────
static float oracleQuantize(const std::vector<int>& semis, const std::vector<float>& weights,
                            const std::vector<float>& posOfSemi, float vIn) {
    const int octave = (int)vIn;
    float bestScore = -1.f, bestV = vIn;
    auto degV = [&](int s){ return posOfSemi[(size_t)s]; };
    for (size_t ai = 0; ai < semis.size(); ++ai) {
        int s = semis[ai]; float w = weights[ai];
        float radius = w * (1.f/12.f);
        for (int o = octave-1; o <= octave+1; ++o) {
            float cand = o + degV(s);
            if (cand < 0.f || cand > 5.f) continue;
            float dist = std::fabs(vIn - cand);
            if (dist <= radius + 1e-4f) {
                float score = w / (dist + 1.f/12.f);
                if (score > bestScore) { bestScore = score; bestV = cand; }
            }
        }
    }
    if (bestScore < 0.f) {
        float minDist = 1e9f;
        for (size_t ai = 0; ai < semis.size(); ++ai) {
            int s = semis[ai];
            for (int o = octave-1; o <= octave+1; ++o) {
                float cand = o + degV(s);
                if (cand < 0.f || cand > 5.f) continue;
                float dist = std::fabs(vIn - cand);
                if (dist < minDist) { minDist = dist; bestV = cand; }
            }
        }
    }
    return bestV < 0.f ? 0.f : (bestV > 5.f ? 5.f : bestV);
}

// helpers to build core inputs from a 12-TET active set
static void build12(const std::vector<int>& semis, const std::vector<float>& weights,
                    std::vector<float>& pos /*out: positions aligned to semis*/,
                    std::vector<float>& w   /*out: weights aligned*/) {
    pos.clear(); w.clear();
    for (size_t i=0;i<semis.size();++i){ pos.push_back(semis[i]/12.f); w.push_back(weights[i]); }
}
// posOfSemi for the oracle: index by semitone 0..11
static std::vector<float> posOfSemi12() {
    std::vector<float> p(12); for (int s=0;s<12;++s) p[(size_t)s]=s/12.f; return p;
}

// share of a sweep across [0,5] that lands on target semitone (via the core)
static float coreShare(const std::vector<int>& semis, const std::vector<float>& weights,
                       int targetSemi, int samples=50000) {
    std::vector<float> pos, w; build12(semis, weights, pos, w);
    int hits=0;
    for (int k=0;k<samples;++k){
        float vIn = 5.f * (float)k / (float)(samples-1);
        auto r = redDot::weightedQuantize(pos.data(), w.data(), (int)semis.size(), vIn);
        if (semis[(size_t)r.index] == targetSemi) ++hits;
    }
    return (float)hits/(float)samples;
}
static float oracleShare(const std::vector<int>& semis, const std::vector<float>& weights,
                         int targetSemi, int samples=50000) {
    auto posS = posOfSemi12();
    int hits=0;
    for (int k=0;k<samples;++k){
        float vIn = 5.f * (float)k / (float)(samples-1);
        float out = oracleQuantize(semis, weights, posS, vIn);
        int deg = (int)std::lround((out - std::floor(out))*12.f) % 12;
        if (deg == targetSemi) ++hits;
    }
    return (float)hits/(float)samples;
}

int main(){
    std::cout<<"\033[1mWeighted-partition quantiser core\033[0m\n";
    std::cout<<std::string(62,'=')<<"\n";

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("G1 — THE SYMPTOM, red-first: a 1% degree, oracle ~50% vs core ~1%");
    // ─────────────────────────────────────────────────────────────────────────
    TEST("G1a {C@1%, G@100%}: oracle gives C ~half, the core gives C ~1%", {
        std::vector<int>   semis   = {0, 7};         // C, G
        std::vector<float> weights = {0.01f, 1.0f};
        float orc  = oracleShare(semis, weights, /*C=*/0);
        float core = coreShare  (semis, weights, /*C=*/0);
        // THE BUG: the old quantiser lands on a 1% C for a large fraction of a full sweep.
        EXPECT(orc  > 0.30f);    // observed ~0.5 — "on at least half the time"
        // THE FIX: the partition gives C its weight share (~1% of its neighbourhood).
        EXPECT(core < 0.03f);
        // and the fix is a >10x reduction
        EXPECT(orc > core * 10.f);
    });
    TEST("G1b sparse scale {C@1%, E,G,A @100%}: oracle inflates C, core ~1%", {
        std::vector<int>   semis   = {0, 4, 7, 9};
        std::vector<float> weights = {0.01f, 1.f, 1.f, 1.f};
        float orc  = oracleShare(semis, weights, 0);
        float core = coreShare  (semis, weights, 0);
        EXPECT(orc  > 0.15f);
        EXPECT(core < 0.03f);
    });
    TEST("G1c the faint degree's core share tracks its weight (1% vs 10% vs 50%)", {
        std::vector<int> semis = {0, 7};
        auto share=[&](float wC){ return coreShare(semis, {wC, 1.0f}, 0); };
        float s01 = share(0.01f), s10 = share(0.10f), s50 = share(0.50f);
        EXPECT(s01 < s10);
        EXPECT(s10 < s50);
        EXPECT(s01 < 0.05f);            // 1% weight -> small share
        EXPECT(s50 > 0.25f && s50 < 0.45f);  // 50% vs 100% neighbour -> ~1/3
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("G2 — UNIFORM weights: byte-identical to the oracle (the goldens' regime)");
    // ─────────────────────────────────────────────────────────────────────────
    TEST("G2a full chromatic, uniform: core volts == oracle volts across a dense sweep", {
        std::vector<int> semis; std::vector<float> weights;
        for (int s=0;s<12;++s){ semis.push_back(s); weights.push_back(1.0f); }
        std::vector<float> pos, w; build12(semis, weights, pos, w);
        auto posS = posOfSemi12();
        int checked=0;
        for (int k=0;k<20000;++k){
            // sample off the exact midpoints (measure-zero ties where a < / <= convention
            // could differ); real audio never sits exactly on a midpoint.
            float vIn = 5.f * ((float)k + 0.123f) / 20000.f;
            float orc = oracleQuantize(semis, weights, posS, vIn);
            auto r = redDot::weightedQuantize(pos.data(), w.data(), 12, vIn);
            EXPECT_NEAR(r.volts, orc, 1e-6f);
            ++checked;
        }
        EXPECT(checked == 20000);
    });
    TEST("G2b uniform sparse scale (pentatonic-ish), core == oracle", {
        std::vector<int> semis = {0,2,4,7,9}; std::vector<float> weights(5, 1.0f);
        std::vector<float> pos, w; build12(semis, weights, pos, w);
        auto posS = posOfSemi12();
        for (int k=0;k<20000;++k){
            float vIn = 5.f * ((float)k + 0.137f) / 20000.f;
            float orc = oracleQuantize(semis, weights, posS, vIn);
            auto r = redDot::weightedQuantize(pos.data(), w.data(), 5, vIn);
            EXPECT_NEAR(r.volts, orc, 1e-6f);
        }
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("G3 — the octave seam: the winner's octave falls out of the slice (+/-1)");
    // ─────────────────────────────────────────────────────────────────────────
    TEST("G3a {C@1.0, G@0.5}: 0.95 V snaps UP to C of the next octave (seam +1)", {
        std::vector<float> pos = {0.f, 7.f/12.f}, w = {1.0f, 0.5f};
        auto r = redDot::weightedQuantize(pos.data(), w.data(), 2, 0.95f);
        EXPECT(r.index == 0);                 // C
        EXPECT(r.octave == 1);                // next octave
        EXPECT_NEAR(r.volts, 1.0f, 1e-6f);
    });
    TEST("G3b {D@1%, B@100%}: 1.05 V snaps DOWN to B of the lower octave (seam -1)", {
        // D at 2/12=0.167 (w 0.01), B at 11/12=0.917 (w 1.0). strong B out-pulls faint D.
        std::vector<float> pos = {2.f/12.f, 11.f/12.f}, w = {0.01f, 1.0f};
        auto r = redDot::weightedQuantize(pos.data(), w.data(), 2, 1.05f);
        EXPECT(r.index == 1);                 // B
        EXPECT(r.octave == 0);                // lower octave
        EXPECT_NEAR(r.volts, 11.f/12.f, 1e-6f);
    });
    TEST("G3c seam shares split the octave in the weight ratio (C 2:1 over G)", {
        // C@1.0 / G@0.5 -> C should claim ~2/3 of the octave, G ~1/3.
        std::vector<int> semis = {0,7};
        float cShare = coreShare(semis, {1.0f, 0.5f}, 0);
        float gShare = coreShare(semis, {1.0f, 0.5f}, 7);
        EXPECT_NEAR(cShare, 2.f/3.f, 0.04f);
        EXPECT_NEAR(gShare, 1.f/3.f, 0.04f);
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("G4 — single degree, clamp edges, relative weight");
    // ─────────────────────────────────────────────────────────────────────────
    TEST("G4a one active degree owns the whole axis (every CV -> that degree)", {
        std::vector<float> pos = {7.f/12.f}, w = {0.3f};   // only G, any weight
        for (float vIn = 0.f; vIn <= 5.f; vIn += 0.013f) {
            auto r = redDot::weightedQuantize(pos.data(), w.data(), 1, vIn);
            EXPECT(r.index == 0);
            EXPECT_NEAR(r.volts, r.octave + 7.f/12.f, 1e-6f);
        }
    });
    TEST("G4b 0 V -> the lowest active degree at octave 0 (clamp tail absorb)", {
        // C muted (absent); lowest active is E. 0 V must map to E@0, not below range.
        std::vector<float> pos = {4.f/12.f, 7.f/12.f}, w = {1.f, 1.f};  // E, G
        auto r = redDot::weightedQuantize(pos.data(), w.data(), 2, 0.f);
        EXPECT(r.octave == 0);
        EXPECT_NEAR(r.volts, 4.f/12.f, 1e-6f);   // E, the lowest active degree
    });
    TEST("G4c near 5 V -> the highest in-range instance, never past 5", {
        std::vector<float> pos = {0.f, 7.f/12.f}, w = {1.f, 1.f};  // C, G
        auto r = redDot::weightedQuantize(pos.data(), w.data(), 2, 4.99f);
        EXPECT(r.volts <= 5.f + 1e-6f);
        EXPECT(r.volts >= 0.f);
    });
    TEST("G4e out-of-range vIn clamps to an IN-range instance (the center filter)", {
        // External CV can be negative or > 5. The output must stay in [0,5] and never snap to
        // an octave -1 / +6 instance. (Kills the drop-the-range-filter mutant.)
        std::vector<float> pos = {0.f, 7.f/12.f}, w = {1.f, 1.f};   // C, G
        auto below = redDot::weightedQuantize(pos.data(), w.data(), 2, -0.5f);
        EXPECT(below.volts >= 0.f && below.volts <= 5.f);
        EXPECT(below.octave >= 0);
        auto above = redDot::weightedQuantize(pos.data(), w.data(), 2, 5.5f);
        EXPECT(above.volts >= 0.f && above.volts <= 5.f);
    });
    TEST("G4d relative: two faint degrees still split the octave in their ratio", {
        // C@2% and G@1% only -> C ~2/3, G ~1/3 (faint is relative within the active set).
        std::vector<int> semis = {0,7};
        float cShare = coreShare(semis, {0.02f, 0.01f}, 0);
        float gShare = coreShare(semis, {0.02f, 0.01f}, 7);
        EXPECT_NEAR(cShare, 2.f/3.f, 0.05f);
        EXPECT_NEAR(gShare, 1.f/3.f, 0.05f);
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("G5 — POSITIONAL SYMMETRY (Rodney's feel-test: the lowest nonzero degree over-picked)");
    // The old core gave the LOWEST and HIGHEST in-range instance an unbounded bound
    // (lb_0 = -inf, ub_last = +inf), so each absorbed the whole unweighted tail
    // ([vLo, firstCenter) / (lastCenter, vHi]) — a full degree's worth regardless of
    // its fader. A faint degree that happened to be the lowest active (e.g. C# when C
    // is muted) was picked far above its weight, while a faint degree in the MIDDLE
    // (same weight, same unit neighbours) was not — a positional asymmetry. The margin
    // enumeration gives the edge instances real weighted boundaries, so a degree's
    // share no longer depends on WHERE it sits. RED on the old core, GREEN now.
    TEST("G5a lowest-active faint == middle faint (C muted; C# low vs G mid, both 1%)", {
        std::vector<int> act; for (int s = 1; s < 12; ++s) act.push_back(s);   // C (0) muted
        std::vector<float> w(act.size(), 1.f);
        w[0] = 0.01f;                        // C# — the LOWEST active degree
        w[6] = 0.01f;                        // G  — a MIDDLE active degree (act[6] == semitone 7)
        float sCs = coreShare(act, w, 1);    // C#
        float sG  = coreShare(act, w, 7);    // G
        EXPECT(std::fabs(sCs - sG) < 0.005f);   // SAME share regardless of position (the fix)
        EXPECT(sCs < 0.04f);                     // and small — no free tail (old core: ~0.09)
    });
    TEST("G5b same weight, same position-independence at the TOP (B high vs E mid, both 1%)", {
        std::vector<int> all; for (int s = 0; s < 12; ++s) all.push_back(s);
        std::vector<float> w(12, 1.f);
        w[11] = 0.01f;                       // B — the HIGHEST active degree
        w[4]  = 0.01f;                       // E — a MIDDLE active degree
        float sB = coreShare(all, w, 11);
        float sE = coreShare(all, w, 4);
        EXPECT(std::fabs(sB - sE) < 0.005f); // the top tail no longer inflates B
        EXPECT(sB < 0.03f);
    });


    // ─────────────────────────────────────────────────────────────────────────
    SUITE("G6 — ZERO-WEIGHT SELF-PROTECTION (Rodney: several zero neighbours need a fallback)");
    // The core must not trust the caller to pre-filter muted degrees. A w<=0 degree
    // passed in must be TRANSPARENT (identical to it being absent — the real neighbours
    // border each other and split their gap), never a winner, and an all-muted array
    // must return a finite in-range fallback (no 0/(0+0)=NaN boundary).
    TEST("G6a a muted degree passed in is transparent (== caller having filtered it)", {
        // with muted C(0) present vs absent, the sweep must map identically
        std::vector<int> withMuted = {0, 4, 7};      // C(muted), E, G
        std::vector<float> wWith   = {0.f, 1.f, 1.f};
        std::vector<int> without   = {4, 7};          // E, G
        std::vector<float> wWithout= {1.f, 1.f};
        std::vector<float> pW, wW, pO, wO;
        build12(withMuted, wWith, pW, wW);
        build12(without,  wWithout, pO, wO);
        int diffs = 0, mutedWins = 0;
        for (int k = 0; k < 20000; ++k) {
            float v = 5.f * (float)k / 19999.f;
            auto a = redDot::weightedQuantize(pW.data(), wW.data(), (int)withMuted.size(), v);
            auto b = redDot::weightedQuantize(pO.data(), wO.data(), (int)without.size(),  v);
            if (withMuted[(size_t)a.index] == 0) ++mutedWins;                 // C must never win
            if (withMuted[(size_t)a.index] != without[(size_t)b.index]) ++diffs;
            if (std::fabs(a.volts - b.volts) > 1e-4f) ++diffs;
        }
        EXPECT(mutedWins == 0);   // the muted degree never sounds
        EXPECT(diffs == 0);       // and its presence changes nothing — fully transparent
    });
    TEST("G6b all-muted array -> chromatic passthrough (no NaN; volts == clamp(vIn))", {
        std::vector<int> all = {0, 4, 7};
        std::vector<float> wz = {0.f, 0.f, 0.f};
        std::vector<float> pz, wzz; build12(all, wz, pz, wzz);
        bool ok = true;
        for (int k = 0; k <= 100; ++k) {
            float v = 5.f * (float)k / 100.f;
            auto r = redDot::weightedQuantize(pz.data(), wzz.data(), 3, v);
            if (!std::isfinite(r.volts)) ok = false;
            if (std::fabs(r.volts - v) > 1e-4f) ok = false;   // passthrough, not snapped
        }
        EXPECT(ok);
        // out-of-range passthrough clamps to the edge
        auto lo = redDot::weightedQuantize(pz.data(), wzz.data(), 3, -2.f);
        auto hi = redDot::weightedQuantize(pz.data(), wzz.data(), 3,  9.f);
        EXPECT(std::fabs(lo.volts - 0.f) < 1e-4f && std::fabs(hi.volts - 5.f) < 1e-4f);
    });

    std::cout<<"\n"<<std::string(62,'=')<<"\n";
    std::cout<<(s_fail==0?"\033[32m":"\033[31m")<<s_pass<<" passed, "<<s_fail
             <<" failed\033[0m\n";
    return s_fail==0 ? 0 : 1;
}

/* ── MUTATION LOG (to run once green) ─────────────────────────────────────────
 * Each mutant built to its own binary, build verified.
 *   M1  boundary ignores weights (always the midpoint: wA/(wA+wB) -> 0.5)
 *         → G1a/G1b/G1c FAIL (a 1% degree reverts to claiming ~half — the old bug) and
 *           G3c/G4d FAIL (shares stop tracking weight). G2 (uniform) still PASSES — which is
 *           the point: the midpoint IS correct for uniform, wrong for weighted.
 *   M2  boundary swaps the ratio (wB/(wA+wB))
 *         → G1a/G3c FAIL (the faint degree claims MORE, not less).
 *   M3  drop the [vLo,vHi] center filter (consider out-of-range instances)
 *         → G4b FAIL (0 V snaps to an octave -1 instance / below the lowest active degree).
 *   M4  winner scan uses `vIn <= ub` with the first instance's lower bound not -inf
 *         (i.e. require vIn >= lb_0) → G4b/G4c edge FAIL (tails not absorbed).
 * ─────────────────────────────────────────────────────────────────────────── */
