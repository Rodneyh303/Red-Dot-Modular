// test_lane_preview_unify.cpp — CC_PROMPT_LANE_RANGE_DISPLAY_UNIFY Stage B
// (the FINAL MODEL, Rodney 2026-11-17)
//
// The PENDING-PREVIEW accessors are the wireframe's authority: effective*Preview
// = base + PENDING (the live-modulated window exactly — the manager pushes
// pending = round(knob+CV) − base every control block), while the effective
// reads carry base + COMMITTED (what plays, latched at the C1b commit gates).
// The display draws committed as THE SOLID window and pending as the wireframe;
// no audio path consumes the preview (read-only — the battery stays
// byte-identical).
//
// Reproduce-first: G2/G3/G5 name behaviour that had no accessor before this
// stage (the suite does not compile against the pre-Stage-B engine — the
// compile failure IS the reproduce); G1/G4/G6 pin the twins against the
// effective family they mirror.
//
// G1 IDENTITY — no CV: preview == effective == base, every tier.
// G2 DIVERGENCE — pending pushed, not committed: preview == base+pending while
//    effective == base (the A/B pair the wireframe exists to draw).
// G3 COMMIT FOLLOWS — at the commit gate committed == pending → preview ==
//    effective again.
// G4 CLAMP — the composed row runs lorRowClamp_ (L >= 1; O in [0, ring);
//    R in [0, L−1]) — preview and effective clamp identically.
// G5 SUSPEND — under setLorCvSuspend the suspended item's preview keeps
//    tracking base+pending while the effective HOLDS (the drag-time divergence
//    the wireframe shows; per-item, the reducer's zone map feeds this).
// G6 READ-ONLY — previewing writes nothing: committed stays put without a gate.
#include <cstdio>
#include <vector>
#include "engines/SequencerEngine.hpp"
#include "VoiceResolver.hpp"

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("  FAIL: %s\n", (m)); ++g_fail; } } while (0)

// The engine voice slot for V2 (the first poly voice) — the C1b rig's subject.
static dotModular::VoiceSlot v2() { return dotModular::VoiceResolver::slot(2); }

// The shared rig: a mapped voice row (L=4) + a mapped REST lane (L=5, O=3), the
// same authoring the C1b suite uses.
struct Rig {
    SequencerEngine eng;
    Rig() {
        eng.reset();
        eng.updateWindow(8.f, 0.f, false, 0.f, 0.f, false);
        eng.numPolyVoices = 2;
        eng.setVoiceLorLocalEast(v2(), true);
        eng.voiceLorRef(v2(), SequencerEngine::LOR_LEN) = 4;
        eng.polyLORRef(v2(), dotModular::LaneId{SequencerEngine::PL_REST},
                       SequencerEngine::LOR_LEN) = 5;
        eng.polyLORRef(v2(), dotModular::LaneId{SequencerEngine::PL_REST},
                       SequencerEngine::LOR_OFF) = 3;
    }
};

static void laneRead(const Rig& r, bool preview, int& L, int& O, int& R) {
    SequencerEngine::LaneDir d;
    if (preview)
        r.eng.effectiveLanePreview(v2(), dotModular::LaneId{SequencerEngine::PL_REST}, L, O, R, d);
    else
        r.eng.effectiveLaneLor(v2(), dotModular::LaneId{SequencerEngine::PL_REST}, L, O, R, d);
}

