// test_lor_phrase_latch.cpp — CC_PROMPT_CADENCE_C1 + C1b (the cadence latch suite)
//
// C1 (merged): nested LOR latched at the level's ADDRESSED-AGAINST frame — voice
// rows at the global phrase edge, probability lanes at the voice-frame edge (an
// identity row's pass-through cycle IS the phrase), the boundary evaluated LIVE
// (the 8→6 trace), C2 keep-counting pinned.
//
// C1b Stage B RE-EXPRESSES the cadence (the doc's DECIDED law): the BASE and the CV
// are split — `effective = base_live + cv_committed_at_boundary`. A BASE change (the
// store rows — what a knob/drag edits) now bites IMMEDIATELY while unlocked (the C1
// "hold every LOR edit to the phrase" was wrong for base edits); a CV change (the
// manager's delta push) lands only at the boundary. The commit gates stay Stage A's:
// (phrase edge OR unlock flush) gated by the axis's ScopeBit — lock-aware (P5/P6).
//
// The A/B/C march structure survives unchanged: A = no change, B = the value applied
// from reset, C = applied mid-march. The change VEHICLE distinguishes the cadence:
// a store write (base → immediate) vs the engine's CV-delta push (boundary). The
// reads under a MAPPED row are pure in (phrasePos, L, O, R), so C==A until the CV
// edge and C==B from it, bit-for-bit; the base variant asserts C==B from the change
// step itself. Reproduce-first (Stage B): the base-immediacy pins FAILED on the C1
// engine (the combined latch held base edits to the edge — the captured run); the
// CV pins are new API and pin the split from birth.
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
// A zeroed east CV-delta block ([16*3] slot-major).
static std::vector<int> zeroEast() { return std::vector<int>(16 * 3, 0); }

// ── P1: the VOICE ROW — base immediate, CV at the global phrase edge ──────────
static void p1_voice_row() {
    std::printf("P1 voice row: base immediate (unlocked), CV at the phrase edge\n");
    const int v = v2().v;
    char msg[160];
    {   // BASE: the store edit bites the NEXT STEP (immediate).
        March march[3];
        for (int which = 0; which < 3; ++which) {
            SequencerEngine eng; eng.reset();
            eng.updateWindow(8.f, 0.f, false, 0.f, 0.f, false);
            eng.numPolyVoices = 2;
            eng.setVoiceLorLocalEast(v2(), true);
            eng.voiceLorRef(v2(), SequencerEngine::LOR_LEN) = (which == 1) ? 6 : 4;
            for (int t = 0; t < STEPS; ++t) {
                eng.advancePlayhead(1, nullptr);
                if (which == 2 && t == 1) eng.voiceLorRef(v2(), SequencerEngine::LOR_LEN) = 6;
                march[which].read.push_back(eng.voiceRead_[v]);
                march[which].pp.push_back(eng.pos_.phrasePos());
            }
        }
        for (int t = 2; t < STEPS; ++t) {
            std::snprintf(msg, sizeof(msg), "P1-base: step %d (pp=%d) still on the old window (read %d, want %d) — a base edit must land IMMEDIATELY while unlocked",
                          t, march[2].pp[t], march[2].read[t], march[1].read[t]);
            CHECK(march[2].read[t] == march[1].read[t], msg);
        }
    }
    {   // CV: the delta push lands at the global phrase edge (the C1 cadence, CV-ward).
        March march[3];
        for (int which = 0; which < 3; ++which) {
            SequencerEngine eng; eng.reset();
            eng.updateWindow(8.f, 0.f, false, 0.f, 0.f, false);
            eng.numPolyVoices = 2;
            eng.setVoiceLorLocalEast(v2(), true);
            if (which == 1) {   // B: the delta applied from reset (committed at the first edge)
                std::vector<int> m3 = {2, 0, 0};
                std::vector<int> east = zeroEast();
                east[v * 3 + SequencerEngine::LOR_LEN] = 2;
                eng.pushVoiceLorCv(m3.data(), east.data());
            }
            for (int t = 0; t < STEPS; ++t) {
                eng.advancePlayhead(1, nullptr);
                if (which == 2 && t == 1) {   // C: the delta pushed mid-phrase (pending, not committed)
                    std::vector<int> m3 = {2, 0, 0};
                    std::vector<int> east = zeroEast();
                    east[v * 3 + SequencerEngine::LOR_LEN] = 2;
                    eng.pushVoiceLorCv(m3.data(), east.data());
                }
                march[which].read.push_back(eng.voiceRead_[v]);
                march[which].pp.push_back(eng.pos_.phrasePos());
            }
        }
        int edge = -1;
        for (int t = 2; t < STEPS; ++t)
            if (march[2].pp[t] == 0) { edge = t; break; }
        std::snprintf(msg, sizeof(msg), "P1-cv: no phrase edge found after the change (edge=%d)", edge);
        CHECK(edge > 2, msg);
        if (edge <= 2) return;
        for (int t = 2; t < edge; ++t) {
            std::snprintf(msg, sizeof(msg), "P1-cv: step %d (pp=%d) already moved to the new window (read %d, want the old %d) — CV must commit only at the boundary",
                          t, march[2].pp[t], march[2].read[t], march[0].read[t]);
            CHECK(march[2].read[t] == march[0].read[t], msg);
        }
        for (int t = edge; t < STEPS; ++t) {
            std::snprintf(msg, sizeof(msg), "P1-cv: step %d after the edge disagrees with the new-window march (read %d, want %d)",
                          t, march[2].read[t], march[1].read[t]);
            CHECK(march[2].read[t] == march[1].read[t], msg);
        }
        // THE REFINEMENT'S NEGATIVE: the voice row's parent is the GLOBAL — the voice's
        // OWN frame edge (mid-phrase) must NOT commit its CV.
        { bool switchedEarly = false;
          for (int t = 2; t < edge; ++t) switchedEarly = switchedEarly || (march[2].read[t] == march[1].read[t] && march[2].read[t] != march[0].read[t]);
          CHECK(!switchedEarly, "P1-cv: the CV committed at the row's OWN frame edge — the rung is the parent's phrase");
        }
    }
}

