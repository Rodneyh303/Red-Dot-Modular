// test_bug6_commit_wrap_guard.cpp — REGRESSION GUARD for the BUG 6 commit edge.
//
// Finding (docs/design/CC_FIX_BUG6_COMMIT_EDGE_FINDING.md): the probability-lane
// offset-CV commit lands EXACTLY at the true voice-frame wrap — once per pushed CV —
// under static period, global-range modulation (windowed AND full-phrase voices),
// and voice-own-length modulation. Reproduce-first found NO off-wrap scatter; this
// pins that so a future edge change can't silently re-break it.
//
// The TRUE wrap is the cell the READS consume — laneTickFor evaluated at the
// read's own fold input (NOT the bare phase field), the same reference P12 uses.
// (CC_FIX_GLOBAL_BELOW_VOICE_PHASE_DESYNC, 2026-11-18) the reference is RE-BASED
// on the live rotation anchor: a CYCLE row's reads fold the transport-signed
// phrase coordinate (wrapMod(sPosV + advanceOffset, lenG)), an ABS1 row's the
// current-global-phrase ring on the raw congruence — the raw voiceFramePhase_
// reference this guard used first diverges from the reads exactly under a
// voice-length mod (scene D), which is what the RESOLVED spec retires.
//
// PASSES on the current engine (this is a guard, not a reproduce-first failure).
#include <cstdio>
#include <vector>
#include "engines/SequencerEngine.hpp"
#include "VoiceResolver.hpp"

using dotModular::VoiceResolver;
using LaneId = dotModular::LaneId;
static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("  FAIL: %s\n", m); ++g_fail; } } while (0)

static dotModular::VoiceSlot v2() { return VoiceResolver::slot(2); }
static constexpr int STR = dotModular::STRAND_RHYTHM;

static int committedLaneO(SequencerEngine& eng) {
    int L, O, R; SequencerEngine::LaneDir d;
    eng.effectiveLaneLor(v2(), LaneId{SequencerEngine::PL_REST}, L, O, R, d);
    return O;
}
static int trueCell(SequencerEngine& eng) {
    int rL, rO, rR; SequencerEngine::LaneDir rd;
    eng.effectiveVoiceLor(v2().v, rL, rO, rR, rd);
    const int lenG = eng.cachedLength > 0 ? eng.cachedLength : 1;
    if (eng.voiceFrameReadsFullPhrase(v2().v)) {
        // ABS1: the current-global-phrase ring on the raw congruence (the read's law).
        const int64_t t0Eff = eng.pos_.phaseClock(eng.transportTick_) + eng.advanceOffset_[v2().v];
        const int64_t ringPos = ((t0Eff % lenG) + lenG) % lenG;
        return (int)SequencerEngine::laneTickFor(rd, ringPos + 1, lenG);
    }
    // CYCLE: the rotation anchor's phrase coordinate — the read's own fold input.
    const int64_t sPosV = (eng.pos_.direction() > 0) ? (int64_t)eng.pos_.phrasePos()
                                                      : -(int64_t)eng.pos_.phrasePos();
    const int64_t vCoord = ((sPosV + eng.advanceOffset_[v2().v]) % lenG + lenG) % lenG;
    return (int)SequencerEngine::laneTickFor(rd, vCoord + 1, rL);
}

