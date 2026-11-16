// test_lor_phrase_latch.cpp — CC_PROMPT_CADENCE_C1_LOR_PHRASE_LATCH
//
// The FIRST deliberate behaviour change of the cadence arc: nested LOR (len/off/rot)
// stops taking effect PER STEP and latches at the level's ADDRESSED-AGAINST frame
// (DESIGN_POSITION_UNIFICATION.md, the CADENCE addendum + the C1 REFINEMENT):
//   - voice-lane LOR (voiceLorStore_[v], macroVoiceLor_) — parent is the GLOBAL →
//     the global phrase edge (pos_.phrasePos()==0);
//   - probability-lane LOR (polyLOR/lor strands) — parent is the VOICE → the
//     voice-frame edge (voiceFramePhase_[v]==0; for an IDENTITY row the holder
//     phase is not maintained — its pass-through cycle IS the global phrase, so
//     the edge is the phrase);
//   - the boundary is evaluated LIVE each step (the 8→6 trace: shrinking the
//     global length moves the voice-lane latch point to the new, earlier wrap);
//   - global length/offset stay STEP-rate (the MeloDicer-native class), and C2
//     keep-counting: phrasePos folds mod the new length, never re-seeds.
// Direction stays live (C3's slice).
//
// Reproduce-first: P1/P1b/P2/P3 FAIL on the pre-slice engine (the store change
// bites the very next step); P4 (C2) already holds and is pinned so it survives.
// The A/B/C march structure: A = old value throughout, B = new value from reset,
// C = old value with the change applied mid-march. The reads under a MAPPED row
// are pure functions of (phrasePos, L, O, R) — the phrase-anchored fold — so
// C must equal A bit-for-bit until the latch edge and B from it.
#include <cstdio>
#include <vector>
#include "engines/SequencerEngine.hpp"
#include "VoiceResolver.hpp"

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("  FAIL: %s\n", m); ++g_fail; } } while (0)

static constexpr int STEPS = 26;

struct March {
    std::vector<int> read;      // the observable per step (post-advance)
    std::vector<int> pp;        // pos_.phrasePos() per step (post-advance)
    std::vector<int> phase;     // voiceFramePhase_[v] per step (post-advance)
};

// The engine voice slot for V2 (the first poly voice).
static dotModular::VoiceSlot v2() { return dotModular::VoiceResolver::slot(2); }

// ── P1: a VOICE ROW's L change lands at the next GLOBAL phrase edge ──────────
static void p1_voice_row_at_phrase_edge() {
    std::printf("P1 voice-row L change -> next global phrase edge\n");
    const int v = v2().v;
    March march[3];   // A, B, C
    for (int which = 0; which < 3; ++which) {
        SequencerEngine eng; eng.reset();
        eng.updateWindow(8.f, 0.f, false, 0.f, 0.f, false);
        eng.numPolyVoices = 2;
        eng.setVoiceLorLocalEast(v2(), true);
        const int L0 = (which == 1) ? 6 : 4;             // B starts at the new L
        eng.voiceLorRef(v2(), SequencerEngine::LOR_LEN) = L0;
        for (int t = 0; t < STEPS; ++t) {
            eng.advancePlayhead(1, nullptr);
            if (which == 2 && t == 1)                    // C: the change mid-phrase
                eng.voiceLorRef(v2(), SequencerEngine::LOR_LEN) = 6;
            march[which].read.push_back(eng.voiceRead_[v]);
            march[which].pp.push_back(eng.pos_.phrasePos());
            march[which].phase.push_back(eng.voiceFramePhase_[v]);
        }
    }
    // the latch edge: the first step AFTER the change with phrasePos==0
    int edge = -1;
    for (int t = 2; t < STEPS; ++t)
        if (march[2].pp[t] == 0) { edge = t; break; }
    char msg[160];
    std::snprintf(msg, sizeof(msg), "P1: no phrase edge found after the change (edge=%d)", edge);
    CHECK(edge > 2, msg);
    if (edge <= 2) return;
    for (int t = 2; t < edge; ++t) {
        std::snprintf(msg, sizeof(msg), "P1: step %d (pp=%d) already moved to the new window (read %d, want the old %d) — the LOR latched per step, not at the phrase",
                      t, march[2].pp[t], march[2].read[t], march[0].read[t]);
        CHECK(march[2].read[t] == march[0].read[t], msg);
    }
    // The switch evidence: both windows read the SAME at the entry (the rotation
    // anchor law), so the probe is the first DIVERGENCE from the old-window march —
    // it must appear within one old-window cycle of the edge, and the from-edge
    // loop below pins C==B from the edge itself.
    { bool diverged = false;
      for (int t = edge + 1; t < edge + 5 && t < STEPS; ++t) diverged = diverged || (march[2].read[t] != march[0].read[t]);
      CHECK(diverged, "P1: the read never left the old window after the edge — no switch happened");
    }
    for (int t = edge; t < STEPS; ++t) {
        std::snprintf(msg, sizeof(msg), "P1: step %d after the edge disagrees with the new-window march (read %d, want %d)",
                      t, march[2].read[t], march[1].read[t]);
        CHECK(march[2].read[t] == march[1].read[t], msg);
    }
    // THE REFINEMENT'S NEGATIVE: the voice row's parent is the GLOBAL — the voice's
    // OWN frame edge (phase==0, mid-phrase at t==4 for L=4) must NOT latch it.
    std::snprintf(msg, sizeof(msg), "P1: the voice row switched at its OWN frame edge t=4 (read %d == new %d) — the rung is the parent's phrase, not the row's frame",
                  march[2].read[4], march[1].read[4]);
    CHECK(march[2].read[4] != march[1].read[4], msg);
}

