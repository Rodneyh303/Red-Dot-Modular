// test_problane_phase_coupling.cpp — CC_PROMPT_REVERSE_PROBLANE_ROTATION_EDGE,
// UPDATE 2 (the re-aimed acceptance test). THE PROPERTY (the doc's verbatim
// definition): the transport step at which the prob-lane playhead sits on its
// rotation cell COINCIDES with the step at which the VOICE is at its first
// step — its DIRECTIONAL entry extreme: the LEFT cell (O_v) forward, the RIGHT
// cell (O_v+L_v-1) reverse (in reverse the voice's first step IS the right end;
// Rodney: "sync to its rotation at the right edge not the left one").
//
// THE RIG (the faithful V1 config, diag_problane_phase_coupling's): the
// WINDOWED voice row (O_v=3 L_v=6 → cells 3..8, the `8 7 6 5 4 3` walk at
// R_v=5) REVERSED via voiceLorDir_[0] (the range-menu gesture), the lane cells
// LEFT AT FORWARD — the lanes reverse THROUGH the voice (the multiplicative
// direction rule). The ANTI-LYING GUARD asserts the lane walk actually follows
// the voice's direction — a rig whose lanes still walk forward proves nothing
// (the doc's config trap).
//
// THE FIX UNDER TEST (the phase-coupling anchor, deriveHolders): the mapped
// row's lanes hang off the ENTRY-ANCHORED frame value (wnow − walkEdge(vd, L)),
// so the lane's rotation-hit phase tracks the voice's directional first step.
// REPRODUCE-FIRST (the pre-fix table, recorded in the commit): the un-anchored
// tie (lanes off raw wnow) put the rotation-hit on the voice's LEFT-extreme
// steps in BOTH directions — the bug — for every R_v and lane R; the anchored
// candidate sat on the RIGHT-extreme steps. The OLD-law mirror is pinned BELOW
// as still violating (its rotation-hit reconstructed on the LEFT-extreme steps).
//
// THE STRIP-UNCHANGED GUARD: the voice's entry read stays O_v+R_v (the UAS
// anchor) and the read never leaves the window — the strip draw / voiceRead_
// law is untouched by the lane-phase fix.
//
// Compile: see test/run_all.sh ("test_problane_phase_coupling").
#include <cstdio>
#include <vector>
#include "engines/SequencerEngine.hpp"
#include "ui/SandsRangeGestureCore.hpp"
#include "VoiceResolver.hpp"

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("  FAIL: %s\n", (m)); ++g_fail; } } while (0)

static constexpr int RING = dotModular::kMaskRing;
static const int STRAND = dotModular::STRAND_RHYTHM;   // the REST probability lane
static constexpr int LV = 6, OV = 3;                    // the WINDOWED voice: cells 3..8
static constexpr int LL = 6, OL = 0;                    // the lane's window (the primary)
static constexpr int LEFT = OV, RIGHT = OV + LV - 1;    // the voice's extremes (3 and 8)
static constexpr int STEPS = 18;                        // three lane periods at L=6

struct Rec { int vr, tick, ph, wnow; };

static std::vector<Rec> march(SequencerEngine::LaneDir vdir, int RV, int RL, int laneLen) {
    SequencerEngine eng;
    eng.reset();
    eng.updateWindow(16.f, 0.f, false, 0.f, 0.f, false);   // the pass-through global 16@0
    eng.setVoiceLorLocalEast(dotModular::kMonoVoiceSlot, true);
    eng.voiceLorRef(dotModular::kMonoVoiceSlot, SequencerEngine::LOR_LEN) = LV;
    eng.voiceLorRef(dotModular::kMonoVoiceSlot, SequencerEngine::LOR_OFF) = OV;
    eng.voiceLorRef(dotModular::kMonoVoiceSlot, SequencerEngine::LOR_ROT) = RV;
    eng.voiceLorDir_[0] = vdir;                            // the range-menu gesture
    eng.lorRef(STRAND, SequencerEngine::LOR_LEN) = laneLen;   // the lane row (cells stay Forward)
    eng.lorRef(STRAND, SequencerEngine::LOR_OFF) = OL;
    eng.lorRef(STRAND, SequencerEngine::LOR_ROT) = RL;
    std::vector<Rec> out;
    out.reserve(STEPS);
    for (int t = 0; t < STEPS; ++t) {
        eng.advancePlayhead(1, nullptr);
        const int tick = eng.laneTick_[STRAND];
        int l_, o_, r_; SequencerEngine::LaneDir d_;
        eng.effectiveMonoLaneLor(STRAND, l_, o_, r_, d_);
        out.push_back({ eng.voiceRead_[0], tick,
                        redDot::SandsRangeGestureCore::lanePlayheadBar(tick, l_, o_, r_, RING),
                        (int)((((int64_t)eng.voiceDriver_[0] - 1) % LV + LV) % LV) });
    }
    return out;
}

