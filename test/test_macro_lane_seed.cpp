// test_macro_lane_seed.cpp — CC_FIX_MACRO_LANE_SEED_AND_CONSOLIDATION: the lane SEED (t0)
// law. THE LOCATED CAUSE: Macro's inline seed had NO identity/mapped branch — the
// non-pass arm was the raw GLOBAL walk (wG+offG) — so the Macro lanes folded over the
// global pattern instead of hanging under the Macro ROW's window walk the way East's
// mapped-voice lanes hang under frameT0[v]. THE FIX: ONE laneSeed helper for all three
// loops (East mono / East poly / Macro — the identity/mapped branch INSIDE, identical),
// the Macro mapped arm seeding the MACRO ROW's frame value (the bounded walk under
// macroVoiceLor_, off the macroFramePhase_ congruence); macroFrameRead_ becomes the
// row's walk read (ring-folded — the %16 fold was bug 17's twin at this tier).
//
// GATES (the doc):
//  1. MACRO LANE WINDOW — a len=7 Macro lane: under the DEFAULT global it cycles its
//     OWN 7 (the polymeter free-run — the UA_NONDIV law's visible edge, UNCHANGED);
//     under an AUTHORED row (CYCLE and ABS1) it hangs under the row's walk; under a
//     MAPPED global + identity row it follows the shared IDENTITY law (the bounded
//     global walk — East's identity arm, bit-identical).
//  2. EAST UNCHANGED — the helper reproduces East's inline law: the East mono lane
//     with the same window equals the Macro lane under the identity arm (both
//     globals), and every follower voice's lane equals it under the mapped arm (gate
//     3) — the ONE law at all three sites. (The battery's UA_NONDIV golden and the
//     position-holder suite pin the deep byte-identity of the whole read chain.)
//  3. SAME WINDOW -> SAME TICK — the drift-proof: with the same lane window and
//     direction, V1's mono lane, every follower V2..V16's lane, and the Macro lane
//     produce the SAME tick for every tick (the three seed sites cannot drift again).
//  4. V1..V16 IDENTICAL — the Macro lane is voice-independent (one global row).
//  5. THE STRIP READ — macroFrameRead_ runs INSIDE the authored window (the Macro
//     strip's playhead reads the shared row's walk), with R phase-shifting the walk
//     (rho = R, window-relative) and the lane inheriting the shifted frame value.

#include <cstdio>
#include "engines/SequencerEngine.hpp"
#include "VoiceResolver.hpp"

using dotModular::VoiceResolver;

static int g_pass = 0, g_fail = 0;
static void check(bool ok, const char* what) {
    if (ok) { ++g_pass; }
    else    { ++g_fail; std::printf("  FAIL: %s\n", what); }
}