// ── P1b: the MACRO ROW — base immediate, CV at the global phrase edge ─────────
static void p1b_macro_row() {
    std::printf("P1b macro row: base immediate (unlocked), CV at the phrase edge\n");
    char msg[160];
    {   // BASE immediate.
        March march[3];
        for (int which = 0; which < 3; ++which) {
            SequencerEngine eng; eng.reset();
            eng.updateWindow(8.f, 0.f, false, 0.f, 0.f, false);
            eng.macroVoiceLor_[SequencerEngine::LOR_LEN] = (which == 1) ? 6 : 4;
            for (int t = 0; t < STEPS; ++t) {
                eng.advancePlayhead(1, nullptr);
                if (which == 2 && t == 1) eng.macroVoiceLor_[SequencerEngine::LOR_LEN] = 6;
                march[which].read.push_back(eng.macroFrameRead());
                march[which].pp.push_back(eng.pos_.phrasePos());
            }
        }
        for (int t = 2; t < STEPS; ++t) {
            std::snprintf(msg, sizeof(msg), "P1b-base: step %d (pp=%d) still on the old window (read %d, want %d)",
                          t, march[2].pp[t], march[2].read[t], march[1].read[t]);
            CHECK(march[2].read[t] == march[1].read[t], msg);
        }
    }
    {   // CV at the phrase edge.
        March march[3];
        for (int which = 0; which < 3; ++which) {
            SequencerEngine eng; eng.reset();
            eng.updateWindow(8.f, 0.f, false, 0.f, 0.f, false);
            if (which == 1) { std::vector<int> m3 = {2, 0, 0}; std::vector<int> east = zeroEast(); eng.pushVoiceLorCv(m3.data(), east.data()); }
            for (int t = 0; t < STEPS; ++t) {
                eng.advancePlayhead(1, nullptr);
                if (which == 2 && t == 1) { std::vector<int> m3 = {2, 0, 0}; std::vector<int> east = zeroEast(); eng.pushVoiceLorCv(m3.data(), east.data()); }
                march[which].read.push_back(eng.macroFrameRead());
                march[which].pp.push_back(eng.pos_.phrasePos());
            }
        }
        int edge = -1;
        for (int t = 2; t < STEPS; ++t)
            if (march[2].pp[t] == 0) { edge = t; break; }
        std::snprintf(msg, sizeof(msg), "P1b-cv: no phrase edge found after the change (edge=%d)", edge);
        CHECK(edge > 2, msg);
        if (edge <= 2) return;
        for (int t = 2; t < edge; ++t) {
            std::snprintf(msg, sizeof(msg), "P1b-cv: step %d (pp=%d) already moved to the new window (read %d, want the old %d)",
                          t, march[2].pp[t], march[2].read[t], march[0].read[t]);
            CHECK(march[2].read[t] == march[0].read[t], msg);
        }
        for (int t = edge; t < STEPS; ++t) {
            std::snprintf(msg, sizeof(msg), "P1b-cv: step %d after the edge disagrees with the new-window march (read %d, want %d)",
                          t, march[2].read[t], march[1].read[t]);
            CHECK(march[2].read[t] == march[1].read[t], msg);
        }
    }
}