// ── P1b: the MACRO ROW's L change lands at the next global phrase edge ───────
static void p1b_macro_row_at_phrase_edge() {
    std::printf("P1b macro-row L change -> next global phrase edge\n");
    March march[3];
    for (int which = 0; which < 3; ++which) {
        SequencerEngine eng; eng.reset();
        eng.updateWindow(8.f, 0.f, false, 0.f, 0.f, false);
        eng.macroVoiceLor_[SequencerEngine::LOR_LEN] = (which == 1) ? 6 : 4;
        for (int t = 0; t < STEPS; ++t) {
            eng.advancePlayhead(1, nullptr);
            if (which == 2 && t == 1)
                eng.macroVoiceLor_[SequencerEngine::LOR_LEN] = 6;
            march[which].read.push_back(eng.macroFrameRead());
            march[which].pp.push_back(eng.pos_.phrasePos());
        }
    }
    int edge = -1;
    for (int t = 2; t < STEPS; ++t)
        if (march[2].pp[t] == 0) { edge = t; break; }
    char msg[160];
    std::snprintf(msg, sizeof(msg), "P1b: no phrase edge found after the change (edge=%d)", edge);
    CHECK(edge > 2, msg);
    if (edge <= 2) return;
    for (int t = 2; t < edge; ++t) {
        std::snprintf(msg, sizeof(msg), "P1b: step %d (pp=%d) already moved to the new window (read %d, want the old %d)",
                      t, march[2].pp[t], march[2].read[t], march[0].read[t]);
        CHECK(march[2].read[t] == march[0].read[t], msg);
    }
    { bool diverged = false;
      for (int t = edge + 1; t < edge + 5 && t < STEPS; ++t) diverged = diverged || (march[2].read[t] != march[0].read[t]);
      CHECK(diverged, "P1b: the macro read never left the old window after the edge — no switch happened");
    }
    for (int t = edge; t < STEPS; ++t) {
        std::snprintf(msg, sizeof(msg), "P1b: step %d after the edge disagrees with the new-window march (read %d, want %d)",
                      t, march[2].read[t], march[1].read[t]);
        CHECK(march[2].read[t] == march[1].read[t], msg);
    }
}