// Drive `ticks` steps; at t==1 push an offset CV (+2). Returns (commits, offWrap)
// where commits = number of steps where committed O changed, offWrap = number of
// those that were NOT at a true voice-frame wrap.
struct Tally { int commits = 0; int offWrap = 0; };
template <class Setup, class PerTick>
static Tally run_scene(Setup setup, PerTick perTick, int ticks) {
    SequencerEngine eng; eng.reset();
    eng.updateWindow(8.f, 0.f, false, 0.f, 0.f, false);
    eng.numPolyVoices = 2;
    eng.setVoiceLorLocalEast(v2(), true);
    eng.voiceLorRef(v2(), SequencerEngine::LOR_LEN) = 8;
    eng.polyLORRef(v2(), LaneId{SequencerEngine::PL_REST}, SequencerEngine::LOR_LEN) = 3;
    setup(eng);
    Tally tal; int prevO = committedLaneO(eng);
    for (int t = 0; t < ticks; ++t) {
        perTick(eng, t);
        eng.advancePlayhead(1, nullptr);
        if (t == 1) { int d3[3] = {0, 2, 0}; eng.pushLaneLorCvPoly(v2(), SequencerEngine::PL_REST, d3); }
        const int O = committedLaneO(eng);
        if (O != prevO) { ++tal.commits; if (trueCell(eng) != 0) ++tal.offWrap; }
        prevO = O;
    }
    return tal;
}

int main() {
    std::printf("== test_bug6_commit_wrap_guard (lane CV commits ONLY at the true voice-frame wrap) ==\n");
    auto noop = [](SequencerEngine&){};
    auto noTick = [](SequencerEngine&, int){};

    // Scene A: static period.
    {
        Tally a = run_scene(noop, noTick, 32);
        CHECK(a.commits == 1, "A: the single offset CV commits exactly once");
        CHECK(a.offWrap == 0, "A: the commit is at the true voice-frame wrap (not off-wrap)");
    }
    // Scene B: global length modulated 8<->7, windowed voice (period fixed at 8).
    {
        auto flip = [](SequencerEngine& eng, int t){
            if (t > 0 && (t % 8) == 0) {
                static thread_local float g = 8.f; g = (g == 8.f) ? 7.f : 8.f;
                eng.updateWindow(g, 0.f, false, 0.f, 0.f, false);
            }
        };
        Tally b = run_scene(noop, flip, 40);
        CHECK(b.commits == 1,  "B: global mod — the offset CV still commits exactly once");
        CHECK(b.offWrap == 0,  "B: global mod — the commit stays at the true wrap (global range never moves the windowed period)");
    }
    // Scene C: MASK_DYNAMIC full-phrase voice, global length modulated 8<->6.
    {
        auto setup = [](SequencerEngine& eng){
            eng.maskLocalEast_[v2().v] = true;
            eng.maskKind_[v2().v] = SequencerEngine::MASK_DYNAMIC;   // voiceFrameReadsFullPhrase=true
        };
        auto flip = [](SequencerEngine& eng, int t){
            if (t > 0 && (t % 8) == 0) {
                static thread_local float g = 8.f; g = (g == 8.f) ? 6.f : 8.f;
                eng.updateWindow(g, 0.f, false, 0.f, 0.f, false);
            }
        };
        Tally c = run_scene(setup, flip, 40);
        CHECK(c.commits == 1, "C: full-phrase — the offset CV commits exactly once (on the current-global-phrase ring)");
        CHECK(c.offWrap == 0, "C: full-phrase — the commit is at the true ring wrap (the ring follows the live global phrase)");
    }
    // Scene D: the voice's OWN length CV'd 8<->5 — the period genuinely moves; the
    // commit must still land on a true wrap of the CURRENT period, never mid-travel.
    {
        auto lenMod = [](SequencerEngine& eng, int t){
            if (t > 0 && (t % 8) == 0) {
                static thread_local int vl = 8; vl = (vl == 8) ? 5 : 8;
                std::vector<int> m3 = {0,0,0}; std::vector<int> east(16*3, 0);
                east[v2().v * 3 + SequencerEngine::LOR_LEN] = vl - 8;
                eng.pushVoiceLorCv(m3.data(), east.data());
            }
        };
        Tally d = run_scene(noop, lenMod, 40);
        CHECK(d.commits == 1, "D: voice-length mod — the offset CV commits exactly once");
        CHECK(d.offWrap == 0, "D: voice-length mod — the commit lands on the CURRENT period's true wrap, never mid-travel");
    }

    std::printf(g_fail ? "FAILED (%d)\n" : "all green\n", g_fail);
    return g_fail ? 1 : 0;
}
