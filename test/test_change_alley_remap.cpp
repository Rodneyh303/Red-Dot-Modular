/**
 * test_change_alley_remap.cpp — the §3-REVISED write-side remap invariants.
 * Compile: g++ -std=c++17 -Itest test/test_change_alley_remap.cpp -o /tmp/test_car && /tmp/test_car
 *
 * Replicates PatternEngine::applyChangeAlleyRemap standalone (the real one depends on the
 * full engine; this models the exact algorithm on a [16][NUM_STRANDS][16] buffer) and
 * locks the properties the design guarantees:
 *   - Identity tables => bit-exact no-op (the regression case: no expander must not change
 *     a single value, on ANY strand — this is where the mono VAR/LEG bug lived).
 *   - A moved rhythm pin copies the SOURCE row's rhythm-family strands only; melody
 *     strands untouched (and vice-versa) — the pool split.
 *   - Locality: rows without a moved pin are bit-identical to pre-remap.
 *   - Subset/superset: two rows pinned to one source get IDENTICAL draws, so any two
 *     thresholds produce nested rest sets.
 */
#include <cstdio>
#include <cstdint>
#include <cstring>

static int pass = 0, fail = 0;
#define CHK(c, m) do { if (c) ++pass; else { ++fail; std::printf("  FAIL: %s\n", m); } } while (0)

// Mirror the engine's strand enum (LaneMapping.hpp order)
enum { STRAND_MELODY=0, STRAND_OCTAVE=1, STRAND_RHYTHM=2, STRAND_ACCENT=3, STRAND_VARIATION=4, STRAND_LEGATO=5, NUM_STRANDS=6 };

struct Rig {
    float random_[16][NUM_STRANDS][16];
    uint8_t caRhythmSrc[16], caMelodySrc[16];

    Rig() {
        for (int b = 0; b < 16; ++b)
            for (int s = 0; s < NUM_STRANDS; ++s)
                for (int i = 0; i < 16; ++i)
                    random_[b][s][i] = (b * 100 + s * 10 + i) * 0.001f;  // unique per cell
        for (int v = 0; v < 16; ++v) { caRhythmSrc[v] = v; caMelodySrc[v] = v; }
    }
    int caSrcRow(int row, int strand) const {
        const bool mel = (strand == STRAND_MELODY || strand == STRAND_OCTAVE);
        const int r = (row >= 0 && row < 16) ? row : 0;
        return mel ? (int)caMelodySrc[r] : (int)caRhythmSrc[r];
    }
    // EXACT replica of PatternEngine::applyChangeAlleyRemap
    void applyChangeAlleyRemap() {
        bool identity = true;
        for (int v = 0; v < 16 && identity; ++v)
            if (caRhythmSrc[v] != v || caMelodySrc[v] != v) identity = false;
        if (identity) return;
        static float snap[16][NUM_STRANDS][16];
        std::memcpy(snap, random_, sizeof(random_));
        for (int row = 0; row < 16; ++row)
            for (int s = 0; s < NUM_STRANDS; ++s) {
                const int src = caSrcRow(row, s);
                if (src == row) continue;
                for (int i = 0; i < 16; ++i) random_[row][s][i] = snap[src][s][i];
            }
    }
};