// ── P1: the LANE tier (the poly lane + the mono strands, the F2 slot-0 law) ───
static void p1_lane() {
    std::printf("P1 lane preview: identity, A/B divergence, commit, clamp, suspend\n");
    char msg[200];
    {   // G1 identity: no CV → preview == effective == base (5/3/0).
        Rig r;
        int Le, Oe, Re, Lp, Op, Rp;
        laneRead(r, false, Le, Oe, Re);
        laneRead(r, true, Lp, Op, Rp);
        std::snprintf(msg, sizeof(msg), "G1: no-CV preview (%d/%d/%d) != effective (%d/%d/%d)",
                      Lp, Op, Rp, Le, Oe, Re);
        CHECK(Lp == Le && Op == Oe && Rp == Re, msg);
        CHECK(Le == 5 && Oe == 3, "G1: the rig's authored base row is 5/3/0");
    }
    {   // G2 divergence: pending L+2/O+1 → preview 7/4/0, effective still 5/3/0.
        Rig r;
        std::vector<int> d3 = {2, 1, 0};
        r.eng.pushLaneLorCvPoly(v2(), SequencerEngine::PL_REST, d3.data());
        int Le, Oe, Re, Lp, Op, Rp;
        laneRead(r, false, Le, Oe, Re);
        laneRead(r, true, Lp, Op, Rp);
        std::snprintf(msg, sizeof(msg), "G2: effective moved with only PENDING pushed (%d/%d/%d)",
                      Le, Oe, Re);
        CHECK(Le == 5 && Oe == 3 && Re == 0, msg);
        std::snprintf(msg, sizeof(msg), "G2: preview != base+pending (%d/%d/%d, want 7/4/0)",
                      Lp, Op, Rp);
        CHECK(Lp == 7 && Op == 4 && Rp == 0, msg);
        // G6 read-only: previewing did not commit (the effective read is unchanged).
        laneRead(r, false, Le, Oe, Re);
        std::snprintf(msg, sizeof(msg), "G6: previewing committed behind the display's back (%d/%d/%d)",
                      Le, Oe, Re);
        CHECK(Le == 5 && Oe == 3, msg);
        // G3 commit follows: the C1b gate copies pend→committed → preview == effective.
        r.eng.refreshLaneLatchesForVoice(v2().v, true, true);
        laneRead(r, false, Le, Oe, Re);
        laneRead(r, true, Lp, Op, Rp);
        std::snprintf(msg, sizeof(msg), "G3: after the commit gate preview (%d/%d/%d) != effective (%d/%d/%d)",
                      Lp, Op, Rp, Le, Oe, Re);
        CHECK(Lp == Le && Op == Oe && Rp == Re, msg);
        CHECK(Le == 7 && Oe == 4, "G3: the commit landed the pushed delta (7/4/0)");
    }
    {   // G4 clamp: a pending row that violates the invariants still comes out valid
        //     (lorRowClamp_ applied) — and so does the effective row. The rows differ
        //     by design (pending != committed); the law is EACH row's validity.
        Rig r;
        std::vector<int> d3 = {-10, 100, 50};   // L→<1, O→>ring, R→>L−1
        r.eng.pushLaneLorCvPoly(v2(), SequencerEngine::PL_REST, d3.data());
        int Le, Oe, Re, Lp, Op, Rp;
        laneRead(r, false, Le, Oe, Re);
        laneRead(r, true, Lp, Op, Rp);
        auto rowValid = [](int L, int O, int R) {
            return L >= 1 && O >= 0 && O <= dotModular::kMaskRing - 1 && R >= 0 && R <= L - 1;
        };
        std::snprintf(msg, sizeof(msg), "G4: the PREVIEW row violates the invariants (%d/%d/%d)",
                      Lp, Op, Rp);
        CHECK(rowValid(Lp, Op, Rp), msg);
        std::snprintf(msg, sizeof(msg), "G4: the EFFECTIVE row violates the invariants (%d/%d/%d)",
                      Le, Oe, Re);
        CHECK(rowValid(Le, Oe, Re), msg);
        CHECK(Lp == 1 && Op == dotModular::kMaskRing - 1,
              "G4: the preview clamp did not bind (want L=1, O=ring−1)");
    }
    {   // G5 suspend: the suspended item HOLDS committed, preview still tracks pending;
        //     a non-suspended item commits through (per-item, the reducer's zone map).
        //     The lane suspend scope is the (voice, lane) pair the gesture owns.
        Rig r;
        std::vector<int> d3 = {2, 1, 0};   // L+2 O+1 R+0
        r.eng.pushLaneLorCvPoly(v2(), SequencerEngine::PL_REST, d3.data());
        const bool m3[3] = {false, false, false}, v3[3] = {false, false, false};
        const bool l3[3] = {true, false, false};   // suspend LENGTH only (Z_END map)
        r.eng.setLorCvSuspend(m3, v2().v, v3, (int)SequencerEngine::PL_REST, l3);
        r.eng.refreshLaneLatchesForVoice(v2().v, true, true);
        int Le, Oe, Re, Lp, Op, Rp;
        laneRead(r, false, Le, Oe, Re);
        laneRead(r, true, Lp, Op, Rp);
        std::snprintf(msg, sizeof(msg), "G5: the suspended item committed anyway (effective %d/%d/%d, want 5/4/0)",
                      Le, Oe, Re);
        CHECK(Le == 5 && Oe == 4 && Re == 0, msg);   // L held, O committed through
        std::snprintf(msg, sizeof(msg), "G5: the suspended item's preview froze too (%d/%d/%d, want 7/4/0)",
                      Lp, Op, Rp);
        CHECK(Lp == 7 && Op == 4, msg);              // preview still shows the live window
        r.eng.setLorCvSuspend(m3, -1, v3, -1, l3);   // clear (absent scope)
    }
    {   // The MONO STRANDS (F2 slot 0): the same law one row up — asserted as the
        // RELATION (preview == base+pending, effective == base); the mono strand's
        // authored base is the reset default, not this rig's poly row.
        Rig r;
        std::vector<int> d3 = {1, 0, 0};
        r.eng.pushLaneLorCvPoly(dotModular::VoiceResolver::mono(), SequencerEngine::PL_REST, d3.data());
        int Le, Oe, Re, Lp, Op, Rp; SequencerEngine::LaneDir d;
        r.eng.effectiveMonoLaneLor(SequencerEngine::PL_REST, Le, Oe, Re, d);
        r.eng.effectiveMonoLanePreview(SequencerEngine::PL_REST, Lp, Op, Rp, d);
        std::snprintf(msg, sizeof(msg), "G2-mono: preview (%d/%d/%d) != base+pending (%d+1/%d/%d)",
                      Lp, Op, Rp, Le, Oe, Re);
        CHECK(Lp == Le + 1 && Op == Oe && Rp == Re, msg);
        r.eng.refreshLaneLatchesForVoice(0, true, true);   // slot 0 = the mono strands' row
        r.eng.effectiveMonoLaneLor(SequencerEngine::PL_REST, Le, Oe, Re, d);
        r.eng.effectiveMonoLanePreview(SequencerEngine::PL_REST, Lp, Op, Rp, d);
        std::snprintf(msg, sizeof(msg), "G3-mono: after the commit preview (%d/%d/%d) != effective (%d/%d/%d)",
                      Lp, Op, Rp, Le, Oe, Re);
        CHECK(Lp == Le && Op == Oe && Rp == Re, msg);
    }
}

