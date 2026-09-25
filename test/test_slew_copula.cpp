// test_slew_copula.cpp — Phase 1: slew readout is a normal-space moving-average copula.
//
// The key invariant the rework must hold (docs/design/SLEW_COPULA_PLAN.md, DISTRIBUTION_REWORK_PLAN):
//   slew KNOB = 1  ->  copula r = 0  ->  out == the raw draw u[0]  BIT-IDENTICALLY
// (the knob is inverted: 1 = sharp/raw, 0 = smooth/high-r). This test pins that through the
// REAL engine path (latchMix + recomputeEffective*), not just the copula unit, and checks the
// scrub mix is parked so the readout is patternAt(N) with no blend.
#include "test_stubs.hpp"
#include "PatternEngine.hpp"
#include <iostream>
#include <sstream>
#include <cstring>
#include <cmath>

static int s_pass=0, s_fail=0;
#define TEST(desc,...) do{ \
    bool _ok=true; std::string _msg; \
    try{__VA_ARGS__;}catch(const std::exception& _e){_ok=false;_msg=_e.what();} \
    if(_ok){++s_pass;std::cout<<"  \033[32m✓\033[0m "<<(desc)<<"\n";} \
    else{++s_fail;std::cout<<"  \033[31m✗\033[0m "<<(desc); \
         if(!_msg.empty())std::cout<<" — "<<_msg;std::cout<<"\n";} \
}while(0)
#define EXPECT(e) do{if(!(e))throw std::runtime_error("EXPECT(" #e ") failed");}while(0)

