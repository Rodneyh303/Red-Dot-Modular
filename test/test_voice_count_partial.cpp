// test_voice_count_partial.cpp — voice-count opt: verify no garbage/NaN at PARTIAL count.
//
// Seeds a PatternEngine with numPolyVoicesHint < 15, draws patterns at slew!=0
// (the blend path that reads zPoly*), and asserts:
// 1. Active voices (v < hint) have valid finite values (not NaN/garbage)
// 2. Inactive voices (v >= hint) read 0 (zero-init belt-and-braces) not garbage
// 3. The active voices' values match what full-count would produce for those same voices
//    (voice-count gating is transparent — same output for active voices regardless of hint)
#include "engines/PatternEngine.hpp"
#include "engines/SequencerEngine.hpp"
#include <cstdio>
#include <cmath>

int main() {
    PatternEngine pe;
    pe.seedRhythmPhilox(3.14f);
    pe.seedMelodyPhilox(2.71f);
    pe.seedQmixPhilox(1.41f);
    pe.rhythmDrawCtr = 0; pe.beginRhythmDraw();
    pe.melodyDrawCtr = 0; pe.beginMelodyDraw();
    pe.qmixDrawCtr = 0; pe.beginQmixDraw();

    // Draw at full count (hint=15) — reference
    pe.numPolyVoicesHint = 15;
    PatternEngine::RhythmDraw rFull;
    PatternEngine::MelodyDraw mFull;
    PatternEngine::QmixDraw qFull;
    pe.patternRhythmAt(0, 0.5f, rFull);  // slew=0.5 → r!=0, blend path
    pe.patternMelodyAt(0, 0.5f, mFull);
    pe.patternQmixAt(0, 0.5f, qFull);

    // Draw at partial count (hint=4) — must match active voices
    pe.numPolyVoicesHint = 4;
    // Need a fresh cache (different hint → different fill). Clear and redraw.
    pe.rhythmDrawCache_.clear();
    pe.melodyDrawCache_.clear();
    pe.qmixDrawCache_.clear();
    PatternEngine::RhythmDraw rPart;
    PatternEngine::MelodyDraw mPart;
    PatternEngine::QmixDraw qPart;
    pe.patternRhythmAt(0, 0.5f, rPart);
    pe.patternMelodyAt(0, 0.5f, mPart);
    pe.patternQmixAt(0, 0.5f, qPart);

    int fail = 0;
    auto check = [&](const char* name, float got, float want, int v) {
        if (std::isnan(got) || std::isinf(got)) {
            printf("FAIL %s v=%d: NaN/Inf (got %.6f)\n", name, v, got);
            ++fail;
        } else if (got != want) {
            printf("FAIL %s v=%d: got %.6f want %.6f\n", name, v, got, want);
            ++fail;
        }
    };

    // Active voices (v < 4): must match full-count values.
    // COLLAPSE: Draw structs are [16][16] — V2..V16 at [1..15]. v=0 → [v+1]=V2.
    for (int v = 0; v < 4; ++v) {
        for (int i = 0; i < 16; ++i) {
            check("rhythm", rPart.rhythm[v+1][i], rFull.rhythm[v+1][i], v);
            check("accent", rPart.accent[v+1][i], rFull.accent[v+1][i], v);
            check("variation", rPart.variation[v+1][i], rFull.variation[v+1][i], v);
            check("legato", rPart.legato[v+1][i], rFull.legato[v+1][i], v);
            check("melody", mPart.melody[v+1][i], mFull.melody[v+1][i], v);
            check("octave", mPart.octave[v+1][i], mFull.octave[v+1][i], v);
            check("qmix", qPart.qmix[v+1][i], qFull.qmix[v+1][i], v);
        }
    }

    // Inactive voices (v >= 4): must be 0 (zero-init, not read by the gated blend loop)
    for (int v = 4; v < 15; ++v) {
        for (int i = 0; i < 16; ++i) {
            if (rPart.rhythm[v+1][i] != 0.f) {
                printf("FAIL rhythm v=%d i=%d: inactive voice not zero (got %.6f)\n", v, i, rPart.rhythm[v+1][i]);
                ++fail;
            }
            if (mPart.melody[v+1][i] != 0.f) {
                printf("FAIL melody v=%d i=%d: inactive voice not zero (got %.6f)\n", v, i, mPart.melody[v+1][i]);
                ++fail;
            }
            if (qPart.qmix[v+1][i] != 0.f) {
                printf("FAIL qmix v=%d i=%d: inactive voice not zero (got %.6f)\n", v, i, qPart.qmix[v+1][i]);
                ++fail;
            }
        }
    }

    printf(fail ? "%d FAILURES\n" : "voice-count partial: active voices match full, inactive are zero\n", fail);
    return fail ? 1 : 0;
}