// ── P2: the VOICE + MACRO ROW tiers ───────────────────────────────────────────
static void p2_voice_macro() {
    std::printf("P2 voice/macro-row preview: identity, divergence, commit\n");
    char msg[200];
    {   // G1 identity: no CV → the east-local voice's preview == its effective == base.
        Rig r;
        int Le, Oe, Re, Lp, Op, Rp; SequencerEngine::LaneDir de, dp;
        r.eng.effectiveVoiceLor(v2().v, Le, Oe, Re, de);
        r.eng.effectiveVoicePreview(v2().v, Lp, Op, Rp, dp);
        std::snprintf(msg, sizeof(msg), "G1-voice: no-CV preview (%d/%d/%d) != effective (%d/%d/%d)",
                      Lp, Op, Rp, Le, Oe, Re);
        CHECK(Lp == Le && Op == Oe && Rp == Re, msg);
        CHECK(Le == 4, "G1-voice: the rig's authored voice base is L=4");
    }
    {   // G2 divergence + G3 commit (the east voice row).
        Rig r;
        std::vector<int> m3 = {0, 0, 0};
        std::vector<int> east(16 * 3, 0);
        east[v2().v * 3 + SequencerEngine::LOR_LEN] = 3;
        r.eng.pushVoiceLorCv(m3.data(), east.data());
        int Le, Oe, Re, Lp, Op, Rp; SequencerEngine::LaneDir de, dp;
        r.eng.effectiveVoiceLor(v2().v, Le, Oe, Re, de);
        r.eng.effectiveVoicePreview(v2().v, Lp, Op, Rp, dp);
        std::snprintf(msg, sizeof(msg), "G2-voice: preview (%d/%d/%d) != base+pending (7/..) or effective moved (%d/%d/%d)",
                      Lp, Op, Rp, Le, Oe, Re);
        CHECK(Lp == 7 && Op == Oe && Rp == Re, msg);
        CHECK(Le == 4, "G2-voice: the effective read moved with only pending pushed");
        r.eng.refreshVoiceLorLatches();
        r.eng.effectiveVoiceLor(v2().v, Le, Oe, Re, de);
        r.eng.effectiveVoicePreview(v2().v, Lp, Op, Rp, dp);
        std::snprintf(msg, sizeof(msg), "G3-voice: after the commit preview (%d/%d/%d) != effective (%d/%d/%d)",
                      Lp, Op, Rp, Le, Oe, Re);
        CHECK(Lp == Le && Op == Oe && Rp == Re, msg);
        CHECK(Le == 7, "G3-voice: the commit landed (7/..)");
    }
    {   // The MACRO ROW (voiceLorCvPendMacro_): the row every follower reads through.
        Rig r;
        std::vector<int> m3 = {0, 2, 0};
        std::vector<int> east(16 * 3, 0);
        r.eng.pushVoiceLorCv(m3.data(), east.data());
        int Le, Oe, Re, Lp, Op, Rp; SequencerEngine::LaneDir de, dp;
        r.eng.effectiveMacroRowLor(Le, Oe, Re, de);
        r.eng.effectiveMacroRowPreview(Lp, Op, Rp, dp);
        std::snprintf(msg, sizeof(msg), "G2-macro: preview (%d/%d/%d) != base+pending (%d/2/%d) or effective moved (%d/%d/%d)",
                      Lp, Op, Rp, Oe, Re, Le, Oe, Re);
        CHECK(Lp == Le && Op == Oe + 2 && Rp == Re, msg);
        // G3-macro: the same refresh commits the macro row too.
        r.eng.refreshVoiceLorLatches();
        r.eng.effectiveMacroRowLor(Le, Oe, Re, de);
        std::snprintf(msg, sizeof(msg), "G3-macro: the committed macro OFF did not land (O=%d, want base+2)",
                      Oe);
        CHECK(Op == Oe, msg);
        r.eng.effectiveMacroRowPreview(Lp, Op, Rp, dp);
        std::snprintf(msg, sizeof(msg), "G3-macro: after the commit preview (%d/%d/%d) != effective (%d/%d/%d)",
                      Lp, Op, Rp, Le, Oe, Re);
        CHECK(Lp == Le && Op == Oe && Rp == Re, msg);
    }
}

int main() {
    std::printf("test_lane_preview_unify — CC_PROMPT_LANE_RANGE_DISPLAY_UNIFY Stage B\n");
    p1_lane();
    p2_voice_macro();
    if (g_fail == 0) std::printf("ALL PASS\n");
    else             std::printf("%d FAILURES\n", g_fail);
    return g_fail ? 1 : 0;
}