// ── P2: a PROBABILITY LANE's L change lands at the next VOICE-FRAME edge ─────
// V2 carries a mapped row {L=4} (its frame edge every 4 steps, mid-phrase at 4),
// its REST lane runs L=3; the change to 5 must land at phase==0, not per step
// and not at the global phrase.
static void p2_lane_at_voice_frame_edge() {
    std::printf("P2 probability-lane L change -> next voice-frame edge\n");
    const int strand = dotModular::STRAND_RHYTHM;
    March march[3];
    for (int which = 0; which < 3; ++which) {
        SequencerEngine eng; eng.reset();
        eng.updateWindow(8.f, 0.f, false, 0.f, 0.f, false);
        eng.numPolyVoices = 2;
        eng.setVoiceLorLocalEast(v2(), true);
        eng.voiceLorRef(v2(), SequencerEngine::LOR_LEN) = 4;   // the mapped row: the frame edge every 4
        eng.polyLORRef(v2(), dotModular::LaneId{SequencerEngine::PL_REST}, SequencerEngine::LOR_LEN) = (which == 1) ? 5 : 3;
        for (int t = 0; t < STEPS; ++t) {
            eng.advancePlayhead(1, nullptr);
            if (which == 2 && t == 1)
                eng.polyLORRef(v2(), dotModular::LaneId{SequencerEngine::PL_REST}, SequencerEngine::LOR_LEN) = 5;
            march[which].read.push_back(eng.laneTickV_[v2().v][strand]);
            march[which].pp.push_back(eng.pos_.phrasePos());
            march[which].phase.push_back(eng.voiceFramePhase_[v2().v]);
        }
    }
    // the latch edge: the first step AFTER the change with the voice frame AT its entry
    int edge = -1;
    for (int t = 2; t < STEPS; ++t)
        if (march[2].phase[t] == 0) { edge = t; break; }
    char msg[160];
    std::snprintf(msg, sizeof(msg), "P2: no voice-frame edge found after the change (edge=%d)", edge);
    CHECK(edge > 2, msg);
    if (edge <= 2) return;
    std::snprintf(msg, sizeof(msg), "P2: the edge is the global phrase (pp=%d at t=%d) — the lane must latch at the VOICE frame, which lands mid-phrase",
                  march[2].pp[edge], edge);
    CHECK(march[2].pp[edge] != 0, msg);
    for (int t = 2; t < edge; ++t) {
        std::snprintf(msg, sizeof(msg), "P2: step %d (phase=%d) already moved to the new lane window (tick %d, want the old %d)",
                      t, march[2].phase[t], march[2].read[t], march[0].read[t]);
        CHECK(march[2].read[t] == march[0].read[t], msg);
    }
    { bool diverged = false;
      for (int t = edge + 1; t < edge + 4 && t < STEPS; ++t) diverged = diverged || (march[2].read[t] != march[0].read[t]);
      CHECK(diverged, "P2: the lane never left the old window after the voice-frame edge — no switch happened");
    }
    for (int t = edge; t < STEPS; ++t) {
        std::snprintf(msg, sizeof(msg), "P2: step %d after the edge disagrees with the new-window march (tick %d, want %d)",
                      t, march[2].read[t], march[1].read[t]);
        CHECK(march[2].read[t] == march[1].read[t], msg);
    }
}

