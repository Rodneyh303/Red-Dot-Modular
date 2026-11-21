// diag_problane_phase_coupling.cpp — CC_PROMPT_REVERSE_PROBLANE_ROTATION_EDGE, UPDATE 2
// (the re-aimed STEP 1: MEASURE THE COUPLING, no fix yet).
//
// THE CONFIG TRAP (the doc's mandate): reversing the VOICE must actually reverse the
// LANES, as in Rodney's Rack. The faithful V1 rig: the voice row REVERSED
// (voiceLorDir_[0], the range-menu gesture) + the lane cells LEFT AT FORWARD — the
// lanes reverse THROUGH the voice (the multiplicative direction rule: the lane walk
// hangs off the voice frame's VALUE, which descends). The rig ASSERTS the lane walk
// actually descends (the anti-lying guard) — a rig whose lanes still walk forward
// proves nothing.
//
// THE MEASUREMENT (the doc's acceptance test): the transport step at which the
// prob-lane playhead sits on its rotation cell (the SHIPPED feed law:
// lanePlayheadBar(laneTick_[s], L, O, R)), vs the step at which the VOICE is at its
// directional entry extreme — the LEFT cell (O_v) forward / the RIGHT cell
// (O_v+L_v-1) reverse. WINDOWED voice range (Rodney's example: O_v=3, L_v=6 →
// cells 3..8, the `8 7 6 5 4 3` walk), lane R swept (R==0 degenerate — mid-range
// emphasized), voice R swept {0,2,5}.
//
// THE CANDIDATE (computed off the measured tick, pre-fix — the coupling lever the
// doc redirects to): anchor the lane phase at the direction-entry frame value —
// tick' = tick - walkEdge(vdir, L_v) — so the rotation-hit lands where the voice's
// frame VALUE equals walkEdge (read image: the LEFT cell forward / the RIGHT cell
// reverse). The fix belongs in deriveHolders' frameT0[v] (the lane-seed
// representative), NOT in the draw.
#include <cstdio>
#include <cstdint>
#include "engines/SequencerEngine.hpp"
#include "ui/SandsRangeGestureCore.hpp"
#include "VoiceResolver.hpp"

using dotModular::VoiceResolver;

static constexpr int RING = dotModular::kMaskRing;
static const int STRAND = dotModular::STRAND_RHYTHM;   // the REST probability lane
static constexpr int LV = 6, OV = 3;                    // the WINDOWED voice: cells 3..8
static constexpr int LL = 6, OL = 0;                    // the lane's window
static constexpr int LEFT = OV, RIGHT = OV + LV - 1;    // the voice's extremes (3 and 8)
static constexpr int STEPS = 18;                        // three lane periods at L=6

struct Rec { int vr, tick, ph, wnow; };

static void rig(SequencerEngine& eng, SequencerEngine::LaneDir vdir, int RV, int RL) {
    eng.reset();
    eng.updateWindow(16.f, 0.f, false, 0.f, 0.f, false);   // the pass-through global 16@0
    eng.setVoiceLorLocalEast(dotModular::kMonoVoiceSlot, true);
    eng.voiceLorRef(dotModular::kMonoVoiceSlot, SequencerEngine::LOR_LEN) = LV;
    eng.voiceLorRef(dotModular::kMonoVoiceSlot, SequencerEngine::LOR_OFF) = OV;
    eng.voiceLorRef(dotModular::kMonoVoiceSlot, SequencerEngine::LOR_ROT) = RV;
    eng.voiceLorDir_[0] = vdir;                            // the range-menu gesture
    eng.lorRef(STRAND, SequencerEngine::LOR_LEN) = LL;     // the lane row (cells stay Forward)
    eng.lorRef(STRAND, SequencerEngine::LOR_OFF) = OL;
    eng.lorRef(STRAND, SequencerEngine::LOR_ROT) = RL;
}

static std::vector<Rec> march(SequencerEngine& eng, int RL) {
    std::vector<Rec> out;
    out.reserve(STEPS);
    for (int t = 0; t < STEPS; ++t) {
        eng.advancePlayhead(1, nullptr);
        const int tick = eng.laneTick_[STRAND];
        out.push_back({ eng.voiceRead_[0], tick,
                        redDot::SandsRangeGestureCore::lanePlayheadBar(tick, LL, OL, RL, RING),
                        (int)((((int64_t)eng.voiceDriver_[0] - 1) % LV + LV) % LV) });
    }
    return out;
}