int main() {
    // 1. Identity = bit-exact no-op on every cell (the regression guard)
    {
        Rig r; float before[16][NUM_STRANDS][16]; std::memcpy(before, r.random_, sizeof(before));
        r.applyChangeAlleyRemap();
        bool same = (std::memcmp(before, r.random_, sizeof(before)) == 0);
        CHK(same, "identity tables => bit-exact no-op (all strands)");
    }

    // 2. Rhythm pin move copies rhythm-family strands only; melody strands untouched
    {
        Rig r;
        r.caRhythmSrc[3] = 7;   // voice 3 borrows voice 7's rhythm pool
        float m3_before = r.random_[3][STRAND_MELODY][5];   // voice 3's own melody, should stay
        float rh7 = r.random_[7][STRAND_RHYTHM][5];
        float ac7 = r.random_[7][STRAND_ACCENT][5];
        float va7 = r.random_[7][STRAND_VARIATION][5];
        float le7 = r.random_[7][STRAND_LEGATO][5];
        r.applyChangeAlleyRemap();
        CHK(r.random_[3][STRAND_RHYTHM][5]    == rh7, "rhythm pin: RHYTHM strand borrowed from source");
        CHK(r.random_[3][STRAND_ACCENT][5]    == ac7, "rhythm pin: ACCENT borrowed (rhythm pool)");
        CHK(r.random_[3][STRAND_VARIATION][5] == va7, "rhythm pin: VARIATION borrowed (rhythm pool)");
        CHK(r.random_[3][STRAND_LEGATO][5]    == le7, "rhythm pin: LEGATO borrowed (rhythm pool)");
        CHK(r.random_[3][STRAND_MELODY][5]    == m3_before, "rhythm pin: MELODY strand UNTOUCHED");
    }

    // 3. Melody pin move copies melody-family only; rhythm strands untouched
    {
        Rig r;
        r.caMelodySrc[4] = 9;
        float rh4_before = r.random_[4][STRAND_RHYTHM][2];
        float me9 = r.random_[9][STRAND_MELODY][2];
        float oc9 = r.random_[9][STRAND_OCTAVE][2];
        r.applyChangeAlleyRemap();
        CHK(r.random_[4][STRAND_MELODY][2] == me9, "melody pin: MELODY borrowed");
        CHK(r.random_[4][STRAND_OCTAVE][2] == oc9, "melody pin: OCTAVE borrowed (melody pool)");
        CHK(r.random_[4][STRAND_RHYTHM][2] == rh4_before, "melody pin: RHYTHM strand UNTOUCHED");
    }

    // 4. Locality: an unpinned row is bit-identical after a remap that moved another row
    {
        Rig r; r.caRhythmSrc[2] = 8;
        float row5_before[NUM_STRANDS][16]; std::memcpy(row5_before, r.random_[5], sizeof(row5_before));
        r.applyChangeAlleyRemap();
        CHK(std::memcmp(row5_before, r.random_[5], sizeof(row5_before)) == 0,
            "locality: unpinned row 5 bit-identical after row 2 remap");
    }

    // 5. Subset/superset precondition: two rows pinned to one source get IDENTICAL draws
    {
        Rig r; r.caRhythmSrc[1] = 6; r.caRhythmSrc[2] = 6;   // rows 1 & 2 both borrow row 6 rhythm
        r.applyChangeAlleyRemap();
        bool identical = true;
        for (int i = 0; i < 16; ++i)
            if (r.random_[1][STRAND_RHYTHM][i] != r.random_[2][STRAND_RHYTHM][i]) identical = false;
        CHK(identical, "shared source: two rows get identical rhythm draws (=> nested rest sets)");
        // and both equal the source
        bool eqSrc = true;
        for (int i = 0; i < 16; ++i)
            if (r.random_[1][STRAND_RHYTHM][i] != r.random_[6][STRAND_RHYTHM][i]) eqSrc = false;
        CHK(eqSrc, "shared source: borrowed draws equal the source row's");
    }

    // 6. Mono (row 0) participates: row 0 pinned to a poly source borrows it
    {
        Rig r; r.caRhythmSrc[0] = 5;
        float rh5 = r.random_[5][STRAND_RHYTHM][0];
        r.applyChangeAlleyRemap();
        CHK(r.random_[0][STRAND_RHYTHM][0] == rh5, "mono row 0 borrows poly source (unified addressing)");
    }

    std::printf(fail ? "\n%d passed, %d FAILED\n" : "\n%d passed, 0 failed\n", pass, fail);
    return fail ? 1 : 0;
}