int main() {
    std::printf("test_problane_phase_coupling: the lane's rotation-hit coincides with the voice's DIRECTIONAL first step (LEFT fwd / the RIGHT wrap-back rev) — the phase-coupling anchor\n");
    char msg[256];

    // ── 1. THE COINCIDENCE: both directions x R_v {0,2,5} x lane R 0..5, L_l == L_v ──
    for (int vd = 0; vd < 2; ++vd) {
        const auto vdir = vd ? SequencerEngine::LaneDir::Reverse : SequencerEngine::LaneDir::Forward;
        const bool rev = vd != 0;
        const int edge = (int)SequencerEngine::walkEdge(vdir, LV);   // 0 fwd / LV-1 rev
        for (const int RV : { 0, 2, 5 }) {
            for (int RL = 0; RL < LL; ++RL) {
                const std::vector<Rec> rec = march(vdir, RV, RL, LL);

                // THE PROPERTY: at EVERY directional-extreme step the lane is on its
                // rotation cell — and (L_l == L_v: one hit per window) every
                // on-rotation step sits at the extreme read.
                for (int t = 0; t < STEPS; ++t) {
                    const bool atExt = rev ? (rec[t].vr == RIGHT) : (rec[t].vr == LEFT);
                    const bool onRot = rec[t].ph == (OL + RL) % RING;
                    if (atExt) {
                        std::snprintf(msg, sizeof msg, "%s R_v=%d laneR=%d step=%d: the voice is at its %s extreme (read %d) but the lane playhead %d is not on its rotation cell %d",
                                      rev ? "REV" : "FWD", RV, RL, t, rev ? "RIGHT" : "LEFT", rec[t].vr, rec[t].ph, (OL + RL) % RING);
                        CHECK(onRot, msg);
                    } else if (onRot) {
                        std::snprintf(msg, sizeof msg, "%s R_v=%d laneR=%d step=%d: an on-rotation step at a NON-extreme read (%d) — the hit must sit on the %s extreme",
                                      rev ? "REV" : "FWD", RV, RL, t, rec[t].vr, rev ? "RIGHT" : "LEFT");
                        CHECK(false, msg);
                    }
                }

                // THE ANTI-LYING GUARD (the doc's config trap): the lane walk FOLLOWS
                // the voice's direction (the multiplicative rule) — the tick steps
                // -1 (mod L) under a reversed voice, +1 under forward. The phrase
                // wrap (the global 16) is a BEAT — the delta crossing it is skipped.
                int walkOk = 0, checked = 0;
                for (size_t t = 1; t < rec.size(); ++t) {
                    if (t == 16) continue;   // the phrase-wrap beat
                    ++checked;
                    const int step = ((rec[t].tick - rec[t - 1].tick) % LL + LL) % LL;
                    if (step == (rev ? LL - 1 : 1)) ++walkOk;
                }
                std::snprintf(msg, sizeof msg, "%s R_v=%d laneR=%d: the lane walk does not follow the voice's direction (%d of %d uniform steps) — the rig is lying",
                              rev ? "REV" : "FWD", RV, RL, walkOk, checked);
                CHECK(checked > 0 && walkOk == checked, msg);

                // THE COUPLING LAW (the fix's expression, realized by the engine): the
                // lane tick hangs off the ENTRY-ANCHORED frame value.
                for (int t = 0; t < STEPS; ++t) {
                    const int want = (int)((((int64_t)rec[t].wnow - edge) % LL + LL) % LL);
                    const int got = ((rec[t].tick % LL) + LL) % LL;
                    std::snprintf(msg, sizeof msg, "%s R_v=%d laneR=%d step=%d: the lane tick %d does not hang off the anchored frame (wnow %d - edge %d = %d)",
                                  rev ? "REV" : "FWD", RV, RL, t, got, rec[t].wnow, edge, want);
                    CHECK(got == want, msg);
                }

                // THE STRIP-UNCHANGED GUARD: the entry read anchors at O_v+R_v (the
                // UAS law), and the read never leaves the windowed range.
                std::snprintf(msg, sizeof msg, "%s R_v=%d: the voice's entry read %d is not O_v+R_v=%d (the UAS anchor — the strip read must be untouched)",
                              rev ? "REV" : "FWD", RV, rec[0].vr, OV + RV);
                CHECK(rec[0].vr == OV + RV, msg);
                for (int t = 0; t < STEPS; ++t) {
                    std::snprintf(msg, sizeof msg, "%s R_v=%d step=%d: the voice read %d left the window [%d,%d)",
                                  rev ? "REV" : "FWD", RV, t, rec[t].vr, OV, OV + LV);
                    CHECK(rec[t].vr >= OV && rec[t].vr < OV + LV, msg);
                }

                // THE OLD-LAW PIN (the pre-fix tie, still violating in reverse): the
                // un-anchored tick (the engine's tick + the edge it now folds) puts
                // the rotation-hit on the voice's LEFT-extreme steps — the recorded
                // bug, kept visible so the fix stays honest.
                if (rev) {
                    int oldHitsAtLeft = 0;
                    for (int t = 0; t < STEPS; ++t) {
                        const int oldTick = (((rec[t].tick + edge) % LL) + LL) % LL;
                        const int oldPh = redDot::SandsRangeGestureCore::lanePlayheadBar(oldTick, LL, OL, RL, RING);
                        if (rec[t].vr == LEFT && oldPh == (OL + RL) % RING) ++oldHitsAtLeft;
                    }
                    std::snprintf(msg, sizeof msg, "REV R_v=%d laneR=%d: the OLD un-anchored law hit rotation on the LEFT-extreme steps %d times (the recorded bug — must be > 0)",
                                  RV, RL, oldHitsAtLeft);
                    CHECK(oldHitsAtLeft > 0, msg);
                }
            }
        }
    }

    // ── 2. A SHORTER LANE (L_l=4 under L_v=6): the lane wraps 1.5x per voice window,
    //    so extra on-rotation steps are correct — the COINCIDENCE at the extreme
    //    step is the law (any lane length, any lane R). ──
    for (int vd = 0; vd < 2; ++vd) {
        const auto vdir = vd ? SequencerEngine::LaneDir::Reverse : SequencerEngine::LaneDir::Forward;
        const bool rev = vd != 0;
        for (int RL = 0; RL < 4; ++RL) {
            const std::vector<Rec> rec = march(vdir, 2, RL, 4);
            for (int t = 0; t < STEPS; ++t) {
                const bool atExt = rev ? (rec[t].vr == RIGHT) : (rec[t].vr == LEFT);
                if (!atExt) continue;
                std::snprintf(msg, sizeof msg, "%s laneR=%d (L_l=4) step=%d: the voice is at its %s extreme but the shorter lane is not on its rotation cell",
                              rev ? "REV" : "FWD", RL, t, rev ? "RIGHT" : "LEFT");
                CHECK(rec[t].ph == (OL + RL) % RING, msg);
            }
        }
    }

    if (g_fail == 0) std::printf("all green\n");
    else std::printf("%d FAILED\n", g_fail);
    return g_fail ? 1 : 0;
}
