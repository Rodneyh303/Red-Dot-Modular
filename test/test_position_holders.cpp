// THE POSITION-HOLDER MODEL (POSITION_HOLDER_MODEL.md) — the walk-engine pins.
//
// The global counter totalStepsElapsed is DEAD as the master timeline: every walk owns a
// bounded PHASE (a position in its OWN cycle, congruent to its raw walk coordinate mod
// the walk's OWN period), advanced LOCALLY on each advance event. These pins hold the
// model to its contract:
//   1. MARCH EQUIVALENCE — with any fixed parameters, the holder positions equal the old
//      f(counter) closed forms (laneTickFor at the raw tick) at EVERY step, for all four
//      walk shapes, non-divisive lengths (7/5/6 — the case a bounded driver would break),
//      the mono AND the per-voice tiers.
//   2. REVERSIBILITY — a backward march retraces the forward march bit-exactly (the
//      bounce walks are time-symmetric; the phase unwinds the same congruence).
//   3. THE RE-ANCHOR LAW — a mid-run length change re-bounds the walk to f(t, newLen)
//      EXACTLY (the walk-key shadow re-anchors from the transport's own position).
//   4. THE WARM-PHASE LAW — a pass-through → mapped → pass-through round-trip keeps
//      every read == f(t, config) at every step (the raw-clock congruences survive any
//      config; the mapped reads take the bounded walk representatives).
//   5. THE ANCHOR LAW — reset()/snapToPhaseStep/resetLaneWalk() seed the congruences
//      from the transport's own position; the first advance lands on f(1) everywhere.
//   6. THE OFFSET DIFF — an advanceOffset_ jump outside a fold (a hand seed, an
//      activation zeroing) is absorbed by the voice's holders exactly as the old
//      f(t + offset) read saw it.
//   7. THE WRITE-SITE LAW — the transport's own position advances ±1 with the events,
//      is left verbatim by the offsets-only refresh, and no read depends on it between
//      anchors (the holders carried the march across a transport-tick... nothing —
//      pinned as: the march equivalence above never consults it).
//
// Build (run_all.sh): test_position_holders|$SE $GS $PE $CE
#include <cstdio>
#include <cstdint>
#include <vector>
#include <algorithm>

#include "SequencerEngine.hpp"
#include "VoiceResolver.hpp"

static int passed = 0, failed = 0;
static void check(bool ok, const char* what) {
    if (ok) ++passed; else { ++failed; std::printf("  FAIL: %s\n", what); }
}

using E = SequencerEngine;
namespace dm = dotModular;

// The old-model reference: the closed form at the raw tick (the f(counter) law), with
// the recomp's local wrap — what every lane cache read under the counter model.
static int fOf(const E& e, int s, int64_t tick) {
    const int len = std::max(1, e.strandLen(s));
    return (int)(((E::laneTickFor(e.laneDir_[s], tick, len) % len) + len) % len);
}
static int fOfV(const E& e, int v, int s, int64_t tEff) {
    const int len = std::max(1, e.polyStrandLenOf(v, s));
    return (int)(((E::laneTickFor(e.laneDirV_[v][s], tEff, len) % len) + len) % len);
}

static void authorNonDiv(E& e) {
    e.lorRef(dm::STRAND_RHYTHM, E::LOR_LEN) = 7;
    e.lorRef(dm::STRAND_MELODY, E::LOR_LEN) = 5;
    e.laneDir_[dm::STRAND_MELODY] = E::LaneDir::Pendulum;
    e.laneDirPending_[dm::STRAND_MELODY] = E::LaneDir::Pendulum;
    e.lorRef(dm::STRAND_OCTAVE, E::LOR_LEN) = 6;
    e.laneDir_[dm::STRAND_OCTAVE] = E::LaneDir::PingPong;
    e.laneDirPending_[dm::STRAND_OCTAVE] = E::LaneDir::PingPong;
    e.lorRef(dm::STRAND_ACCENT, E::LOR_LEN) = 7;
    e.laneDir_[dm::STRAND_ACCENT] = E::LaneDir::Reverse;
    e.laneDirPending_[dm::STRAND_ACCENT] = E::LaneDir::Reverse;
    // per-voice: V2 (bank 1) rhythm len 7 pendulum, V3 (bank 2) VAR len 5 reverse
    e.polyLenERef(dm::VoiceResolver::slot(2), dm::LaneId{E::PL_REST}) = 7;
    e.laneDirV_[1][dm::STRAND_RHYTHM] = E::LaneDir::Pendulum;
    e.laneDirVPending_[1][dm::STRAND_RHYTHM] = E::LaneDir::Pendulum;
    e.polyLORRef(dm::VoiceResolver::slot(3), dm::LaneId{E::EDITOR_LANE_VARIATION}, E::LOR_LEN) = 5;
    e.laneDirV_[2][dm::STRAND_VARIATION] = E::LaneDir::Reverse;
    e.laneDirVPending_[2][dm::STRAND_VARIATION] = E::LaneDir::Reverse;
}

