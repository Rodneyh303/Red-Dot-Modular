// test/test_reverse_anchor_ring.cpp — CC_PROMPT_REVERSE_ANCHOR_OFFBYONE
// (reproduce-first). In cycle mode the rotation-anchored read is
// wnow = (laneTickFor(vd, phrasePos+1, L) + R) mod L; at the parent's first step
// (phrasePos=0) Forward anchors at R but REVERSE anchored at R−1
// (laneTickFor(Reverse,1,L) = −1 ≡ L−1) — one cell left of rotation, walking
// R−1, R−2, … — contradicting the UAS "R is the cell the voice is at when the
// global is at its first step, ANY direction". Pendulum and PingPong already
// return 0 at t=1 and anchor at R.
//
// THE FIX (verified here): the ONE helper anchorBias(dir) = {F:0, Rev:+1,
// Pend:0, PP:0} applied uniformly at every rotation-anchored read (the frame
// reads' wnow + their wprev sign twins, laneRecomp's cur/prv, the display
// twin) — the reverse walk starts at 0 before + R. laneTickFor UNTOUCHED.
//
// GATES: the reverse voice read == R at phrasePos=0 (R 0/2/6, windowed G<L and
// full G=L), the sequence R, R−1, R−2, … (R=0, G=4, L=8 → 0,7,6,5); the abs1
// reverse read anchors the same as forward; a probability-LANE reverse read
// anchors at 0 (its rotation cell) at the parent's entry; the REGRESSION —
// Forward, Pendulum, PingPong still anchor at R, unchanged.
#include <cstdio>
#include <vector>
#include "engines/SequencerEngine.hpp"
#include "VoiceResolver.hpp"

using dotModular::VoiceResolver;
static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("  FAIL: %s\n", (m)); ++g_fail; } } while (0)

static const auto V2S = VoiceResolver::slot(2);

