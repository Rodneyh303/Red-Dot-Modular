#include "test_stubs.hpp"
// _probe_varleg.cpp — runtime probe: are per-voice VAR/LEG draws generated?
// Compile: g++ -std=c++17 -Itest -Isrc -Isrc/dsp -Isrc/dsp/engines \
//          test/_probe_varleg.cpp src/dsp/engines/PatternEngine.cpp -o _probe_varleg
#include "PatternEngine.hpp"
#include <cstdio>

int main() {
    PatternEngine pe;
    pe.seedRhythmPhilox(7.3f);     // non-default seed so any draw is real, not a seed artefact
    pe.seedMelodyPhilox(0.f);

    PatternInput in;
    for (int i = 0; i < 12; ++i) in.semiWeights[i] = 1.f;
    in.octaveLo = 3.f; in.octaveHi = 5.f; in.restProb = 0.f;
    in.variationAmount = 0.5f; in.transpose = 0.f;
    in.noteVariationMask = 0b111; in.locked = false;

    // Force a rhythm redraw (this is where the Philox draws happen) then recompute
    // so the slewed buffers + the !sandsActive promote path both run.
    pe.redrawRhythm(in);
    pe.rhythmSlewLatched = 0.f;     // r==0 path (raw draw blend)
    pe.rhythmMixLatched  = 0.f;
    pe.recomputeEffectiveRhythm();

    auto dump = [](const char* name, int strand, int pl) {
        std::printf("== %s (strand %d, PL %d) ==\n", name, strand, pl);
        // mono (bank 0)
        std::printf("  mono  : ");
        for (int i = 0; i < 16; ++i) std::printf("%.3f ", pe.finalRandomByStrand(strand, i));
        std::printf("\n");
        int nonzero = 0;
        for (int v = 0; v < 15; ++v) {
            int nz = 0;
            for (int i = 0; i < 16; ++i) if (pe.polyRandom(v + 1, pl)[i] != 0.f) ++nz;
            nonzero += nz;
            std::printf("  v%-2d  : ", v + 1);
            for (int i = 0; i < 16; ++i) std::printf("%.3f ", pe.polyRandom(v + 1, pl)[i]);
            std::printf("  (nonzero=%d/16)\n", nz);
        }
        std::printf("  TOTAL nonzero poly cells: %d/240\n\n", nonzero);
    };

    // polyRandom maps PL_VARIATION(5)->editor lane 5 == STRAND_VARIATION; PL_LEGATO(6)->6.
    dump("VARIATION", dotModular::STRAND_VARIATION, PatternEngine::PL_VARIATION);
    dump("LEGATO",    dotModular::STRAND_LEGATO,    PatternEngine::PL_LEGATO);

    // Also show a known-good lane (RHYTHM) to prove the probe wiring + redraw actually ran:
    std::printf("== RHYTHM (control: must be nonzero) ==\n");
    int rnz = 0;
    for (int v = 0; v < 15; ++v)
        for (int i = 0; i < 16; ++i)
            if (pe.polyRandom(v + 1, PatternEngine::PL_REST)[i] != 0.f) ++rnz;
    std::printf("  TOTAL nonzero poly RHYTHM cells: %d/240\n", rnz);
    return 0;
}