// ── P3: the boundary is LIVE — shrinking the global 8→6 mid-phrase moves the ─
// voice-lane latch point to the new, EARLIER wrap (not the stale 8-cycle one).
static void p3_live_boundary_shrink8to6() {
    std::printf("P3 live boundary: global 8->6 mid-phrase moves the latch point earlier\n");
    const int v = v2().v;
    March march[3];   // all three shrink 8->6 at t==3; C also changes its row L there
    for (int which = 0; which < 3; ++which) {
        SequencerEngine eng; eng.reset();
        eng.updateWindow(8.f, 0.f, false, 0.f, 0.f, false);
        eng.numPolyVoices = 2;
        eng.setVoiceLorLocalEast(v2(), true);
        eng.voiceLorRef(v2(), SequencerEngine::LOR_LEN) = (which == 1) ? 6 : 4;
        for (int t = 0; t < STEPS; ++t) {
            eng.advancePlayhead(1, nullptr);
            if (t == 3) {
                eng.updateWindow(6.f, 0.f, false, 0.f, 0.f, false);   // the STEP-rate shrink (all marches)
                if (which == 2) eng.voiceLorRef(v2(), SequencerEngine::LOR_LEN) = 6;   // the pending change rides the moved boundary
            }
            march[which].read.push_back(eng.voiceRead_[v]);
            march[which].pp.push_back(eng.pos_.phrasePos());
        }
    }
    // the phrasePos fold under keep-counting: 3 stays 3 (3 < 6), the wrap lands at t=6
    int edge = -1;
    for (int t = 4; t < STEPS; ++t)
        if (march[2].pp[t] == 0) { edge = t; break; }
    char msg[160];
    std::snprintf(msg, sizeof(msg), "P3: no phrase edge after the shrink (edge=%d)", edge);
    CHECK(edge == 6, msg);
    if (edge != 6) return;
    std::snprintf(msg, sizeof(msg), "P3: the latch waited for the STALE 8-cycle boundary (edge t=%d) — the boundary must be evaluated live (the 8->6 trace)", edge);
    CHECK(edge < 8, msg);
    for (int t = 4; t < edge; ++t) {
        std::snprintf(msg, sizeof(msg), "P3: step %d (pp=%d) already moved to the new window (read %d, want the old %d)",
                      t, march[2].pp[t], march[2].read[t], march[0].read[t]);
        CHECK(march[2].read[t] == march[0].read[t], msg);
    }
    { bool diverged = false;
      for (int t = edge + 1; t < edge + 5 && t < STEPS; ++t) diverged = diverged || (march[2].read[t] != march[0].read[t]);
      CHECK(diverged, "P3: the read never left the old window after the moved boundary — no switch happened");
    }
    for (int t = edge; t < STEPS; ++t) {
        std::snprintf(msg, sizeof(msg), "P3: step %d after the moved boundary disagrees with the new-window march (read %d, want %d)",
                      t, march[2].read[t], march[1].read[t]);
        CHECK(march[2].read[t] == march[1].read[t], msg);
    }
}

// ── P4/P5: global L+O stay STEP-rate and C2 keeps counting ───────────────────
// (i) 10→8 at phrasePos 4 (no fold): the played walk continues (no re-seed) and
//     the wrap bites at the NEW length (the step-rate class).
// (ii) 10→6 at phrasePos 8: the TRANSPORT keeps counting (the tick is untouched
//     by the re-author) and its phrase view FOLDS mod the new length — never
//     re-seeds; the played column re-parks into the shrunk window on the next
//     advance (the identity harness's documented re-author defence).
// NOTE: after t advances phrasePos == t mod lenG — the FIRST advance lands AT
// the phrase entry (phrasePos 0), the park's entry law.
static void p4_c2_keep_counting_and_step_rate_global() {
    std::printf("P4 C2 keep-counting + global L/O stay step-rate\n");
    char msg[160];
    {   // (i) no-fold case
        SequencerEngine eng; eng.reset();
        eng.updateWindow(10.f, 0.f, false, 0.f, 0.f, false);
        for (int t = 0; t < 5; ++t) eng.advancePlayhead(1, nullptr);
        std::snprintf(msg, sizeof(msg), "P4(i): march sanity (phrasePos %d, want 4)", eng.pos_.phrasePos());
        CHECK(eng.pos_.phrasePos() == 4, msg);
        eng.updateWindow(8.f, 0.f, false, 0.f, 0.f, false);
        eng.advancePlayhead(1, nullptr);
        std::snprintf(msg, sizeof(msg), "P4(i): phrasePos after the shrink step is %d, want 5 (the walk plays THROUGH — no re-seed)",
                      eng.pos_.phrasePos());
        CHECK(eng.pos_.phrasePos() == 5, msg);
        eng.advancePlayhead(1, nullptr);
        CHECK(eng.pos_.phrasePos() == 6, "P4(i): phrasePos 6");
        eng.advancePlayhead(1, nullptr);
        CHECK(eng.pos_.phrasePos() == 7, "P4(i): phrasePos 7");
        eng.advancePlayhead(1, nullptr);
        std::snprintf(msg, sizeof(msg), "P4(i): the wrap bites at the NEW length (phrasePos %d, want 0 at the 8-cycle — was 10)",
                      eng.pos_.phrasePos());
        CHECK(eng.pos_.phrasePos() == 0, msg);
    }
    {   // (ii) the fold case (shrink BELOW the current phrase coordinate)
        SequencerEngine eng; eng.reset();
        eng.updateWindow(10.f, 0.f, false, 0.f, 0.f, false);
        for (int t = 0; t < 9; ++t) eng.advancePlayhead(1, nullptr);
        std::snprintf(msg, sizeof(msg), "P4(ii): march sanity (phrasePos %d, want 8)", eng.pos_.phrasePos());
        CHECK(eng.pos_.phrasePos() == 8, msg);
        const int64_t tickBefore = eng.transportTick_;
        const int foldBefore = eng.pos_.phaseClock(eng.transportTick_);
        eng.updateWindow(6.f, 0.f, false, 0.f, 0.f, false);
        std::snprintf(msg, sizeof(msg), "P4(ii): the transport tick moved with the re-author (%lld -> %lld) — KEEP-COUNTING means the clock is untouched",
                      (long long)tickBefore, (long long)eng.transportTick_);
        CHECK(eng.transportTick_ == tickBefore, msg);
        const int foldAfter = eng.pos_.phaseClock(eng.transportTick_);
        std::snprintf(msg, sizeof(msg), "P4(ii): the transport phrase view folded %d -> %d, want 8 -> 2 (mod the new length, never re-seeded)",
                      foldBefore, foldAfter);
        CHECK(foldBefore == 8 && foldAfter == 2, msg);
        eng.advancePlayhead(1, nullptr);
        std::snprintf(msg, sizeof(msg), "P4(ii): the played column re-parks INTO the shrunk window (pp %d)", eng.pos_.phrasePos());
        CHECK(eng.pos_.phrasePos() >= 0 && eng.pos_.phrasePos() < 6, msg);
        std::snprintf(msg, sizeof(msg), "P4(ii): the transport fold continues from the fold (%d, want 3)", eng.pos_.phaseClock(eng.transportTick_));
        CHECK(eng.pos_.phaseClock(eng.transportTick_) == 3, msg);
    }
}