int main() {
// ── 1+2. march equivalence + reversibility ──────────────────────────────────────
{
    E e; e.reset();
    e.numPolyVoices = 3;
    authorNonDiv(e);
    std::vector<std::vector<int>> fwd;   // per step: the watched reads
    const int64_t N = 96;
    for (int64_t i = 1; i <= N; ++i) {
        e.advancePlayhead();
        std::vector<int> r = { fOf(e, dm::STRAND_RHYTHM, i),  e.laneTick_[dm::STRAND_RHYTHM],
                               fOf(e, dm::STRAND_MELODY, i),  e.laneTick_[dm::STRAND_MELODY],
                               fOf(e, dm::STRAND_OCTAVE, i),  e.laneTick_[dm::STRAND_OCTAVE],
                               fOf(e, dm::STRAND_ACCENT, i),  e.laneTick_[dm::STRAND_ACCENT],
                               fOfV(e, 1, dm::STRAND_RHYTHM, i),      e.laneTickV_[1][dm::STRAND_RHYTHM],
                               fOfV(e, 2, dm::STRAND_VARIATION, i),   e.laneTickV_[2][dm::STRAND_VARIATION] };
        fwd.push_back(r);
    }
    bool marchExact = true;
    for (size_t k = 0; k < fwd.size(); ++k)
        for (size_t j = 0; j + 1 < fwd[k].size(); j += 2)
            marchExact = marchExact && (fwd[k][j] == fwd[k][j + 1]);
    check(marchExact, "MARCH: all four shapes + non-div 7/5/6, mono and per-voice, == f(t) at every step");

    bool retrace = true;
    for (int64_t j = 0; j < N - 1; ++j) {
        e.advancePlayhead(-1);
        const int64_t t = N - 1 - j;   // the raw tick this backward step lands on
        retrace = retrace && e.laneTick_[dm::STRAND_RHYTHM]  == fwd[t - 1][1]
                       && e.laneTick_[dm::STRAND_MELODY] == fwd[t - 1][3]
                       && e.laneTick_[dm::STRAND_OCTAVE] == fwd[t - 1][5]
                       && e.laneTick_[dm::STRAND_ACCENT] == fwd[t - 1][7]
                       && e.laneTickV_[1][dm::STRAND_RHYTHM] == fwd[t - 1][9]
                       && e.laneTickV_[2][dm::STRAND_VARIATION] == fwd[t - 1][11];
    }
    check(retrace, "REVERSIBILITY: backward retraces the forward march bit-exactly (bouncers included)");
}

// ── 3. the re-anchor law (a mid-run length change == f(t, newLen)) ─────────────
{
    E e; e.reset();
    e.numPolyVoices = 1;
    e.lorRef(dm::STRAND_RHYTHM, E::LOR_LEN) = 7;
    for (int64_t i = 1; i <= 20; ++i) e.advancePlayhead();
    e.lorRef(dm::STRAND_RHYTHM, E::LOR_LEN) = 5;   // mid-run re-author
    bool exact = true;
    for (int64_t i = 21; i <= 40; ++i) {
        e.advancePlayhead();
        exact = exact && (e.laneTick_[dm::STRAND_RHYTHM] == fOf(e, dm::STRAND_RHYTHM, i));
    }
    check(exact, "RE-ANCHOR: a mid-run len change re-bounds the walk to f(t, newLen) exactly");

    // a mid-run DIRECTION flip re-anchors the same way (the DirCell promotion path)
    E e2; e2.reset();
    e2.numPolyVoices = 1;
    e2.lorRef(dm::STRAND_RHYTHM, E::LOR_LEN) = 7;
    e2.laneDir_[dm::STRAND_RHYTHM] = E::LaneDir::Forward;
    for (int64_t i = 1; i <= 11; ++i) e2.advancePlayhead();
    e2.laneDir_[dm::STRAND_RHYTHM] = E::LaneDir::Pendulum;
    e2.laneDirPending_[dm::STRAND_RHYTHM] = E::LaneDir::Pendulum;
    bool flipExact = true;
    for (int64_t i = 12; i <= 30; ++i) {
        e2.advancePlayhead();
        flipExact = flipExact && (e2.laneTick_[dm::STRAND_RHYTHM] == fOf(e2, dm::STRAND_RHYTHM, i));
    }
    check(flipExact, "RE-ANCHOR: a mid-run dir flip walks the new shape from f(t, newDir) exactly");
}

// ── 4. the warm-phase law (pass-through → mapped → pass-through) ───────────────
{
    E e; e.reset();
    e.numPolyVoices = 2;
    e.lorRef(dm::STRAND_RHYTHM, E::LOR_LEN) = 7;   // non-div: the phase-carried case
    bool exact = true;
    for (int64_t i = 1; i <= 12; ++i) {            // pass-through
        e.advancePlayhead();
        exact = exact && (e.laneTick_[dm::STRAND_RHYTHM] == fOf(e, dm::STRAND_RHYTHM, i));
    }
    e.setWindow(10, 4);                            // → mapped
    for (int64_t i = 13; i <= 30; ++i) {
        e.advancePlayhead();
        const int walk = (int)(((i - 1) % 10 + 10) % 10) + 4;   // the mapped read law
        exact = exact && (e.laneTick_[dm::STRAND_RHYTHM] == (walk % 7 + 7) % 7);
    }
    e.setWindow(16, 0);                            // → back to pass-through
    for (int64_t i = 31; i <= 60; ++i) {
        e.advancePlayhead();
        exact = exact && (e.laneTick_[dm::STRAND_RHYTHM] == fOf(e, dm::STRAND_RHYTHM, i));
    }
    check(exact, "WARM-PHASE: pass-through → mapped → pass-through, every read == f(t, config)");
}

// ── 5. the anchor law (reset / phase snap / hard reset) ────────────────────────
{
    E e; e.reset();
    e.numPolyVoices = 2;
    e.lorRef(dm::STRAND_RHYTHM, E::LOR_LEN) = 7;
    e.advancePlayhead();
    check(e.laneTick_[dm::STRAND_RHYTHM] == 0 && e.laneTick_[dm::STRAND_MELODY] == 0
          && e.laneTickV_[1][dm::STRAND_VARIATION] == 0,
          "ANCHOR: the first advance after reset lands on f(1) everywhere");

    // the Mode E phase snap: anchor at an absolute tick, the walks take its congruence
    e.advancePlayhead();  e.advancePlayhead();  e.advancePlayhead();   // t = 4
    const int rhyBefore = e.laneTick_[dm::STRAND_RHYTHM];
    e.snapToPhaseStep(47 * 24, 24);                // absStep = 47 → tick 48
    check(e.laneTick_[dm::STRAND_RHYTHM] == fOf(e, dm::STRAND_RHYTHM, 48),
          "ANCHOR: snapToPhaseStep seeds the walks from the phase-derived tick exactly");
    check(rhyBefore == fOf(e, dm::STRAND_RHYTHM, 4), "ANCHOR: the pre-snap march was on f(t)");

    // Monsoon's hard reset = resetLaneWalk(): re-anchor to Beat 1, the next advance == f(1)
    for (int i = 0; i < 9; ++i) e.advancePlayhead();
    e.resetLaneWalk();
    e.advancePlayhead();
    check(e.laneTick_[dm::STRAND_RHYTHM] == 0 && e.laneTick_[dm::STRAND_ACCENT] == 0,
          "ANCHOR: resetLaneWalk (the Beat-1 hard reset) re-anchors every walk");
}

// ── 6. the offset diff (a jump outside a fold is absorbed exactly) ─────────────
{
    E a, b; a.reset(); b.reset();
    a.numPolyVoices = 2; b.numPolyVoices = 2;
    for (int i = 0; i < 10; ++i) { a.advancePlayhead(); b.advancePlayhead(); }
    b.advanceOffset_[1] += 5;   // a hand seed OUTSIDE any fold (the activation/seed shape)
    a.advancePlayhead();
    b.advancePlayhead();
    const int dVar = ((b.laneTickV_[1][dm::STRAND_VARIATION]
                     - a.laneTickV_[1][dm::STRAND_VARIATION]) % 16 + 16) % 16;
    check(dVar == 5, "OFFSET DIFF: a hand-seeded offset shifts that voice's reads by exactly 5 (mod len)");
    bool monoSame = true;
    for (int s = 0; s < dm::NUM_STRANDS; ++s) monoSame = monoSame && (a.laneTick_[s] == b.laneTick_[s]);
    check(monoSame, "OFFSET DIFF: the mono lanes are untouched (slot 0 IS the transport)");

    // the activation zeroing: a diverged voice re-enters at the macro position
    b.advanceOffset_[1] = 0;   // what voice activation does
    a.advancePlayhead();
    b.advancePlayhead();
    const int dBack = ((b.laneTickV_[1][dm::STRAND_VARIATION]
                      - a.laneTickV_[1][dm::STRAND_VARIATION]) % 16 + 16) % 16;
    check(dBack == 0, "OFFSET DIFF: the activation zeroing re-unifies the voice at the macro position");
}

// ── 7. the write-site law (the transport's own position) ───────────────────────
{
    E e; e.reset();
    e.numPolyVoices = 1;
    e.advancePlayhead();
    check(e.transportTick_ == 1, "WRITE SITE: the advance event moves the transport's own position +1");
    e.advancePlayhead(-1);
    check(e.transportTick_ == 0, "WRITE SITE: a backward advance moves it −1 (the net-step law)");
    const int64_t before = e.transportTick_;
    e.advanceOffset_[1] += 3;
    e.recomputeLaneTicks();     // the offsets-only refresh: the clock held
    check(e.transportTick_ == before, "WRITE SITE: the offsets-only refresh leaves the transport verbatim");
    // (CC_PROMPT_16_TRIAGE option C re-aim) the transport default len is the cap.
    check(e.globalWalk_ == (int)(((before - 1) % dm::kPatternLengthCap + dm::kPatternLengthCap) % dm::kPatternLengthCap),
          "WRITE SITE: the global walk holder reads (tick−1) mod len — nothing else consumes the tick");
}

std::printf("%d passed, %d failed\n", passed, failed);
return failed ? 1 : 0;
}
