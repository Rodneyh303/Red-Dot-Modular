// test_playhead_slot_edge.cpp — CC_PROMPT_REVERSE_VOICE_ROTATION_EDGE (Rodney,
// Rack feel-test 2026-11-19): in REVERSE the voice-range playhead reached its
// rotation when the range hit its LEFT step edge; it must reach rotation at the
// RIGHT edge. Forward is correct (rotation at the LEFT edge). The commit timing is
// CORRECT and untouchable (CC_FIX_CYCLE_COMMIT_WINDOW_EDGE) — this is ONLY the
// playhead's position/rotation alignment within the range.
//
// THE PROPERTY (the doc's verifiable definition): the step at which the voice reads
// rotation content R must be the step at which the playhead sits at the
// direction-entry edge — the LEFT slot (box-offset 0) in forward, the RIGHT slot
// (box-offset L-1) in reverse — for EVERY R, both directions. The drawn slot is the
// ONE law PositionResolver::playheadSlotBar (the read anchor's position twin:
// the read cell minus the rotation fold minus the reverse anchor bias), fed the
// LIVE engine's read cells — the same fold the strip feeds run.
//
// REPRODUCE-FIRST (mandatory): the PRE-FIX draw fed the strip the RAW read cell
// (the drawn slot == the content cell), which reads R at slot R — the LEFT-side
// slot in reverse (at R==0 exactly the LEFT edge — the degeneracy that hid this
// next to the commit-edge bug). That old-law mirror is pinned BELOW as still
// violating the property: this test could and did distinguish (pre-wiring run:
// the old-law property checks FAILED from the first non-degenerate R; the commit
// records the run).
//
// THE MODULATED-ROTATION EDGE CASE (the doc's agreed addition): the voice range
// edge is where LOR modulation lands AND where the playhead aligns to its
// rotation. A rotation change THROUGH the commit edge must re-fold the slot to
// the POST-modulation (new) rotation AT that step — atomic, not the old rotation,
// not one step late. Swept old→new pairs, both directions, none degenerate
// (R==0 hides the coincidence).
//
// Compile: see test/run_all.sh ("test_playhead_slot_edge").
#include <cstdio>
#include <vector>
#include "engines/SequencerEngine.hpp"
#include "ui/SandsRangeGestureCore.hpp"
#include "VoiceResolver.hpp"
using dotModular::VoiceResolver; using LaneId = dotModular::LaneId;

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("  FAIL: %s\n", (m)); ++g_fail; } } while (0)

static const auto V2 = [] { return VoiceResolver::slot(2); };
static constexpr int RING = dotModular::kMaskRing;

static PatternInput mk(){ PatternInput in; for(int i=0;i<12;i++) in.semiWeights[i]=1.f; in.noteVariationMask=0b111; in.variationAmount=.6f; in.octaveLo=0; in.octaveHi=1; return in; }

static void setupVoice(SequencerEngine& eng, SequencerEngine::LaneDir dir, int O, int R) {
    eng.reset(); eng.numPolyVoices = 2;
    for (int i=0;i<16;i++){ eng.pe.melodyRandom[i]=.3f; eng.pe.octaveRandom[i]=.4f; eng.pe.qmixRandom[i]=.5f; eng.pe.rhythmRandom[i]=.6f; eng.pe.accentRandom[i]=.2f; eng.pe.variationRandom[i]=.7f; eng.pe.legatoRandom[i]=.1f; }
    eng.voiceLorRef(V2(), SequencerEngine::LOR_LEN)=8;
    eng.voiceLorRef(V2(), SequencerEngine::LOR_OFF)=O;
    eng.voiceLorRef(V2(), SequencerEngine::LOR_ROT)=R;
    eng.setVoiceLorLocalEast(V2(), true);
    eng.voiceLorDir_[V2().v]=dir;
    eng.updateWindow(16.f,0.f,false,0.f,0.f,false);
    const PatternInput in = mk(); (void)in;
}

