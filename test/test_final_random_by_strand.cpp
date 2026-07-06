// test_final_random_by_strand.cpp ─ Step 4 of the probability-modifier unification.
// Guards the finalRandomByStrand strand→array addressing after collapsing its 6-case switch to a
// pointer-to-member table. Seeds each named *Random array with a distinct per-strand value and
// confirms every strand (incl. VAR/LEG) maps to its OWN array, plus the out-of-range→rhythm fallback.
// Build: g++ -std=c++17 -I. -I../src/dsp -I../src/dsp/engines -I../src/dsp/gates \
//   test_final_random_by_strand.cpp ../src/dsp/engines/PatternEngine.cpp \
//   ../src/dsp/engines/SequencerEngine.cpp ../src/dsp/gates/GateState.cpp \
//   ../src/dsp/engines/ClockEngine.cpp -o /tmp/tfr && /tmp/tfr
#include "engines/PatternEngine.hpp"
#include <cstdio>
int main() {
    PatternEngine pe;
    // seed each named array with a distinct per-strand value so mis-mapping shows
    for (int i=0;i<16;i++){
        pe.melodyRandom[i]=0.10f+i*0.001f;
        pe.octaveRandom[i]=0.20f+i*0.001f;
        pe.rhythmRandom[i]=0.30f+i*0.001f;
        pe.accentRandom[i]=0.40f+i*0.001f;
        pe.variationRandom[i]=0.50f+i*0.001f;
        pe.legatoRandom[i]=0.60f+i*0.001f;
    }
    struct { int strand; float base; const char* n; } cases[] = {
        {dotModular::STRAND_MELODY,0.10f,"MELODY"},{dotModular::STRAND_OCTAVE,0.20f,"OCTAVE"},
        {dotModular::STRAND_RHYTHM,0.30f,"RHYTHM"},{dotModular::STRAND_ACCENT,0.40f,"ACCENT"},
        {dotModular::STRAND_VARIATION,0.50f,"VARIATION"},{dotModular::STRAND_LEGATO,0.60f,"LEGATO"},
    };
    int fail=0;
    for (auto&c:cases) for(int s=0;s<16;s++){
        float got=pe.finalRandomByStrand(c.strand,s), want=c.base+s*0.001f;
        if (got!=want){ printf("FAIL %s step %d: got %.4f want %.4f\n",c.n,s,got,want); fail++; }
    }
    // out-of-range → rhythm fallback
    if (pe.finalRandomByStrand(99,5)!=pe.rhythmRandom[5]){ printf("FAIL oob fallback\n"); fail++; }
    printf(fail? "%d FAILURES\n":"finalRandomByStrand: all mappings correct\n", fail);
    return fail?1:0;
}
