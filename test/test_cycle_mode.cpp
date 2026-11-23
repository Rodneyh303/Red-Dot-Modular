// test_cycle_mode.cpp — the UAS Cycle: Phrase / Cycle: Custom law (CycleModeCore.hpp),
// CC_PROMPT_UAS_CYCLE_PHRASE_MODE.md. INDEPENDENT oracles: every expectation is recomputed
// from Rodney's stated rulings, never by calling the function under test.
//
// The headline property is the bug being fixed: in Phrase mode a 16-step pattern REPEATS
// EVERY LAP (it used to sit over a 32-step voice window whose 2nd lap drew different cells).
#include <cstdio>
#include <vector>
#include <algorithm>
#include "dsp/CycleModeCore.hpp"

using namespace redDot::cycle;

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("  FAIL: %s\n", (m)); ++g_fail; } } while (0)

// Independent oracle: the read law, written straight from the rationale sentence
// "the voice at its first step reads the cell at the global offset; it wraps at the global length".
static int oracleRead(int step, int L, int R) {
    int len = L; if (len < 1) len = 1; if (len > 32) len = 32;
    int r = R % len; if (r < 0) r += len;
    int s = step % len; if (s < 0) s += len;
    return (s + r) % len;
}

int main() {
    std::printf("test_cycle_mode\n");

    // 1. THE HEADLINE FIX: Phrase at transportLen = 16 gives L = 16 (NOT the 32 cap), so the
    //    read sequence REPEATS every lap. (Bug 23 seeded L = cap, breaking the repeat.)
    {
        Cycle authored{32, 7, 5};                       // junk authored values — Phrase must ignore them
        Cycle eff = resolveCycle(CycleMode::Phrase, 16, 0, authored);
        CHECK(eff.L == 16, "Phrase L follows the transport length (16), not the cap");
        CHECK(eff.O == 0,  "Phrase O is locked at 0");
        CHECK(eff.R == 0,  "Phrase R = transport offset (0 here)");
        for (int s = 0; s < 16; ++s)
            CHECK(readIndex(s, eff.L, eff.R) == readIndex(s + 16, eff.L, eff.R),
                  "16-step pattern repeats every lap");
        // and the two laps together visit only 16 distinct cells, not 32
        std::vector<int> cells;
        for (int s = 0; s < 32; ++s) cells.push_back(readIndex(s, eff.L, eff.R));
        std::sort(cells.begin(), cells.end());
        cells.erase(std::unique(cells.begin(), cells.end()), cells.end());
        CHECK((int)cells.size() == 16, "two laps touch 16 distinct cells (not 32)");
    }

    // 2. Phrase IGNORES authored entirely, and tracks a LIVE transport length change.
    {
        Cycle junk{3, 9, 11};
        for (int tl : {1, 7, 12, 16, 31, 32}) {
            Cycle eff = resolveCycle(CycleMode::Phrase, tl, 0, junk);
            CHECK(eff.L == tl && eff.O == 0, "Phrase tracks transport length live, O locked");
        }
        // out-of-range transport lengths clamp, never crash or go negative
        CHECK(resolveCycle(CycleMode::Phrase, 0,   0, junk).L == 1,  "transport len 0 clamps to 1");
        CHECK(resolveCycle(CycleMode::Phrase, 99,  0, junk).L == 32, "transport len over cap clamps");
        CHECK(resolveCycle(CycleMode::Phrase, -5,  0, junk).L == 1,  "negative transport len clamps");
    }

    // 3. Phrase R = the transport OFFSET 1:1, normalised into [0,L) — and the rationale's
    //    defining property: AT THE FIRST STEP the voice reads the cell at the global offset.
    {
        Cycle junk{32, 0, 0};
        for (int L : {4, 7, 16, 32}) {
            for (int off = -3; off <= L + 3; ++off) {
                Cycle eff = resolveCycle(CycleMode::Phrase, L, off, junk);
                int wantR = off % L; if (wantR < 0) wantR += L;
                CHECK(eff.R == wantR, "Phrase R == transport offset mod L (negative-safe)");
                CHECK(readIndex(0, eff.L, eff.R) == wantR,
                      "first step reads the cell AT the global offset");
            }
        }
    }

    // 4. The offset ROTATES WITHIN the phrase (same probabilities, shifted start) — it does
    //    NOT slide over a bigger ring. Oracle: a full lap is a PERMUTATION of 0..L-1 for ANY R.
    {
        for (int L : {5, 16, 32}) {
            for (int R = 0; R < L; ++R) {
                std::vector<int> seen;
                for (int s = 0; s < L; ++s) {
                    int c = readIndex(s, L, R);
                    CHECK(c == oracleRead(s, L, R), "readIndex matches the rationale oracle");
                    CHECK(c >= 0 && c < L, "cell inside the phrase");
                    seen.push_back(c);
                }
                std::sort(seen.begin(), seen.end());
                for (int i = 0; i < L; ++i)
                    CHECK(seen[i] == i, "a lap visits every cell exactly once (rotation, not slide)");
            }
        }
    }

    // 5. Custom is unchanged: it uses its OWN authored L/O/R (and ignores the transport).
    {
        Cycle authored{12, 5, 3};
        Cycle eff = resolveCycle(CycleMode::Custom, 16, 9, authored);
        CHECK(eff.L == 12 && eff.O == 5 && eff.R == 3, "Custom uses authored L/O/R");
        // Custom with L == transport and R == offset still is NOT the identity (mode is the thing)
        Cycle match{16, 0, 0};
        Cycle me = resolveCycle(CycleMode::Custom, 16, 0, match);
        CHECK(isIdentityCycle(CycleMode::Custom, me, true) == false,
              "a Custom row that happens to match the phrase is NOT the pass-through identity");
    }

    // 6. IDENTITY: Phrase && R_eff == 0 && forward. (Replaces the old L==cap sentinel.)
    {
        Cycle junk{1, 0, 0};
        Cycle p0 = resolveCycle(CycleMode::Phrase, 16, 0, junk);
        Cycle p3 = resolveCycle(CycleMode::Phrase, 16, 3, junk);
        CHECK(isIdentityCycle(CycleMode::Phrase, p0, true)  == true,  "Phrase + R=0 + forward IS identity");
        CHECK(isIdentityCycle(CycleMode::Phrase, p3, true)  == false, "Phrase + rotation is NOT identity");
        CHECK(isIdentityCycle(CycleMode::Phrase, p0, false) == false, "reverse is NOT identity");
    }

    // 7. MODE SWITCH: Phrase -> Custom must not jump (adopt the effective window);
    //    Custom -> Phrase discards (and the round trip does NOT restore the original Custom).
    {
        Cycle authored{7, 2, 1};
        Cycle eff = resolveCycle(CycleMode::Phrase, 16, 5, authored);     // {16,0,5}
        Cycle adopted = authoredOnModeSwitch(CycleMode::Phrase, CycleMode::Custom, eff, authored);
        CHECK(adopted.L == eff.L && adopted.O == eff.O && adopted.R == eff.R,
              "Phrase->Custom adopts the effective window (no jump)");
        // the adopted window resolves to the SAME effective under Custom => continuity
        Cycle after = resolveCycle(CycleMode::Custom, 16, 5, adopted);
        CHECK(after.L == eff.L && after.O == eff.O && after.R == eff.R,
              "no audible jump: Custom resolves to what Phrase was producing");
        // Custom -> Phrase passes authored through (unused by Phrase)
        Cycle kept = authoredOnModeSwitch(CycleMode::Custom, CycleMode::Phrase, after, authored);
        CHECK(kept.L == authored.L && kept.O == authored.O && kept.R == authored.R,
              "Custom->Phrase leaves authored untouched (Phrase ignores it)");
        // documented consequence: a Custom->Phrase->Custom round trip does NOT restore {7,2,1}
        Cycle backEff = resolveCycle(CycleMode::Phrase, 16, 5, kept);
        Cycle back = authoredOnModeSwitch(CycleMode::Phrase, CycleMode::Custom, backEff, kept);
        CHECK(!(back.L == 7 && back.O == 2 && back.R == 1),
              "round trip discards the original Custom window (the documented 'discards')");
    }

    if (g_fail == 0) std::printf("  OK\n");
    else             std::printf("  %d FAILURE(S)\n", g_fail);
    return g_fail ? 1 : 0;
}