// ── P2: the PROBABILITY LANE — base immediate, CV at the VOICE-FRAME edge ─────
// V2 carries a mapped row {L=4} (its frame edge every 4 steps, mid-phrase at 4),
// its REST lane runs L=3; the change to 5: base bites immediately, CV lands at
// phase==0 — NOT per step and not at the global phrase.
static void p2_lane() {
    std::printf("P2 probability lane: base immediate (unlocked), CV at the voice-frame edge\n");
    const int strand = dotModular::STRAND_RHYTHM;
    char msg[160];
    {   // BASE immediate.
        March march[3];
        for (int which = 0; which < 3; ++which) {
            SequencerEngine eng; eng.reset();
            eng.updateWindow(8.f, 0.f, false, 0.f, 0.f, false);
            eng.numPolyVoices = 2;
            eng.setVoiceLorLocalEast(v2(), true);
            eng.voiceLorRef(v2(), SequencerEngine::LOR_LEN) = 4;
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
        for (int t = 2; t < STEPS; ++t) {
            std::snprintf(msg, sizeof(msg), "P2-base: step %d (phase=%d) still on the old lane window (tick %d, want %d)",
                          t, march[2].phase[t], march[2].read[t], march[1].read[t]);
            CHECK(march[2].read[t] == march[1].read[t], msg);
        }
    }
    {   // CV at the voice-frame edge.
        March march[3];
        for (int which = 0; which < 3; ++which) {
            SequencerEngine eng; eng.reset();
            eng.updateWindow(8.f, 0.f, false, 0.f, 0.f, false);
            eng.numPolyVoices = 2;
            eng.setVoiceLorLocalEast(v2(), true);
            eng.voiceLorRef(v2(), SequencerEngine::LOR_LEN) = 4;
            if (which == 1) { std::vector<int> d3 = {2, 0, 0}; eng.pushLaneLorCvPoly(v2(), SequencerEngine::PL_REST, d3.data()); }
            for (int t = 0; t < STEPS; ++t) {
                eng.advancePlayhead(1, nullptr);
                if (which == 2 && t == 1) { std::vector<int> d3 = {2, 0, 0}; eng.pushLaneLorCvPoly(v2(), SequencerEngine::PL_REST, d3.data()); }
                march[which].read.push_back(eng.laneTickV_[v2().v][strand]);
                march[which].pp.push_back(eng.pos_.phrasePos());
                march[which].phase.push_back(eng.voiceFramePhase_[v2().v]);
            }
        }
        int edge = -1;
        for (int t = 2; t < STEPS; ++t)
            if (march[2].phase[t] == 0) { edge = t; break; }
        std::snprintf(msg, sizeof(msg), "P2-cv: no voice-frame edge found after the change (edge=%d)", edge);
        CHECK(edge > 2, msg);
        if (edge <= 2) return;
        std::snprintf(msg, sizeof(msg), "P2-cv: the edge is the global phrase (pp=%d at t=%d) — the lane CV must latch at the VOICE frame, which lands mid-phrase",
                      march[2].pp[edge], edge);
        CHECK(march[2].pp[edge] != 0, msg);
        for (int t = 2; t < edge; ++t) {
            std::snprintf(msg, sizeof(msg), "P2-cv: step %d (phase=%d) already moved to the new lane window (tick %d, want the old %d)",
                          t, march[2].phase[t], march[2].read[t], march[0].read[t]);
            CHECK(march[2].read[t] == march[0].read[t], msg);
        }
        for (int t = edge; t < STEPS; ++t) {
            std::snprintf(msg, sizeof(msg), "P2-cv: step %d after the edge disagrees with the new-window march (tick %d, want %d)",
                          t, march[2].read[t], march[1].read[t]);
            CHECK(march[2].read[t] == march[1].read[t], msg);
        }
    }
}

// ── P3: the boundary is LIVE — shrinking the global 8→6 mid-phrase moves the ──
// voice-row CV latch point to the new, EARLIER wrap (not the stale 8-cycle one).
static void p3_live_boundary_shrink8to6() {
    std::printf("P3 live boundary: global 8->6 mid-phrase moves the CV latch point earlier\n");
    const int v = v2().v;
    char msg[160];
    March march[3];   // all three shrink 8->6 at t==3; C also pushes its CV delta there
    for (int which = 0; which < 3; ++which) {
        SequencerEngine eng; eng.reset();
        eng.updateWindow(8.f, 0.f, false, 0.f, 0.f, false);
        eng.numPolyVoices = 2;
        eng.setVoiceLorLocalEast(v2(), true);
        eng.voiceLorRef(v2(), SequencerEngine::LOR_LEN) = (which == 1) ? 6 : 4;
        if (which == 1) { std::vector<int> m3 = {0, 0, 0}; std::vector<int> east = zeroEast(); east[v * 3 + SequencerEngine::LOR_LEN] = 2; eng.pushVoiceLorCv(m3.data(), east.data()); }
        for (int t = 0; t < STEPS; ++t) {
            eng.advancePlayhead(1, nullptr);
            if (t == 3) {
                eng.updateWindow(6.f, 0.f, false, 0.f, 0.f, false);   // the STEP-rate shrink (all marches)
                if (which == 2) {   // the pending CV delta rides the MOVED boundary
                    std::vector<int> m3 = {0, 0, 0};
                    std::vector<int> east = zeroEast();
                    east[v * 3 + SequencerEngine::LOR_LEN] = 2;
                    eng.pushVoiceLorCv(m3.data(), east.data());
                }
            }
            march[which].read.push_back(eng.voiceRead_[v]);
            march[which].pp.push_back(eng.pos_.phrasePos());
        }
    }
    int edge = -1;
    for (int t = 4; t < STEPS; ++t)
        if (march[2].pp[t] == 0) { edge = t; break; }
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
    for (int t = edge; t < STEPS; ++t) {
        std::snprintf(msg, sizeof(msg), "P3: step %d after the moved boundary disagrees with the new-window march (read %d, want %d)",
                      t, march[2].read[t], march[1].read[t]);
        CHECK(march[2].read[t] == march[1].read[t], msg);
    }
}

// ── P4: global L+O stay STEP-rate and C2 keeps counting ───────────────────────
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

// ── P5/P6 (C1b Stage A): the CV commit is lock-aware ──────────────────────────
// (phrase edge OR unlock flush) gated by the axis's ScopeBit. Under lock the
// committed CV holds (the pending may churn); the first step event after unlock
// flushes. A lane whose axis bit is set commits at the phrase edge even under lock.
static void p5p6_lock_aware_commit() {
    std::printf("P5/P6 lock-aware CV commit (CC_PROMPT_CADENCE_C1B STAGE A)\n");
    const int v = v2().v;
    char msg[160];
    {   // P5: under lock the committed CV pends; at the unlock edge it flushes.
        SequencerEngine eng; eng.reset();
        eng.updateWindow(8.f, 0.f, false, 0.f, 0.f, false);
        eng.numPolyVoices = 2;
        eng.setVoiceLorLocalEast(v2(), true);
        eng.voiceLorRef(v2(), SequencerEngine::LOR_LEN) = 4;
        for (int t = 0; t < 3; ++t) eng.advancePlayhead(1, nullptr);
        eng.locked = true;                       // the lock closes (the engine's own bool — Monsoon aliases it)
        for (int t = 0; t < 3; ++t) eng.advancePlayhead(1, nullptr);   // the tracker sees the locked state
        {   // the CV delta lands on the PENDING side mid-phrase, under lock
            std::vector<int> m3 = {2, 0, 0};
            std::vector<int> east = zeroEast();
            east[v * 3 + SequencerEngine::LOR_LEN] = 2;
            eng.pushVoiceLorCv(m3.data(), east.data());
        }
        bool held = true;                        // march past the divergence (pp==4: held 4 mod 4 = 0; pended 4 mod 6 = 4)
        bool sawDivergenceStep = false;
        int divergeRead = -1;
        for (int t = 0; t < 14; ++t) {
            eng.advancePlayhead(1, nullptr);
            const int pp = eng.pos_.phrasePos();
            if (pp == 4 && !sawDivergenceStep) {
                sawDivergenceStep = true;
                divergeRead = eng.voiceRead_[v];
                held = held && (divergeRead == 0);
            }
        }
        std::snprintf(msg, sizeof(msg), "P5: never reached the divergence step (pp==4)");
        CHECK(sawDivergenceStep, msg);
        std::snprintf(msg, sizeof(msg), "P5: the effective read moved to the new window UNDER LOCK at the divergence step (read %d, want the held 0) — the committed CV must pend while the axis is frozen",
                      divergeRead);
        CHECK(held && sawDivergenceStep, msg);
        eng.locked = false;                      // the unlock edge
        eng.advancePlayhead(1, nullptr);         // the first step event after unlock = the flush
        bool walksNew = true;
        for (int t = 0; t < 8; ++t) {
            eng.advancePlayhead(1, nullptr);
            walksNew = walksNew && (eng.voiceRead_[v] == eng.pos_.phrasePos() % 6);   // the flush = the CLOSED FORM under the new window
        }
        std::snprintf(msg, sizeof(msg), "P5: the read does not walk the new window after the unlock flush (read %d at pp %d)",
                      eng.voiceRead_[v], eng.pos_.phrasePos());
        CHECK(walksNew, msg);
    }
    {   // P6: the scope opt-out — with the axis's ScopeBit set, the CV commit fires at
        // the phrase edge EVEN UNDER LOCK (the dirLive_ opt-out, SB_SANDS_R for the voice family).
        SequencerEngine eng; eng.reset();
        eng.updateWindow(8.f, 0.f, false, 0.f, 0.f, false);
        eng.numPolyVoices = 2;
        eng.setVoiceLorLocalEast(v2(), true);
        eng.voiceLorRef(v2(), SequencerEngine::LOR_LEN) = 4;
        eng.scopeLiveMask = 1u << 2;             // SB_SANDS_R: the rhythm-axis LOR is live-under-lock
        for (int t = 0; t < 3; ++t) eng.advancePlayhead(1, nullptr);
        eng.locked = true;
        for (int t = 0; t < 3; ++t) eng.advancePlayhead(1, nullptr);
        {   // the CV delta pending mid-phrase
            std::vector<int> m3 = {2, 0, 0};
            std::vector<int> east = zeroEast();
            east[v * 3 + SequencerEngine::LOR_LEN] = 2;
            eng.pushVoiceLorCv(m3.data(), east.data());
        }
        int edgeAt = -1;
        for (int t = 0; t < 8; ++t) {
            eng.advancePlayhead(1, nullptr);
            if (eng.pos_.phrasePos() == 0) { edgeAt = t; break; }
        }
        std::snprintf(msg, sizeof(msg), "P6: no phrase edge observed (edgeAt=%d)", edgeAt);
        CHECK(edgeAt >= 0, msg);
        if (edgeAt < 0) return;
        std::snprintf(msg, sizeof(msg), "P6: the read at the opted-out axis's edge is %d, want %d (the CV must commit at the edge under lock)",
                      eng.voiceRead_[v], eng.pos_.phrasePos() % 6);
        CHECK(eng.voiceRead_[v] == eng.pos_.phrasePos() % 6, msg);
    }
}

// ── P7 (C1b Stage B): the row invariants hold on the COMPOSED row ─────────────
// A CV delta cannot push L below 1, O out of the ring, or R outside [0, L-1].
static void p7_row_invariants_on_composed() {
    std::printf("P7 row invariants on the composed (base+CV) row\n");
    const int v = v2().v;
    char msg[160];
    SequencerEngine eng; eng.reset();
    eng.updateWindow(8.f, 0.f, false, 0.f, 0.f, false);
    eng.numPolyVoices = 2;
    eng.setVoiceLorLocalEast(v2(), true);
    eng.voiceLorRef(v2(), SequencerEngine::LOR_LEN) = 4;
    eng.voiceLorRef(v2(), SequencerEngine::LOR_ROT) = 2;
    eng.advancePlayhead(1, nullptr);
    eng.advancePlayhead(1, nullptr);   // OFF the commit edge (pp==1): the pending must wait for the boundary
    {   // a negative LEN delta big enough to sink the row
        std::vector<int> m3 = {0, 0, 0};
        std::vector<int> east = zeroEast();
        east[v * 3 + SequencerEngine::LOR_LEN]  = -9;   // 4 - 9 < 1
        east[v * 3 + SequencerEngine::LOR_ROT]  = +9;   // R would escape [0, L-1]
        eng.pushVoiceLorCv(m3.data(), east.data());
    }
    for (int t = 0; t < 8 && eng.pos_.phrasePos() != 0; ++t) eng.advancePlayhead(1, nullptr);   // the phrase edge commits the pending
    std::printf("  (P7 commit edge at pp=%d)\n", eng.pos_.phrasePos());
    int L, O, R; SequencerEngine::LaneDir d;
    eng.effectiveVoiceLor(v, L, O, R, d);
    std::snprintf(msg, sizeof(msg), "P7: the composed row broke the invariants (L=%d R=%d)", L, R);
    CHECK(L >= 1 && R >= 0 && R <= L - 1, msg);
    std::snprintf(msg, sizeof(msg), "P7: the composed row is not the clamped expectation (L=%d, want 1; R=%d, want 0)", L, R);
    CHECK(L == 1 && R == 0, msg);
}

// ── P8 (C1b Stage C): THE EDIT-SUSPEND — during an active LOR gesture the CV ──
// commit for the EDITED SCOPE holds the pre-gesture sample (the window tracks only
// the live base — "a stable target"); other scopes/voices commit normally; on
// release the CV resumes at the NEXT boundary. The test drives the engine's suspend
// push directly (the rack-free form of the manager's per-block gesture snapshot).
static void p8_edit_suspend() {
    std::printf("P8 edit-suspend during an active gesture\n");
    const int v = v2().v;
    const int v3 = dotModular::VoiceResolver::slot(3).v;
    char msg[160];
    {   // the voice row: V2's LEN suspended — its CV holds; V3's commits at the edge.
        SequencerEngine eng; eng.reset();
        eng.updateWindow(8.f, 0.f, false, 0.f, 0.f, false);
        eng.numPolyVoices = 4;
        eng.setVoiceLorLocalEast(v2(), true);
        eng.setVoiceLorLocalEast(dotModular::VoiceResolver::slot(3), true);
        eng.voiceLorRef(v2(), SequencerEngine::LOR_LEN) = 4;
        eng.voiceLorRef(dotModular::VoiceResolver::slot(3), SequencerEngine::LOR_LEN) = 4;
        {   // the pre-gesture sample: +2 on BOTH voices, committed at the first edge
            std::vector<int> m3 = {0, 0, 0};
            std::vector<int> east = zeroEast();
            east[v * 3 + SequencerEngine::LOR_LEN]  = 2;
            east[v3 * 3 + SequencerEngine::LOR_LEN] = 2;
            eng.pushVoiceLorCv(m3.data(), east.data());
        }
        eng.advancePlayhead(1, nullptr);   // the first edge commits (+2 each: both rows walk mod 6)
        eng.advancePlayhead(1, nullptr);   // off-edge
        {   // THE GESTURE ENGAGES on V2 (len+off items) and BOTH rows get new deltas:
            // V2's is suspended (holds 6); V3's commits at the boundary (-> 4).
            std::array<bool, 3> mac3 = {false, false, false};
            std::array<bool, 3> vS = {true, false, false};   // the LEN item (the window zones)
            std::array<bool, 3> lS = {false, false, false};
            eng.setLorCvSuspend(mac3.data(), v, vS.data(), -1, lS.data());
            std::vector<int> m3 = {0, 0, 0};
            std::vector<int> east = zeroEast();
            east[v * 3 + SequencerEngine::LOR_LEN]  = -2;   // V2: back toward the base — SUSPENDED
            east[v3 * 3 + SequencerEngine::LOR_LEN] = -2;   // V3: the same push — NOT suspended
            eng.pushVoiceLorCv(m3.data(), east.data());
        }
        int edge = -1;
        for (int t = 0; t < 9; ++t) {
            eng.advancePlayhead(1, nullptr);
            if (eng.pos_.phrasePos() == 0) { edge = t; break; }
        }
        std::snprintf(msg, sizeof(msg), "P8: no boundary observed after the suspended push (edge=%d)", edge);
        CHECK(edge > 0, msg);
        if (edge <= 0) return;
        // Probe at the DIVERGENCE step (pp==4): both windows read the same at the
        // entry (the rotation-anchor law) — the held row walks mod 6 (read 4), the
        // committed row mod 4 (read 0).
        int v2Read4 = -1, v3Read4 = -1;
        for (int t = 0; t < 9; ++t) {
            eng.advancePlayhead(1, nullptr);
            if (eng.pos_.phrasePos() == 4) { v2Read4 = eng.voiceRead_[v]; v3Read4 = eng.voiceRead_[v3]; break; }
        }
        std::snprintf(msg, sizeof(msg), "P8: never reached the divergence step (pp==4)");
        CHECK(v2Read4 >= 0, msg);
        if (v2Read4 < 0) return;
        std::snprintf(msg, sizeof(msg), "P8: the SUSPENDED scope moved during the gesture (V2 read %d at pp 4, want the held 4) — the gesture must hold the pre-gesture sample",
                      v2Read4);
        CHECK(v2Read4 == 4, msg);
        std::snprintf(msg, sizeof(msg), "P8: the NON-suspended voice did not commit at the boundary (V3 read %d at pp 4, want 0 mod 4)",
                      v3Read4);
        CHECK(v3Read4 == 0, msg);
        {   // RELEASE: the suspend clears; the pending (−2) commits at the NEXT boundary.
            std::array<bool, 3> mac3 = {false, false, false};
            std::array<bool, 3> vS = {false, false, false};
            std::array<bool, 3> lS = {false, false, false};
            eng.setLorCvSuspend(mac3.data(), -1, vS.data(), -1, lS.data());
        }
        int edge2 = -1;
        for (int t = 0; t < 10; ++t) {
            eng.advancePlayhead(1, nullptr);
            if (eng.pos_.phrasePos() == 0) { edge2 = t; break; }
        }
        std::snprintf(msg, sizeof(msg), "P8: no boundary after the release (edge2=%d)", edge2);
        CHECK(edge2 > 0, msg);
        if (edge2 <= 0) return;
        int v2After = -1;
        for (int t = 0; t < 9; ++t) {
            eng.advancePlayhead(1, nullptr);
            if (eng.pos_.phrasePos() == 4) { v2After = eng.voiceRead_[v]; break; }
        }
        std::snprintf(msg, sizeof(msg), "P8: the CV did not resume at the next boundary after release (V2 read %d at pp 4, want 0 mod 4)",
                      v2After);
        CHECK(v2After == 0, msg);
    }
}

int main() {
    std::printf("== test_lor_phrase_latch (the cadence latch suite: C1 + C1b) ==\n");
    p1_voice_row();
    p1b_macro_row();
    p2_lane();
    p3_live_boundary_shrink8to6();
    p4_c2_keep_counting_and_step_rate_global();
    p5p6_lock_aware_commit();
    p7_row_invariants_on_composed();
    p8_edit_suspend();
    if (g_fail == 0) std::printf("all green\n");
    else std::printf("%d FAILED\n", g_fail);
    return g_fail ? 1 : 0;
}
