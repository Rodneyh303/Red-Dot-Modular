// PARKED (not registered): the 2026-11-18 abs1 content law, split out of test_global_dip_differential.
// STATUS 2026-11-22: it asserts (s+R-O) mod G, which was superseded by Rodney's 2026-11-21 ruling
// (CYCLE-CONSISTENT ABS1: the content IS the cycle read; the range base only mutes; see SequencerEngine.cpp
// at the abs1 read derivation). The sounding-set checks pass; the content checks read voiceDriver_ (the lane
// frame tick), not voiceRead_. Re-aim to the 11-21 law, then register. Do NOT "fix" the engine to this file.
// test/test_global_dip_differential.cpp — CC_FIX_GLOBAL_BELOW_VOICE_PHASE_DESYNC
// (the doc's differential, turned into the guard per Rodney's 2026-11-18 ruling:
// the WITHIN-ENGINE residue-free property, not raw A==B under live modulation).
//
// LEG 1 (STATIC — the regression floor): two engines, same seed + identical gate
// stream; B dips global 16->4->16; NO live modulation. The WANDER (advanceOffset_)
// stays A==B — the diag's own metric ("the dip perturbs nothing, advanceOffset_
// included"). The played columns may legitimately differ: the mono transport is
// itself a probabilistic decision reading the global-bounded variation lane, so
// pos_.phrasePos() is a view of a walk that legitimately differs A vs B (the
// doc's own ruling — that is the music, not a defect).
//
// LEG 2 (LIVE — the on-schedule IFF): the doc's live variation-lane offset CV
// rig, in BOTH row flavours. The offsets MAY diverge — the dipped engine
// genuinely played a different window. What must hold: at every step the
// lane-LOR commit fires IFF the engine's OWN live global-anchored re-entry
// holds this step — the committed value equals the pend when the anchor holds
// and a tickHolders pass ran, and is untouched otherwise. No commit at a step
// the live anchor does not sanction — no latched off-grid step-6/7. 2a runs
// Rodney's diag rig verbatim (identity-delegated rows: the anchor is the global
// phrase entry); 2b runs the worked example's rig (Local-East mapped L=8
// frames — the rotation anchor's own law, exercised through the dip).
//
// LEG 3 (the abs1 A-table): the ring = the CURRENT GLOBAL PHRASE (the doc's abs1
// rulings) — the sounding set is a pure function of (step mod G, O, L), rotation
// moves content and not sound, a declared L >= G sounds the whole phrase with L
// staying as authored (no cached min), and a global change is immediate
// (drift-free by construction).
#include <cstdio>
#include <cstdint>
#include <cmath>
#include <vector>
#include "engines/SequencerEngine.hpp"
#include "VoiceResolver.hpp"
#include "PhiloxRng.hpp"
using redDot::PhiloxRng;
using dotModular::VoiceResolver; using LaneId = dotModular::LaneId;

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("  FAIL: %s\n", (m)); ++g_fail; } } while (0)
// ── LEG 3: the abs1 A-table (the ring = the current global phrase) ────────────
static void leg3_abs1_table() {
    std::printf("LEG3: the abs1 A-table (the ring = the current global phrase)\n");
    char msg[240];
    const auto v2s = VoiceResolver::slot(2);
    struct Cfg { int G, O, L, R; bool abs1; const char* tag; };
    const Cfg cfgs[] = {
        {8, 2, 4, 0, true,  "A1"},
        {8, 2, 4, 2, true,  "A2"},
        {4, 0, 8, 0, true,  "A4"},
        {8, 0, 4, 0, true,  "A6-abs1"},
        {8, 0, 4, 0, false, "A6-cycle"},
    };
    for (const Cfg& c : cfgs) {
        SequencerEngine eng; eng.reset();
        eng.updateWindow((float)c.G, 0.f, false, 0.f, 0.f, false);
        eng.numPolyVoices = 2;
        eng.setVoiceLorLocalEast(v2s, true);
        eng.voiceLorRef(v2s, SequencerEngine::LOR_LEN) = c.L;
        eng.voiceLorRef(v2s, SequencerEngine::LOR_OFF) = c.O;
        eng.voiceLorRef(v2s, SequencerEngine::LOR_ROT) = c.R;
        eng.maskMacroKind_ = c.abs1 ? SequencerEngine::MASK_DYNAMIC : SequencerEngine::MASK_CYCLE;
        // The declared L stays as authored — no cached min (A4's second point).
        int dL, dO, dR; SequencerEngine::LaneDir dd;
        eng.effectiveVoiceLor(v2s.v, dL, dO, dR, dd);
        std::snprintf(msg, sizeof msg, "%s: the declared L was clamped (%d, want %d) — the extent is a consequence, never a stored min", c.tag, dL, c.L);
        CHECK(dL == c.L, msg);
        // One full phrase: the sounding set + the content law, hand-computed.
        for (int n = 1; n <= c.G; ++n) {
            eng.advancePlayhead(1, nullptr);
            const int s = eng.pos_.phrasePos();
            const bool sound = eng.outputMaskPasses(v2s, s);
            const bool wantSound = !c.abs1 || (c.L >= c.G) || (((s - c.O) % c.G + c.G) % c.G < c.L);
            std::snprintf(msg, sizeof msg, "%s: phrase step %d sounds=%d, want %d (G=%d O=%d L=%d)", c.tag, s, (int)sound, (int)wantSound, c.G, c.O, c.L);
            CHECK(sound == wantSound, msg);
            // The content (the ABS1 law — the cycle row LOOPS its window instead,
            // the doc's A6 pair): the frame value at a phrase step s is
            // (s + R - O) mod G — rotation moves CONTENT, never the sounding set.
            if (c.abs1) {
                const int wantContent = (((s + c.R - c.O) % c.G) + c.G) % c.G;
                const int content = (int)(eng.voiceDriver_[v2s.v] - 1);
                std::snprintf(msg, sizeof msg, "%s: phrase step %d content %d, want %d (the rho=(R-O) anchor: content at s=O is exactly R)", c.tag, s, content, wantContent);
                CHECK(content == wantContent, msg);
            } else {
                const int wantLoop = (((s + c.R) % c.L) + c.L) % c.L;
                const int content = (int)(eng.voiceDriver_[v2s.v] - 1);
                std::snprintf(msg, sizeof msg, "%s: phrase step %d content %d, want %d (the cycle row LOOPS its window: 0,1,2,3,0,1,2,3)", c.tag, s, content, wantLoop);
                CHECK(content == wantLoop, msg);
            }
        }
    }
    {   // A7: abs1 O0 L4, G 8->4 — before {0..3} sound / {4..7} silent; after ALL 4, immediately.
        SequencerEngine eng; eng.reset();
        eng.updateWindow(8.f, 0.f, false, 0.f, 0.f, false);
        eng.numPolyVoices = 2;
        eng.setVoiceLorLocalEast(v2s, true);
        eng.voiceLorRef(v2s, SequencerEngine::LOR_LEN) = 4;
        eng.maskMacroKind_ = SequencerEngine::MASK_DYNAMIC;
        for (int n = 1; n <= 8; ++n) {
            eng.advancePlayhead(1, nullptr);
            const int s = eng.pos_.phrasePos();
            const bool want = s < 4;
            std::snprintf(msg, sizeof msg, "A7-before: phrase step %d sounds=%d, want %d (G8 O0 L4: 0-3 sound, 4-7 silent)", s, (int)eng.outputMaskPasses(v2s, s), (int)want);
            CHECK(eng.outputMaskPasses(v2s, s) == want, msg);
        }
        eng.updateWindow(4.f, 0.f, false, 0.f, 0.f, false);
        for (int n = 1; n <= 4; ++n) {
            eng.advancePlayhead(1, nullptr);
            const int s = eng.pos_.phrasePos();
            std::snprintf(msg, sizeof msg, "A7-after: phrase step %d silent after G 8->4 (L=4 >= G=4 — the whole phrase sounds, immediately)", s);
            CHECK(eng.outputMaskPasses(v2s, s), msg);
        }
    }
}

int main() { leg3_abs1_table(); return g_fail ? 1 : 0; }