int main() {
    std::printf("diag_problane_phase_coupling — UPDATE 2 STEP 1: WHEN is the lane on its rotation,\n");
    std::printf("and WHICH voice extreme is the voice at then? (voice O=%d L=%d cells %d..%d, lane L=%d O=%d)\n",
                OV, LV, OV, OV + LV - 1, LL, OL);
    int lyingRigs = 0;
    for (int vd = 0; vd < 2; ++vd) {
        const auto vdir = vd ? SequencerEngine::LaneDir::Reverse : SequencerEngine::LaneDir::Forward;
        const int edge = (int)SequencerEngine::walkEdge(vdir, LV);   // 0 fwd / LV-1 rev
        for (const int RV : { 0, 2, 5 }) {
            // ── the anti-lying guards (per voice config, once) ──
            {
                SequencerEngine eng;
                rig(eng, vdir, RV, 2);
                const std::vector<Rec> rec = march(eng, 2);
                int l_, o_, r_; SequencerEngine::LaneDir d_;
                eng.effectiveMonoLaneLor(STRAND, l_, o_, r_, d_);
                // the lane WALK must follow the voice's direction (the multiplicative rule):
                // the tick steps by -1 (mod LL) under a reversed voice, +1 under forward.
                // The PHRASE wrap (the global 16) is a BEAT — the frame value jumps there,
                // so the delta crossing it (t == 16, phrasePos wrapping) is skipped.
                int walkOk = 0, checked = 0;
                for (size_t t = 1; t < rec.size(); ++t) {
                    if (t == 16) continue;   // the phrase-wrap beat
                    ++checked;
                    const int step = ((rec[t].tick - rec[t - 1].tick) % LL + LL) % LL;
                    if (step == (vd ? LL - 1 : 1)) ++walkOk;
                }
                const bool lanesReversed = (walkOk == checked);
                if (!lanesReversed) ++lyingRigs;
                std::printf("\n=== VOICE %s, R_v=%d | voice reads: ", vd ? "REVERSE" : "FORWARD", RV);
                for (int t = 0; t < 8; ++t) std::printf("%d ", rec[t].vr);
                std::printf("| lane cells %s (effectiveMonoLaneLor d=%s), lane walk %s %s\n",
                            "FWD", d_ == SequencerEngine::LaneDir::Reverse ? "REV" : "FWD",
                            vd ? "REVERSED" : "FORWARD",
                            lanesReversed ? "(guard OK)" : "(GUARD FAILED — RIG IS LYING)");
                std::printf("    entry read vr[0]=%d (want O_v+R_v=%d — the UAS anchor / strip-unchanged witness)\n",
                            rec[0].vr, OV + RV);
            }
            std::printf("    laneR | onRot steps (vr@)      | LEFT steps (vr=3) | RIGHT steps (vr=8) | CANDIDATE onRot (vr@)\n");
            for (int RL = 0; RL < LL; ++RL) {
                SequencerEngine eng;
                rig(eng, vdir, RV, RL);
                const std::vector<Rec> rec = march(eng, RL);
                std::printf("    %s R=%d | ", RL == 0 ? "·" : " ", RL);
                bool first = true;
                for (int t = 0; t < STEPS; ++t)
                    if (rec[t].ph == (OL + RL) % RING) { std::printf("%s%d(vr=%d)", first ? "" : ",", t, rec[t].vr); first = false; }
                if (first) std::printf("none");
                std::printf(" | ");
                first = true;
                for (int t = 0; t < STEPS; ++t)
                    if (rec[t].vr == LEFT) { std::printf("%s%d", first ? "" : ",", t); first = false; }
                if (first) std::printf("none");
                std::printf("     | ");
                first = true;
                for (int t = 0; t < STEPS; ++t)
                    if (rec[t].vr == RIGHT) { std::printf("%s%d", first ? "" : ",", t); first = false; }
                if (first) std::printf("none");
                std::printf("     | ");
                // THE ANCHORED LAW (the coupling lever, computed off the MEASURED frame
                // value — valid pre- AND post-fix): the lanes hang off wnow - walkEdge
                // (the direction-entry frame value). Pre-fix the engine's tick is the
                // un-anchored wnow (this column is the candidate); post-fix the shipped
                // onRot column REALIZES this column (they must coincide exactly).
                first = true;
                for (int t = 0; t < STEPS; ++t) {
                    const int tick2 = (((rec[t].wnow - edge) % LL) + LL) % LL;
                    if (redDot::SandsRangeGestureCore::lanePlayheadBar(tick2, LL, OL, RL, RING) == (OL + RL) % RING) {
                        std::printf("%s%d(vr=%d)", first ? "" : ",", t, rec[t].vr); first = false;
                    }
                }
                if (first) std::printf("none");
                std::printf("\n");
            }
        }
    }
    std::printf("\nlying rigs (the lane walk failed to follow the voice): %d\n", lyingRigs);
    return 0;
}