int main(){
    std::printf("test_playhead_slot_edge: the playhead reads rotation content R AT the direction-entry edge (fwd LEFT / rev RIGHT slot), every R; rotation modulation at the edge re-folds atomically\n");
    char msg[256];
    const int L = 8;

    // ── 1. THE STATIC R-SWEEP (the property, every R, both directions, two offsets) ──
    int oldLawViolations = 0, readRSteps = 0;
    for (int which = 0; which < 2; ++which) {
        const auto dir = which ? SequencerEngine::LaneDir::Reverse : SequencerEngine::LaneDir::Forward;
        const bool rev = which != 0;
        for (const int O : { 0, 2 }) {
            const int edgeBar = rev ? (O + L - 1) : O;   // the direction-entry edge CELL
            for (int R = 0; R < L; ++R) {
                SequencerEngine eng;
                setupVoice(eng, dir, O, R);
                int Lp, Op, Rp; SequencerEngine::LaneDir dp;
                for (int s = 0; s < 16; ++s) {
                    eng.advancePlayhead(1, nullptr);
                    eng.effectiveVoiceLor(V2().v, Lp, Op, Rp, dp);
                    const int readCell = eng.voiceReadPosition(V2());
                    const int content = ((readCell - Op) % RING + RING) % RING;   // the frame value (window-relative content)
                    if (content != (unsigned)R) continue;                          // not the rotation-read step
                    ++readRSteps;
                    const int bias = SequencerEngine::anchorBias(dp);
                    // THE LAW (what the strip feeds run): the slot bar at the read-R step.
                    const int slotBar = PositionResolver::playheadSlotBar(readCell, Lp, Op, Rp, bias, RING);
                    std::snprintf(msg, sizeof msg, "%s O=%d R=%d step=%d: playhead slot bar %d, want the entry edge %d (reads rotation content R at the %s slot)",
                                  rev ? "REV" : "FWD", O, R, s, slotBar, edgeBar, rev ? "RIGHT" : "LEFT");
                    CHECK(slotBar == edgeBar, msg);
                    // The sweep stays inside the range (the playhead never leaves the window).
                    CHECK(redDot::SandsRangeGestureCore::windowContains(Op, Lp, slotBar, RING),
                          "the slot bar stays inside the voice range");
                    // THE PINNED REPRODUCTION: the pre-fix draw (the raw read cell as the
                    // slot) violates the property — forever visible, so the fix stays honest.
                    if (((readCell - Op) % RING + RING) % RING != (rev ? (L - 1) : 0)) ++oldLawViolations;
                }
            }
        }
    }
    CHECK(readRSteps >= 32, "the sweep actually visited read-R steps (both dirs x R x offsets)");
    std::snprintf(msg, sizeof msg, "the pre-fix draw law (slot == the raw read cell) violates the property %d times — the reproduction this fix answers (must be > 0)", oldLawViolations);
    CHECK(oldLawViolations > 0, msg);

    // ── 2. THE MODULATED-ROTATION EDGE CASE (old→new pairs through the commit edge) ──
    const std::pair<int,int> pairs[] = { {2,5}, {5,2}, {3,6}, {6,1} };
    for (int which = 0; which < 2; ++which) {
        const auto dir = which ? SequencerEngine::LaneDir::Reverse : SequencerEngine::LaneDir::Forward;
        const bool rev = which != 0;
        for (const auto& pr : pairs) {
            const int Rold = pr.first, Rnew = pr.second;
            SequencerEngine eng;
            setupVoice(eng, dir, 0, Rold);
            int effL, effO, effR; SequencerEngine::LaneDir effD;
            eng.effectiveVoiceLor(V2().v, effL, effO, effR, effD);
            int prevR = effR, prevSlot = -1, prevReadCell = -1, committed = 0;
            // The VOICE row's LOR CV commits at the PHRASE edge (the C1 cadence — the
            // LANES commit at the voice-frame edge, the voice's own row at the phrase);
            // global 16 ⇒ the phrase edge recurs at s=16/32 — a 40-tick window covers it.
            for (int s = 0; s < 40; ++s) {
                eng.advancePlayhead(1, nullptr);
                if (s == 1) {   // drive a rotation change THROUGH the commit edge
                    std::vector<int> m3 = {0,0,0}; std::vector<int> east(16*3, 0);
                    east[V2().v * 3 + SequencerEngine::LOR_ROT] = Rnew - Rold;
                    eng.pushVoiceLorCv(m3.data(), east.data());
                }
                eng.effectiveVoiceLor(V2().v, effL, effO, effR, effD);
                const int readCell = eng.voiceReadPosition(V2());
                const int bias = SequencerEngine::anchorBias(effD);
                const int slot = PositionResolver::playheadSlotBar(readCell, effL, effO, effR, bias, RING);
                if (effR != prevR) {   // THE COMMIT STEP — the rotation lands here
                    ++committed;
                    // The slot re-folds ATOMICALLY to the NEW rotation at this step.
                    const int oldFold = PositionResolver::playheadSlotBar(readCell, effL, effO, Rold, bias, RING);
                    std::snprintf(msg, sizeof msg, "%s R %d->%d step=%d: the slot %d follows the NEW rotation (the old-rotation fold would draw %d)",
                                  rev ? "REV" : "FWD", Rold, Rnew, s, slot, oldFold);
                    CHECK(slot == PositionResolver::playheadSlotBar(readCell, effL, effO, effR, bias, RING), msg);
                    CHECK(slot != oldFold, msg);   // the jump happened (Rold != Rnew ⇒ the folds differ)
                    if (prevSlot >= 0) {
                        // NOT early: the PREVIOUS step still folded the OLD rotation.
                        const int prevOldFold = PositionResolver::playheadSlotBar(prevReadCell, effL, effO, Rold, bias, RING);
                        CHECK(prevSlot == prevOldFold, "the pre-commit step folded the OLD rotation (no early jump)");
                    }
                }
                prevReadCell = readCell;
                prevSlot = slot;
                prevR = effR;
            }
            std::snprintf(msg, sizeof msg, "%s R %d->%d: the rotation CV committed exactly once at the edge", rev ? "REV" : "FWD", Rold, Rnew);
            CHECK(committed == 1, msg);
        }
    }

    if (g_fail == 0) std::printf("all green\n");
    else std::printf("%d FAILED\n", g_fail);
    return g_fail ? 1 : 0;
}