int main() {
    const int MEL = dotModular::STRAND_MELODY;   // strand 0 == editor lane 0 == macroLOR_[0]

    // ── GATE 1a: the DEFAULT global + identity row — the lane's OWN 7 (the free-run) ──
    {
        SequencerEngine eng; eng.reset();
        eng.macroLOR_[MEL] = 7;
        bool ok = true;
        for (int t = 0; t < 21; ++t) {
            eng.advancePlayhead(1, nullptr);
            const int T = t + 1;
            ok = ok && eng.macroLaneTick_[MEL] == (T - 1) % 7;
        }
        check(ok, "1a: default global + identity row — the len-7 Macro lane cycles its OWN 7 "
                  "((T-1)%7, the raw-tick polymeter free-run — the UA_NONDIV law, UNCHANGED)");
    }

    // ── GATE 1b: a MAPPED global + identity row — the shared IDENTITY law ──
    {
        SequencerEngine eng; eng.reset();
        eng.updateWindow(10.f, 0.f, false, 2.f, 0.f, false);
        eng.macroLOR_[MEL] = 7;
        bool ok = true;
        for (int t = 0; t < 21; ++t) {
            eng.advancePlayhead(1, nullptr);
            const int T = t + 1;
            ok = ok && eng.macroLaneTick_[MEL] == (((T - 1) % 10) + 2) % 7;
        }
        check(ok, "1b: mapped global 10@2 + identity row — the IDENTITY arm seeds the bounded "
                  "GLOBAL walk, folded by the lane's own length (((T-1)%10 + 2) mod 7 — the "
                  "shared law, East's identity arm bit-identical)");
    }

    // ── GATE 1c: an AUTHORED row (CYCLE) — the lane hangs under the ROW's walk ──
    {
        SequencerEngine eng; eng.reset();
        eng.macroVoiceLor_[SequencerEngine::LOR_LEN] = 8;
        eng.macroVoiceLor_[SequencerEngine::LOR_OFF] = 16;
        eng.macroLOR_[MEL] = 7;
        bool ok = true;
        for (int t = 0; t < 21; ++t) {
            eng.advancePlayhead(1, nullptr);
            const int T = t + 1;
            ok = ok && eng.macroLaneTick_[MEL] == ((T - 1) % 8) % 7;
        }
        check(ok, "1c: authored row 8@16 (CYCLE) — the lane hangs under the ROW's walk "
                  "(((T-1)%8)%7: period 8, restarting at the row's wrap) — NOT the global's "
                  "period, NOT the free-run (pre-fix: the free-run, the row ignored)");
    }

    // ── GATE 1d: an AUTHORED row (ABS1) — the row walks the FULL PHRASE ──
    {
        SequencerEngine eng; eng.reset();
        eng.macroVoiceLor_[SequencerEngine::LOR_LEN] = 8;
        eng.macroVoiceLor_[SequencerEngine::LOR_OFF] = 16;
        eng.maskMacroKind_ = SequencerEngine::MASK_DYNAMIC;   // the row's ABS1
        eng.macroLOR_[MEL] = 7;
        bool ok = true;
        for (int t = 0; t < 21; ++t) {
            eng.advancePlayhead(1, nullptr);
            const int T = t + 1;
            ok = ok && eng.macroLaneTick_[MEL] == ((T - 1 + 16) % 32) % 7;
        }
        check(ok, "1d: authored row 8@16 (ABS1) — the row walks the FULL PHRASE (the ring, "
                  "rho=(R-O): ((T-1+16)%32)%7), the lane still cycling its own 7 values "
                  "(the voice rows' abs1 twin, one tier up)");
    }

    // ── GATE 2: EAST UNCHANGED (the identity arm) — the helper reproduces the inline law ──
    {
        bool okDefault = true, okMapped = true;
        {
            SequencerEngine eng; eng.reset();
            eng.lorRef(MEL, SequencerEngine::LOR_LEN) = 7;   // V1's mono MEL lane — the same window
            eng.macroLOR_[MEL] = 7;
            for (int t = 0; t < 21; ++t) {
                eng.advancePlayhead(1, nullptr);
                okDefault = okDefault && eng.laneTick_[MEL] == eng.macroLaneTick_[MEL];
            }
        }
        {
            SequencerEngine eng; eng.reset();
            eng.updateWindow(10.f, 0.f, false, 2.f, 0.f, false);
            eng.lorRef(MEL, SequencerEngine::LOR_LEN) = 7;
            eng.macroLOR_[MEL] = 7;
            for (int t = 0; t < 21; ++t) {
                eng.advancePlayhead(1, nullptr);
                okMapped = okMapped && eng.laneTick_[MEL] == eng.macroLaneTick_[MEL];
            }
        }
        check(okDefault && okMapped,
              "2: EAST UNCHANGED (the identity arm) — V1's mono lane with the same window "
              "equals the Macro lane under BOTH globals (the one seed law; the helper "
              "reproduces East's inline expressions verbatim)");
    }

    // ── GATE 3: SAME WINDOW -> SAME TICK — the drift-proof, all three seed sites ──
    {
        SequencerEngine eng; eng.reset();
        eng.numPolyVoices = 15;                                // every voice live
        eng.macroVoiceLor_[SequencerEngine::LOR_LEN] = 8;
        eng.macroVoiceLor_[SequencerEngine::LOR_OFF] = 16;
        eng.lorRef(MEL, SequencerEngine::LOR_LEN) = 7;         // V1's mono lane — the same window
        for (int v = 1; v <= 15; ++v)                          // V2..V16's lanes — the same window
            eng.polyLenERef(VoiceResolver::slot(v + 1),
                            dotModular::LaneId{SequencerEngine::PL_MELODY}) = 7;
        eng.macroLOR_[MEL] = 7;                                // the Macro lane — the same window
        bool ok = true;
        for (int t = 0; t < 21; ++t) {
            eng.advancePlayhead(1, nullptr);
            const int T = t + 1;
            const int want = ((T - 1) % 8) % 7;                // the row's walk, mod the lane's own 7
            ok = ok && eng.laneTick_[MEL] == want;             // East MONO  (V1, slot 0)
            for (int v = 1; v <= 15; ++v)
                ok = ok && eng.laneTickV_[v][MEL] == want;     // East POLY  (V2..V16, followers)
            ok = ok && eng.macroLaneTick_[MEL] == want;        // MACRO      (the global row)
        }
        check(ok,
              "3: SAME WINDOW -> SAME TICK — with the same lane window (7) and direction under "
              "the authored row, V1's mono lane, EVERY follower V2..V16's lane, and the Macro "
              "lane produce the SAME tick for every tick (the three seed sites, one law — the "
              "drift-proof; pre-fix the Macro site free-ran, ignoring the row)");
    }

    // ── GATE 4: V1..V16 IDENTICAL — the Macro lane is voice-independent ──
    {
        int seq0[21], seq15[21];
        {
            SequencerEngine eng; eng.reset();
            eng.macroVoiceLor_[SequencerEngine::LOR_LEN] = 8;
            eng.macroVoiceLor_[SequencerEngine::LOR_OFF] = 16;
            eng.macroLOR_[MEL] = 7;
            for (int t = 0; t < 21; ++t) { eng.advancePlayhead(1, nullptr); seq0[t] = eng.macroLaneTick_[MEL]; }
        }
        {
            SequencerEngine eng; eng.reset();
            eng.numPolyVoices = 15;                            // every voice live
            eng.macroVoiceLor_[SequencerEngine::LOR_LEN] = 8;
            eng.macroVoiceLor_[SequencerEngine::LOR_OFF] = 16;
            eng.macroLOR_[MEL] = 7;
            for (int t = 0; t < 21; ++t) { eng.advancePlayhead(1, nullptr); seq15[t] = eng.macroLaneTick_[MEL]; }
        }
        bool ok = true;
        for (int t = 0; t < 21; ++t) ok = ok && seq0[t] == seq15[t];
        check(ok,
              "4: V1..V16 IDENTICAL — the Macro lane is voice-independent (one global row: the "
              "sequence is the same with 0 and 15 poly voices live)");
    }

    // ── GATE 5: THE STRIP READ — the playhead inside the authored window ──
    {
        SequencerEngine eng; eng.reset();
        eng.macroVoiceLor_[SequencerEngine::LOR_LEN] = 8;
        eng.macroVoiceLor_[SequencerEngine::LOR_OFF] = 16;
        bool ok = true;
        for (int t = 0; t < 21; ++t) {
            eng.advancePlayhead(1, nullptr);
            const int T = t + 1;
            ok = ok && eng.macroFrameRead() == ((T - 1) % 8) + 16   // INSIDE [16,24)
                && eng.macroFrameSign() == +1;
        }
        check(ok,
              "5: THE STRIP READ — macroFrameRead_ walks the authored window [16,24) (the row's "
              "own walk, period 8 — the Macro strip's playhead reads the shared window; pre-fix "
              "it read the GLOBAL walk folded %16), sign +1 under the forward march");
    }
    {
        // 5b: R phase-shifts the row walk (rho = R, window-relative); the read stays
        // inside the window and the lane inherits the shifted frame value.
        SequencerEngine eng; eng.reset();
        eng.macroVoiceLor_[SequencerEngine::LOR_LEN] = 8;
        eng.macroVoiceLor_[SequencerEngine::LOR_OFF] = 16;
        eng.macroVoiceLor_[SequencerEngine::LOR_ROT] = 3;
        eng.macroLOR_[MEL] = 7;
        bool ok = true;
        for (int t = 0; t < 21; ++t) {
            eng.advancePlayhead(1, nullptr);
            const int T = t + 1;
            ok = ok && eng.macroFrameRead() == ((T - 1 + 3) % 8) + 16
                && eng.macroLaneTick_[MEL] == ((T - 1 + 3) % 8) % 7;
        }
        check(ok,
              "5b: R=3 phase-shifts the row walk by rho=R — the read still inside [16,24) and "
              "the lane seeding the shifted frame value (the authoring clamp keeps R < L)");
    }

    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
