// test_lane_bitcompare.cpp — bit-compare for lane-index unification.
// Seeds random_[] by PL_* NAME (not raw index), reads back via polyRandom.
// If the output is identical before/after the lane-index reorder, this test
// passes on BOTH branches (lane-expander-refactor and lane-index-unification).
// The raw index in random_[] changes, but both write and read use the same
// PL_* name, so the data is consistent — this test PROVES that.
#include "engines/PatternEngine.hpp"
#include "engines/SequencerEngine.hpp"
#include "VoiceResolver.hpp"   // CC_PROMPT_STRONG_TYPES: VoiceSlot minting
#include <cstdio>

int main() {
    PatternEngine pe;
    // Seed mono (voice 0) random_[] by PL_* name with distinct per-lane values
    for (int i = 0; i < 16; ++i) {
        pe.polyRandom(dotModular::VoiceResolver::slot(0 + 1), dotModular::LaneId{SequencerEngine::PL_MELODY})[i]    = 0.10f + i * 0.001f;
        pe.polyRandom(dotModular::VoiceResolver::slot(0 + 1), dotModular::LaneId{SequencerEngine::PL_OCTAVE})[i]    = 0.20f + i * 0.001f;
        pe.polyRandom(dotModular::VoiceResolver::slot(0 + 1), dotModular::LaneId{SequencerEngine::PL_QMIX})[i]      = 0.30f + i * 0.001f;
        pe.polyRandom(dotModular::VoiceResolver::slot(0 + 1), dotModular::LaneId{SequencerEngine::PL_REST})[i]      = 0.40f + i * 0.001f;
        pe.polyRandom(dotModular::VoiceResolver::slot(0 + 1), dotModular::LaneId{SequencerEngine::PL_ACCENT})[i]    = 0.50f + i * 0.001f;
        pe.polyRandom(dotModular::VoiceResolver::slot(0 + 1), dotModular::LaneId{SequencerEngine::PL_VARIATION})[i] = 0.60f + i * 0.001f;
        pe.polyRandom(dotModular::VoiceResolver::slot(0 + 1), dotModular::LaneId{SequencerEngine::PL_LEGATO})[i]    = 0.70f + i * 0.001f;
    }
    // Read back — must match the seeded values (PL_* name → same data regardless of raw index)
    struct { int lane; float base; const char* n; } cases[] = {
        {SequencerEngine::PL_MELODY,    0.10f, "MELODY"},
        {SequencerEngine::PL_OCTAVE,    0.20f, "OCTAVE"},
        {SequencerEngine::PL_QMIX,      0.30f, "QMIX"},
        {SequencerEngine::PL_REST,      0.40f, "REST"},
        {SequencerEngine::PL_ACCENT,    0.50f, "ACCENT"},
        {SequencerEngine::PL_VARIATION, 0.60f, "VARIATION"},
        {SequencerEngine::PL_LEGATO,    0.70f, "LEGATO"},
    };
    int fail = 0;
    for (auto& c : cases) {
        for (int s = 0; s < 16; ++s) {
            float got = pe.polyRandom(0, c.lane)[s];
            float want = c.base + s * 0.001f;
            if (got != want) {
                printf("FAIL %s step %d: got %.4f want %.4f\n", c.n, s, got, want);
                ++fail;
            }
        }
    }
    // Also verify the strand mapping (polyLaneStrand) is correct per NAME
    // (PL_* name → STRAND_* name must be consistent regardless of index values)
    struct { int pl; int strand; const char* n; } strandCases[] = {
        {SequencerEngine::PL_MELODY,    dotModular::STRAND_MELODY,    "MEL->MEL"},
        {SequencerEngine::PL_OCTAVE,    dotModular::STRAND_OCTAVE,    "OCT->OCT"},
        {SequencerEngine::PL_QMIX,      dotModular::STRAND_QMIX,      "QMIX->QMIX"},
        {SequencerEngine::PL_REST,      dotModular::STRAND_RHYTHM,    "REST->RHY"},
        {SequencerEngine::PL_ACCENT,    dotModular::STRAND_ACCENT,    "ACC->ACC"},
        {SequencerEngine::PL_VARIATION, dotModular::STRAND_VARIATION, "VAR->VAR"},
        {SequencerEngine::PL_LEGATO,    dotModular::STRAND_LEGATO,    "LEG->LEG"},
    };
    for (auto& c : strandCases) {
        int got = SequencerEngine::polyLaneStrand(c.pl);
        if (got != c.strand) {
            printf("FAIL strand %s: got %d want %d\n", c.n, got, c.strand);
            ++fail;
        }
    }
    printf(fail ? "%d FAILURES\n" : "bit-compare: all PL_* lanes read/write consistently\n", fail);
    return fail ? 1 : 0;
}