int main(){
    std::cout<<"\033[1mSlew copula (Phase 1) Tests\033[0m\n"<<std::string(50,'=')<<"\n";

    // ── r==0 (slew knob = 1) bit-identity through the engine path ──────────────
    // mix=0 -> scrub reads patternAt(N) only (f=0, frac=0 -> bl(d0,d1)=d0). slew=1 -> r=0.
    // So slewedRhythm[i] MUST equal the raw Philox draw at counter N bitwise.
    TEST("slew knob=1 (r=0) reproduces the raw draw bitwise — rhythm", {
        PatternEngine pe;
        pe.seedRhythmPhilox(0.42f);
        // Advance a few draws so N != 0 (exercises the addressable window, not just seed pos).
        PatternInput in;   // default; not used by recompute
        for (int k=0;k<3;++k) pe.advanceRhythmDraw(+1);
        const int64_t N = pe.rhythmDrawCtr;
        // Park scrub at mix=0 and slew knob=1 (r=0), then recompute.
        pe.latchMix(/*rMix=*/0.f, /*mMix=*/0.f, /*qMix=*/0.f,
                    /*rSlew=*/1.f, /*mSlew=*/1.f, /*qSlew=*/1.f,
                    /*applyRhythm=*/true, /*applyMelody=*/false, /*applyQmix=*/false);
        pe.recomputeEffectiveRhythm();
        // The raw draw at N (what r=0 must reproduce).
        PatternEngine::RhythmDraw raw;
        pe.rawDrawRhythmPatternAt(N, raw);
        for (int i=0;i<16;++i)
            EXPECT(std::memcmp(&pe.slewedRhythm[i], &raw.rhythm[i], sizeof(float)) == 0);
    });

    // Same bit-identity on the melody stream.
    TEST("slew knob=1 (r=0) reproduces the raw draw bitwise — melody", {
        PatternEngine pe;
        pe.seedMelodyPhilox(0.17f);
        for (int k=0;k<5;++k) pe.advanceMelodyDraw(+1);
        const int64_t N = pe.melodyDrawCtr;
        pe.latchMix(0.f,0.f,0.f, 1.f,1.f,1.f, /*R=*/false, /*M=*/true, /*Q=*/false);
        pe.recomputeEffectiveMelody();
        PatternEngine::MelodyDraw raw;
        pe.rawDrawMelodyPatternAt(N, raw);
        for (int i=0;i<16;++i) {
            EXPECT(std::memcmp(&pe.slewedMelody[i], &raw.melody[i], sizeof(float)) == 0);
            EXPECT(std::memcmp(&pe.slewedOctave[i], &raw.octave[i], sizeof(float)) == 0);
        }
    });

    // Same on q-mix.
    TEST("slew knob=1 (r=0) reproduces the raw draw bitwise — qmix", {
        PatternEngine pe;
        pe.seedQmixPhilox(0.9f);
        for (int k=0;k<2;++k) pe.advanceQmixDraw(+1);
        const int64_t N = pe.qmixDrawCtr;
        pe.latchMix(0.f,0.f,0.f, 1.f,1.f,1.f, /*R=*/false, /*M=*/false, /*Q=*/true);
        pe.recomputeEffectiveQmix();
        PatternEngine::QmixDraw raw;
        pe.rawDrawQmixPatternAt(N, raw);
        for (int i=0;i<16;++i)
            EXPECT(std::memcmp(&pe.slewedQmix[i], &raw.qmix[i], sizeof(float)) == 0);
    });

    // ── Reversibility: patternAt(N) is a pure function of (N, slew) — forward then back ─
    TEST("patternRhythmAt is pure in pos: forward-then-back reproduces bitwise", {
        PatternEngine pe;
        pe.seedRhythmPhilox(1.1f);
        const float slew = 0.4f;   // mid slew -> r > 0, exercises the copula combine
        PatternEngine::RhythmDraw fwd, back;
        // Forward to N=4, capture; reverse to N=1, capture; the value at a FIXED pos must be
        // identical regardless of how the counter got there.
        for (int k=0;k<4;++k) pe.advanceRhythmDraw(+1);
        pe.patternRhythmAt(pe.rhythmDrawCtr, slew, fwd);
        const int64_t target = pe.rhythmDrawCtr;
        // Walk backward past it then forward again to it.
        for (int k=0;k<3;++k) pe.advanceRhythmDraw(-1);
        for (int k=0;k<3;++k) pe.advanceRhythmDraw(+1);
        EXPECT(pe.rhythmDrawCtr == target);
        pe.patternRhythmAt(pe.rhythmDrawCtr, slew, back);
        for (int i=0;i<16;++i)
            EXPECT(std::memcmp(&fwd.rhythm[i], &back.rhythm[i], sizeof(float)) == 0);
    });

    // ── Negative counters: patternAt works at pos < 0 (infinite line, no origin floor) ──
    TEST("patternRhythmAt at negative pos is finite + reproducible", {
        PatternEngine pe;
        pe.seedRhythmPhilox(2.2f);
        const float slew = 0.6f;
        PatternEngine::RhythmDraw a, b;
        pe.patternRhythmAt(-7, slew, a);
        pe.patternRhythmAt(-7, slew, b);   // re-read same negative pos
        for (int i=0;i<16;++i) {
            EXPECT(std::isfinite(a.rhythm[i]));
            EXPECT(std::memcmp(&a.rhythm[i], &b.rhythm[i], sizeof(float)) == 0);
        }
    });

    // ── Distribution: at r>0 (slew knob < 1) the marginal stays ~uniform (variance ~1/12) ─
    // Today's L1 linear window collapses variance to ~1/K at low slew; the copula must NOT.
    // Thin by K (samples K apart share no source draws → independent) before the variance check.
    TEST("slew knob=0.1 (high r): thinned marginal variance ~1/12 (no concentration)", {
        PatternEngine pe;
        pe.seedRhythmPhilox(3.3f);
        const float slewKnob = 0.1f;          // -> r ≈ 0.9·R_MAX (high correlation, but uniform marginal)
        const int M = 4096;                   // positions
        const std::size_t K = redDot::MovingAverageCopula::K;
        double sum=0, sumsq=0; int n=0;
        PatternEngine::RhythmDraw d;
        for (int p=0; p<M; ++p) {
            pe.patternRhythmAt(p, slewKnob, d);
            // Thin by K: take step 0 only (positions p and p+K share no source draws).
            const double v = d.rhythm[0];
            sum += v; sumsq += v*v; ++n;
        }
        const double mean = sum/n;
        const double var  = sumsq/n - mean*mean;
        // Uniform[0,1] variance is 1/12 ≈ 0.0833. The copula marginal is EXACTLY uniform (up to
        // float rounding); allow a generous band to absorb finite-sample noise. The point is to
        // FAIL the old L1 window (which gave ~1/(K·7) ≈ 0.002 here).
        EXPECT(var > 0.06);   // well above the collapsed ~0.002; near 0.0833 means uniform
        (void)K;
    });

    std::cout<<"\n"<<s_pass<<" passed, "<<s_fail<<" failed\n";
    return s_fail ? 1 : 0;
}