int main() {
    std::printf("test_reverse_anchor_ring — CC_PROMPT_REVERSE_ANCHOR_OFFBYONE (the reverse anchors at R)\n");
    char msg[200];

    // ── G1: the reverse VOICE read anchors at R; the sequence R, R−1, … ──────
    const struct { int G, L; const char* tag; } rigs[] = {
        {4, 8, "windowed G4<L8"},
        {8, 8, "full G8==L8"},
    };
    for (const auto& rig : rigs) {
        for (const int R : {0, 2, 6}) {
            SequencerEngine eng; eng.reset();
            eng.updateWindow((float)rig.G, 0.f, false, 0.f, 0.f, false);
            eng.numPolyVoices = 2;
            eng.setVoiceLorLocalEast(V2S, true);
            eng.voiceLorRef(V2S, SequencerEngine::LOR_LEN) = rig.L;
            eng.voiceLorRef(V2S, SequencerEngine::LOR_ROT) = R;
            eng.voiceLorDir_[V2S.v] = SequencerEngine::LaneDir::Reverse;
            int expect = R;
            for (int t = 0; t < rig.G; ++t) {
                eng.advancePlayhead(1, nullptr);
                const int pp = eng.pos_.phrasePos();
                const int want = ((expect % rig.L) + rig.L) % rig.L;
                std::snprintf(msg, sizeof msg, "G1 (%s, R=%d): phrase step %d frame value %d, want %d (R, R-1, R-2, ... walking backward)", rig.tag, R, pp, (int)(eng.voiceDriver_[V2S.v] - 1), want);
                CHECK(eng.voiceDriver_[V2S.v] - 1 == want, msg);
                --expect;
            }
        }
    }
    {   // The doc's exact sequence: R=0, G=4, L=8 -> 0,7,6,5.
        SequencerEngine eng; eng.reset();
        eng.updateWindow(4.f, 0.f, false, 0.f, 0.f, false);
        eng.numPolyVoices = 2;
        eng.setVoiceLorLocalEast(V2S, true);
        eng.voiceLorRef(V2S, SequencerEngine::LOR_LEN) = 8;
        eng.voiceLorRef(V2S, SequencerEngine::LOR_ROT) = 0;
        eng.voiceLorDir_[V2S.v] = SequencerEngine::LaneDir::Reverse;
        static const int want[4] = {0, 7, 6, 5};
        for (int t = 0; t < 4; ++t) {
            eng.advancePlayhead(1, nullptr);
            std::snprintf(msg, sizeof msg, "G1-exact: step %d frame value %d, want %d (R=0,G4,L8 -> 0,7,6,5)", t, (int)(eng.voiceDriver_[V2S.v] - 1), want[t]);
            CHECK(eng.voiceDriver_[V2S.v] - 1 == want[t], msg);
        }
    }

    // ── G2: the abs1 REVERSE read anchors exactly like forward ──────────────
    for (const int R : {0, 3}) {
        int fwd0 = -99, rev0 = -99;
        for (int pass = 0; pass < 2; ++pass) {
            SequencerEngine eng; eng.reset();
            eng.updateWindow(8.f, 0.f, false, 0.f, 0.f, false);
            eng.numPolyVoices = 2;
            eng.setVoiceLorLocalEast(V2S, true);
            eng.voiceLorRef(V2S, SequencerEngine::LOR_LEN) = 4;
            eng.voiceLorRef(V2S, SequencerEngine::LOR_ROT) = R;
            eng.voiceLorDir_[V2S.v] = (pass == 0) ? SequencerEngine::LaneDir::Forward
                                                  : SequencerEngine::LaneDir::Reverse;
            eng.maskLocalEast_[V2S.v] = true;
            eng.maskKind_[V2S.v] = SequencerEngine::MASK_DYNAMIC;
            eng.advancePlayhead(1, nullptr);
            (pass == 0 ? fwd0 : rev0) = (int)(eng.voiceDriver_[V2S.v] - 1);
        }
        std::snprintf(msg, sizeof msg, "G2 (R=%d): the abs1 reverse anchors at %d, want %d (the forward's anchor — the correction reached the abs1 read)", R, rev0, fwd0);
        CHECK(rev0 == fwd0, msg);
    }

    // ── G3: a probability-LANE reverse read anchors at 0 (its rotation cell) ─
    {
        SequencerEngine eng; eng.reset();
        eng.updateWindow(8.f, 0.f, false, 0.f, 0.f, false);
        eng.numPolyVoices = 2;
        eng.setVoiceLorLocalEast(V2S, true);
        eng.voiceLorRef(V2S, SequencerEngine::LOR_LEN) = 8;   // the row forward: frame value 0 at the entry
        eng.voiceLorRef(V2S, SequencerEngine::LOR_ROT) = 0;
        eng.voiceLorDir_[V2S.v] = SequencerEngine::LaneDir::Forward;
        eng.polyLORRef(V2S, dotModular::LaneId{SequencerEngine::PL_REST}, SequencerEngine::LOR_LEN) = 8;
        eng.polyLORRef(V2S, dotModular::LaneId{SequencerEngine::PL_REST}, SequencerEngine::LOR_ROT) = 2;
        eng.laneDirV_[V2S.v][dotModular::STRAND_RHYTHM] = SequencerEngine::LaneDir::Reverse;
        eng.advancePlayhead(1, nullptr);
        const int s = dotModular::STRAND_RHYTHM;
        std::snprintf(msg, sizeof msg, "G3: the reverse lane's tick at the parent's entry is %d, want 0 (the reverse walk starts at 0 — the rotation cell follows)", eng.laneTickV_[V2S.v][s]);
        CHECK(eng.laneTickV_[V2S.v][s] == 0, msg);
        const int cell = eng.getStrandIdx(eng.laneTickV_[V2S.v][s], 8, 0, 2);
        std::snprintf(msg, sizeof msg, "G3: the reverse lane's CELL is %d, want 2 (its rotation — one cell RIGHT of the old R-1)", cell);
        CHECK(cell == 2, msg);
    }

    // ── G4 (regression): Forward, Pendulum, PingPong still anchor at R ──────
    for (const auto& dd : {SequencerEngine::LaneDir::Forward,
                           SequencerEngine::LaneDir::Pendulum,
                           SequencerEngine::LaneDir::PingPong}) {
        for (const int R : {0, 5}) {
            SequencerEngine eng; eng.reset();
            eng.updateWindow(8.f, 0.f, false, 0.f, 0.f, false);
            eng.numPolyVoices = 2;
            eng.setVoiceLorLocalEast(V2S, true);
            eng.voiceLorRef(V2S, SequencerEngine::LOR_LEN) = 8;
            eng.voiceLorRef(V2S, SequencerEngine::LOR_ROT) = R;
            eng.voiceLorDir_[V2S.v] = dd;
            eng.advancePlayhead(1, nullptr);
            std::snprintf(msg, sizeof msg, "G4 (dir=%d, R=%d): the frame value at the entry is %d, want %d (UNCHANGED — the fix is Reverse-only)", (int)dd, R, (int)(eng.voiceDriver_[V2S.v] - 1), R);
            CHECK(eng.voiceDriver_[V2S.v] - 1 == R, msg);
        }
    }

    // ── G5 (regression, bounces byte-identical): the Pendulum and PingPong FULL
    // two-period LANE walks are bit-for-bit the doc's reference walks — the whole
    // walk dumped, not just t=1 — exercising the anchorBias=0 path and proving
    // the ten-site edit touched nothing the bounces consume. The rig: a MONO lane
    // (L=8) under the PASS-THROUGH global (G=16, off 0) so the lane's raw
    // congruence spans both periods (a cycle-mode frame read would re-anchor the
    // walk's leg every phrase). The fixture was captured on the PRE-FIX engine
    // and matches the doc's reference exactly; pre == post verified by dump.
    {
        const struct { SequencerEngine::LaneDir d; int walk[16]; const char* tag; } refs[] = {
            { SequencerEngine::LaneDir::Pendulum, {0,1,2,3,4,5,6,7,6,5,4,3,2,1,0,1}, "Pendulum" },
            { SequencerEngine::LaneDir::PingPong, {0,1,2,3,4,5,6,7,7,6,5,4,3,2,1,0}, "PingPong" },
        };
        const int S = dotModular::STRAND_RHYTHM;
        for (const auto& ref : refs) {
            SequencerEngine eng; eng.reset();
            eng.updateWindow(16.f, 0.f, false, 0.f, 0.f, false);   // the pass-through global
            eng.lorRef(S, SequencerEngine::LOR_LEN) = 8;
            eng.laneDir_[S] = ref.d;
            for (int t = 0; t < 16; ++t) {
                eng.advancePlayhead(1, nullptr);
                std::snprintf(msg, sizeof msg, "G5 (%s): step %d lane tick %d, want %d (the full two-period bounce walk, byte-identical pre/post)", ref.tag, t, eng.laneTick_[S], ref.walk[t]);
                CHECK(eng.laneTick_[S] == ref.walk[t], msg);
            }
        }
    }

    // ── G6 (regression, the sign law — the load-bearing one): the reverse
    // re-anchored read's CELL moves R-1 -> R while the per-step sign/edge cue
    // is IDENTICAL to the pre-fix sequence at EVERY step, the wrap included
    // (the fixture captured on the pre-fix engine: all -1 — the guard
    // substitution signMotion*dirSign is bias-independent). If a biased site
    // dropped its |dd|>1 guard, this row fails at the wrap step while the
    // cell-position assertions still pass.
    {
        const struct { int R; int cells[16]; } fixtures[] = {
            { 0, {0,7,6,5,4,3,2,1,0,7,6,5,4,3,2,1} },
            { 3, {3,2,1,0,7,6,5,4,3,2,1,0,7,6,5,4} },
        };
        for (const auto& fx : fixtures) {
            SequencerEngine eng; eng.reset();
            eng.updateWindow(8.f, 0.f, false, 0.f, 0.f, false);
            eng.numPolyVoices = 2;
            eng.setVoiceLorLocalEast(V2S, true);
            eng.voiceLorRef(V2S, SequencerEngine::LOR_LEN) = 8;
            eng.voiceLorRef(V2S, SequencerEngine::LOR_ROT) = fx.R;
            eng.voiceLorDir_[V2S.v] = SequencerEngine::LaneDir::Reverse;
            for (int t = 0; t < 16; ++t) {
                eng.advancePlayhead(1, nullptr);
                std::snprintf(msg, sizeof msg, "G6 (R=%d): step %d frame value %d, want %d (the cell moved R-1 -> R)", fx.R, t, (int)(eng.voiceDriver_[V2S.v] - 1), fx.cells[t]);
                CHECK(eng.voiceDriver_[V2S.v] - 1 == fx.cells[t], msg);
                std::snprintf(msg, sizeof msg, "G6 (R=%d): step %d the sign cue is %d, want -1 (IDENTICAL to the pre-fix fixture — the wrap step included; a dropped |dd|>1 guard fails HERE)", fx.R, t, eng.voiceSign_[V2S.v]);
                CHECK(eng.voiceSign_[V2S.v] == -1, msg);
            }
        }
    }

    if (g_fail == 0) std::printf("all green\n");
    else std::printf("%d FAILED\n", g_fail);
    return g_fail ? 1 : 0;
}