// ── P5/P6 (CC_PROMPT_CADENCE_C1B STAGE A): the latch commit is lock-aware ────
// The commit rides the shared boundary-commit vocabulary: (phrase edge OR unlock
// flush) gated by the axis's ScopeBit — the engine-side twins of
// LockManager::queueFires()/liveNow(Control::Lor, ...) (kept include-free per
// dirLive_'s law). Under lock (scope bit NOT opted out) the latch HOLDS — the
// store may churn (the managers keep writing it) but the effective reads freeze —
// and the first step event after unlock FLUSHES the accumulated change. A lane
// whose axis bit is set in scopeLiveMask commits at the phrase edge even under
// lock (the dirLive_ opt-out). Unlocked marches are unchanged (P1-P4 above).
static void p5p6_lock_aware_commit() {
    std::printf("P5/P6 lock-aware latch commit (CC_PROMPT_CADENCE_C1B STAGE A)\n");
    const int v = v2().v;
    char msg[160];
    {   // P5: under lock the latch pends; at the unlock edge it flushes.
        // Probe: both windows read the SAME at the entry (the rotation-anchor law),
        // so the pend is proven at the first DIVERGENCE — pp mod 4 (the held old
        // window) vs pp mod 6 (the pended edit) differ from pp == 4.
        SequencerEngine eng; eng.reset();
        eng.updateWindow(8.f, 0.f, false, 0.f, 0.f, false);
        eng.numPolyVoices = 2;
        eng.setVoiceLorLocalEast(v2(), true);
        eng.voiceLorRef(v2(), SequencerEngine::LOR_LEN) = 4;
        for (int t = 0; t < 3; ++t) eng.advancePlayhead(1, nullptr);
        eng.locked = true;                       // the lock closes (the engine's own bool — Monsoon aliases it)
        for (int t = 0; t < 3; ++t) eng.advancePlayhead(1, nullptr);   // the tracker sees the locked state
        eng.voiceLorRef(v2(), SequencerEngine::LOR_LEN) = 6;   // the edit lands on the STORE mid-phrase, under lock
        bool held = true;                        // the phrase edge passes UNDER LOCK; march past pp==4
        bool sawDivergenceStep = false;
        int divergeRead = -1;
        for (int t = 0; t < 14; ++t) {
            eng.advancePlayhead(1, nullptr);
            const int pp = eng.pos_.phrasePos();
            if (pp == 4 && !sawDivergenceStep) {   // held: 4 mod 4 = 0; pended: 4 mod 6 = 4
                sawDivergenceStep = true;
                divergeRead = eng.voiceRead_[v];
                held = held && (divergeRead == 0);
            }
        }
        std::snprintf(msg, sizeof(msg), "P5: never reached the divergence step (pp==4)");
        CHECK(sawDivergenceStep, msg);
        std::snprintf(msg, sizeof(msg), "P5: the effective read moved to the new window UNDER LOCK at the divergence step (read %d, want the held 0) — the latch must pend while the axis is frozen",
                      divergeRead);
        CHECK(held && sawDivergenceStep, msg);
        eng.locked = false;                      // the unlock edge
        eng.advancePlayhead(1, nullptr);         // the first step event after unlock = queueFires's unlockNow flush
        bool walksNew = true;
        for (int t = 0; t < 8; ++t) {
            eng.advancePlayhead(1, nullptr);
            walksNew = walksNew && (eng.voiceRead_[v] == eng.pos_.phrasePos() % 6);   // the flush = the CLOSED FORM under the new window
        }
        std::snprintf(msg, sizeof(msg), "P5: the read does not walk the new window after the unlock flush (read %d at pp %d)",
                      eng.voiceRead_[v], eng.pos_.phrasePos());
        CHECK(walksNew, msg);
    }
    {   // P6: the scope opt-out — with the axis's ScopeBit set, the commit fires at
        // the phrase edge EVEN UNDER LOCK (the dirLive_ opt-out, SB_SANDS_R for the
        // voice family).
        SequencerEngine eng; eng.reset();
        eng.updateWindow(8.f, 0.f, false, 0.f, 0.f, false);
        eng.numPolyVoices = 2;
        eng.setVoiceLorLocalEast(v2(), true);
        eng.voiceLorRef(v2(), SequencerEngine::LOR_LEN) = 4;
        eng.scopeLiveMask = 1u << 2;             // SB_SANDS_R: the rhythm-axis LOR is live-under-lock
        for (int t = 0; t < 3; ++t) eng.advancePlayhead(1, nullptr);
        eng.locked = true;
        for (int t = 0; t < 3; ++t) eng.advancePlayhead(1, nullptr);
        eng.voiceLorRef(v2(), SequencerEngine::LOR_LEN) = 6;
        int edgeAt = -1;
        for (int t = 0; t < 8; ++t) {
            eng.advancePlayhead(1, nullptr);
            if (eng.pos_.phrasePos() == 0) { edgeAt = t; break; }
        }
        std::snprintf(msg, sizeof(msg), "P6: no phrase edge observed (edgeAt=%d)", edgeAt);
        CHECK(edgeAt >= 0, msg);
        if (edgeAt < 0) return;
        std::snprintf(msg, sizeof(msg), "P6: the read at the opted-out axis's edge is %d, want %d (the store change must commit at the edge under lock)",
                      eng.voiceRead_[v], eng.pos_.phrasePos() % 6);
        CHECK(eng.voiceRead_[v] == eng.pos_.phrasePos() % 6, msg);
    }
}

int main() {
    std::printf("== test_lor_phrase_latch (CC_PROMPT_CADENCE_C1) ==\n");
    p1_voice_row_at_phrase_edge();
    p1b_macro_row_at_phrase_edge();
    p2_lane_at_voice_frame_edge();
    p3_live_boundary_shrink8to6();
    p4_c2_keep_counting_and_step_rate_global();
    p5p6_lock_aware_commit();
    if (g_fail == 0) std::printf("all green\n");
    else std::printf("%d FAILED\n", g_fail);
    return g_fail ? 1 : 0;
}
