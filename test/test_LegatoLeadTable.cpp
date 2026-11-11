// test_LegatoLeadTable.cpp  pins which note divisions can LEAD a legato at 24 and 48 PPQN.
// A division leads only if its pulse count is a multiple of PULSES_PER_STEP (6), because a
// legato can only connect at a step boundary. Pulses = fraction * 4 * ppqn (noteValuePulses).
// Header-only, Rack-free. Registered in test/run_all.sh.
//
// Index order is the engine's NOTE_VALUES order (strict duration, 13 entries):
//   0 1/1  1 1/2.  2 1/2  3 1/4.  4 1/4  5 1/8.  6 1/4T  7 1/8  8 1/16.  9 1/8T  10 1/16  11 1/16T  12 1/32
#include "../src/dsp/NoteValues.hpp"
#include <cstdio>

static int fails = 0, passes = 0;
#define ok(cond, msg) do { if (cond) ++passes; else { ++fails; std::printf("FAIL %s\n", msg); } } while (0)

int main() {
    static const int pulses24[NUM_NOTE_VALUES] = { 96, 72, 48, 36, 24, 18, 16, 12,  9,  8,  6,  4,  3 };
    static const int pulses48[NUM_NOTE_VALUES] = {192, 144, 96, 72, 48, 36, 32, 24, 18, 16, 12,  8,  6 };
    static const bool lead24[NUM_NOTE_VALUES]  = {  1,  1,  1,  1,  1,  1,  0,  1,  0,  0,  1,  0,  0 };
    static const bool lead48[NUM_NOTE_VALUES]  = {  1,  1,  1,  1,  1,  1,  0,  1,  1,  0,  1,  0,  1 };

    ok(PULSES_PER_STEP == 6, "PULSES_PER_STEP is 6 at any PPQN");

    for (int i = 0; i < NUM_NOTE_VALUES; ++i) {
        ok(noteValuePulses(i, 24) == pulses24[i], "pulses at 24 PPQN");
        ok(noteValuePulses(i, 48) == pulses48[i], "pulses at 48 PPQN");
        ok(noteCanLeadLegato(i, 24) == lead24[i], "legato-lead set at 24 PPQN");
        ok(noteCanLeadLegato(i, 48) == lead48[i], "legato-lead set at 48 PPQN");
    }

    // The two changes that matter: 1/32 becomes a lead at 48, and dotted 1/16 becomes a lead at 48.
    ok(!noteCanLeadLegato(12, 24) && noteCanLeadLegato(12, 48), "1/32 leads only at 48");
    ok(!noteCanLeadLegato(8, 24)  && noteCanLeadLegato(8, 48),  "dotted 1/16 leads only at 48");
    // Triplets never lead at either setting.
    ok(!noteCanLeadLegato(6, 24) && !noteCanLeadLegato(6, 48),   "1/4T never leads");
    ok(!noteCanLeadLegato(9, 24) && !noteCanLeadLegato(9, 48),   "1/8T never leads");
    ok(!noteCanLeadLegato(11, 24) && !noteCanLeadLegato(11, 48), "1/16T never leads");

    std::printf("test_LegatoLeadTable: %d passed, %d failed\n", passes, fails);
    return fails ? 1 : 0;
}
