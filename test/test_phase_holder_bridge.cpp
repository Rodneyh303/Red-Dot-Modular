// test_phase_holder_bridge.cpp — CA5 (CC_PROMPT_REVERSE_COMPLETIONS_THEN_CA5 Part 2):
// the PhaseHolder extraction's BYTE-IDENTICAL bridge. The "maintain a walk PHASE
// congruent to a driver coordinate, re-seed when the walk KEY changes" lifecycle —
// five inline copies in tickHolders + the anchorTransport re-rolls — is ONE law now
// (PositionResolver::PhaseHolder). THREE gates here:
//
// 1. THE CLOSED-FORM GRID — the moved law vs a VERBATIM copy of the shipped inline
//    ternary, swept over (phase x storedKey x key x t0 x k x period), including the
//    key-change arm, negative t0/k, and period 1. The moved code equals the shipped
//    code in every cell.
// 2. THE ENGINE-MARCH CONGRUENCE — the skeleton's CONTRACT on the real engine: every
//    holder's phase stays ≡ its driver's t0 (mod its CURRENT period) at every step of
//    marches covering the doc's configurations: forward, the RETRACE (a mid-phrase
//    rewind — advancePlayhead(-1)), the STOP (the offsets-only refresh, k=0), the
//    ANCHOR (anchorTransport — the re-seed form), and the FREE-RUN (a global of 10
//    with a shorter lane — the 2026-11-21 carry-over's configuration).
// 3. THE RESOLVER CHARACTERIZATION (the doc's untested list): the host transport's
//    stop / rewind / phase-snap effects on entryDir_ / the read coordinate — the
//    TRUE-RETRACE convention pinned as it ships (a stop moves nothing; a rewind
//    walks the SAME coordinate backwards; a snap feeds its own raw phrase). This is
//    a characterization pin, not a change: the doc's cleaner read = f(t − t0, config)
//    fold is NOT byte-identical under reversal (the retrace is path-dependent BY
//    DESIGN — the TRUE-RETRACE ruling), so the entryDir_ model stays and is pinned.
//
// Compile: see test/run_all.sh ("test_phase_holder_bridge").
#include <cstdio>
#include <cstdint>
#include "engines/SequencerEngine.hpp"
#include "engines/PositionResolver.hpp"
#include "VoiceResolver.hpp"

using LaneDir = SequencerEngine::LaneDir;
using dotModular::VoiceResolver;

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("  FAIL: %s\n", (m)); ++g_fail; } } while (0)

// The VERBATIM shipped arithmetic (the pre-extraction inline ternary's helper).
static int oldWrap(int64_t v, int m) { int64_t r = v % m; if (r < 0) r += m; return (int)r; }

int main() {
    std::printf("test_phase_holder_bridge: the CA5 PhaseHolder extraction is byte-identical (the closed-form grid + the engine-march congruence + the resolver characterization)\n");
    char msg[256];

    // ── 1. THE CLOSED-FORM GRID ──
    {
        long cells = 0;
        const int keys[] = { -1, 0, 7, 40 };
        for (int phase = 0; phase <= 33; ++phase)
            for (int sk : keys)
                for (int nk : { 0, 7, 40 })
                    for (int64_t t0 : { (int64_t)-5, (int64_t)-1, (int64_t)0, (int64_t)1, (int64_t)13, (int64_t)30 })
                        for (int64_t k : { (int64_t)-1, (int64_t)0, (int64_t)1, (int64_t)2 })
                            for (int period = 1; period <= 36; ++period) {
                                int keyA = sk, keyB = sk;
                                const int a = PositionResolver::PhaseHolder::advance(keyA, phase, nk, t0, k, period);
                                const int b = (keyB != nk) ? oldWrap(t0, period) : oldWrap((int64_t)phase + k, period);
                                keyB = nk;
                                ++cells;
                                if (a != b || keyA != keyB) {
                                    std::snprintf(msg, sizeof msg, "advance grid: phase=%d sk=%d nk=%d t0=%lld k=%lld per=%d -> %d/%d key %d/%d",
                                                  phase, sk, nk, (long long)t0, (long long)k, period, a, b, keyA, keyB);
                                    CHECK(false, msg);
                                }
                                int keyC = sk;
                                const int c = PositionResolver::PhaseHolder::reseed(keyC, nk, t0, period);
                                ++cells;
                                if (c != oldWrap(t0, period) || keyC != nk) {
                                    std::snprintf(msg, sizeof msg, "reseed grid: t0=%lld per=%d -> %d", (long long)t0, period, c);
                                    CHECK(false, msg);
                                }
                            }
        std::snprintf(msg, sizeof msg, "the closed-form grid swept %ld cells (both laws, both arms)", cells);
        CHECK(cells > 100000, msg);
        std::printf("  grid: %ld cells, all identical\n", cells);
    }

    // ── 2. THE ENGINE-MARCH CONGRUENCE ──
    // The rig: V1's row authored (mapped, so the frame maintenance runs), the REST
    // lane authored (a different length, so the lane periods differ from the frame's).
    auto rigEngine = [](SequencerEngine& eng, float gLen) {
        eng.reset();
        eng.updateWindow(gLen, 0.f, false, 0.f, 0.f, false);
        eng.setVoiceLorLocalEast(dotModular::kMonoVoiceSlot, true);
        eng.voiceLorRef(dotModular::kMonoVoiceSlot, SequencerEngine::LOR_LEN) = 8;
        eng.voiceLorRef(dotModular::kMonoVoiceSlot, SequencerEngine::LOR_OFF) = 0;
        eng.voiceLorRef(dotModular::kMonoVoiceSlot, SequencerEngine::LOR_ROT) = 2;
        eng.lorRef(dotModular::STRAND_RHYTHM, SequencerEngine::LOR_LEN) = 6;
        eng.lorRef(dotModular::STRAND_RHYTHM, SequencerEngine::LOR_OFF) = 0;
        eng.lorRef(dotModular::STRAND_RHYTHM, SequencerEngine::LOR_ROT) = 1;
    };
    auto checkCongruence = [&](SequencerEngine& eng, const char* what) {
        const int64_t t0Now = (int64_t)eng.transportTick_ - 1;
        for (int s = 0; s < dotModular::NUM_STRANDS; ++s) {
            int lm, lom, rm; LaneDir dm;
            eng.effectiveMonoLaneLor(s, lm, lom, rm, dm);
            const int len = std::max(1, lm);
            const int per = SequencerEngine::walkPeriod(dm, len);
            std::snprintf(msg, sizeof msg, "%s: mono lane %d phase %d is not congruent to t0 %lld mod %d", what, s, eng.lanePhase_[s], (long long)t0Now, per);
            CHECK(oldWrap((int64_t)eng.lanePhase_[s] - t0Now, per) == 0, msg);
        }
        {
            int L, O, R; LaneDir vd;
            eng.effectiveVoiceLor(0, L, O, R, vd);
            const bool identity = (L == dotModular::kPatternLengthCap && O == 0 && R == 0 && vd == LaneDir::Forward);
            if (!identity) {   // the maintenance runs only on mapped rows
                if (L < 1) L = 1;
                const int per = SequencerEngine::walkPeriod(vd, L);
                std::snprintf(msg, sizeof msg, "%s: V1 frame phase %d is not congruent to t0 %lld mod %d", what, eng.voiceFramePhase_[0], (long long)t0Now, per);
                CHECK(oldWrap((int64_t)eng.voiceFramePhase_[0] - t0Now, per) == 0, msg);
            }
        }
        {
            int rl, ro, rr; LaneDir rd;
            eng.effectiveMonoLaneLor(dotModular::STRAND_RHYTHM, rl, ro, rr, rd);
            const int rLen = std::max(1, rl);
            std::snprintf(msg, sizeof msg, "%s: globalT0 phase %d is not congruent to t0 %lld mod %d", what, eng.globalT0Phase_, (long long)t0Now, rLen);
            CHECK(oldWrap((int64_t)eng.globalT0Phase_ - t0Now, rLen) == 0, msg);
        }
    };
    {
        SequencerEngine eng;
        rigEngine(eng, 16.f);
        for (int i = 0; i < 6; ++i) { eng.advancePlayhead(1, nullptr); checkCongruence(eng, "forward"); }
        for (int i = 0; i < 3; ++i) { eng.advancePlayhead(-1, nullptr); checkCongruence(eng, "retrace/rewind"); }
        eng.recomputeLaneTicks(nullptr);            // the STOP: the offsets-only refresh (k = 0)
        checkCongruence(eng, "stop");
        eng.anchorTransport(1);                      // the ANCHOR: the re-seed form
        checkCongruence(eng, "anchor");
        for (int i = 0; i < 4; ++i) { eng.advancePlayhead(1, nullptr); checkCongruence(eng, "post-anchor forward"); }
    }
    {   // the FREE-RUN configuration (a global of 10, a lane of 6 — the carry-over's rig)
        SequencerEngine eng;
        rigEngine(eng, 10.f);
        for (int i = 0; i < 9; ++i) { eng.advancePlayhead(1, nullptr); checkCongruence(eng, "free-run G=10"); }
    }

    // ── 3. THE RESOLVER CHARACTERIZATION (the doc's untested list) ──
    {
        PositionResolver p;
        p.reset();
        p.setWindow(8, 0);
        for (int i = 0; i < 4; ++i) p.advance(+1);
        CHECK(p.readCoord() == 3, "readCoord after 4 forward steps = 3");
        p.advance(-1);   // the REWIND: the SAME coordinate walked backwards (the retrace)
        CHECK(p.readCoord() == 2, "readCoord retraces to 2 after one rewind");
        p.advance(-1);
        CHECK(p.readCoord() == 1, "readCoord retraces to 1 after two rewinds");
        const int colAtStop = p.playedColumn(), coordAtStop = (int)p.readCoord();
        p.advance(0);    // the STOP: nothing moves
        CHECK(p.playedColumn() == colAtStop && (int)p.readCoord() == coordAtStop, "a stop moves neither the column nor the read coordinate");
        p.snapTo(5, +1, 3);   // the PHASE SNAP: the column + its own raw phrase
        CHECK(p.playedColumn() == 5 && p.phrasePos() == 3 && p.readCoord() == 3, "the snap sets the column and feeds its own raw phrase (dir +1)");
        p.setWindow(8, 2);   // a REAL window change re-anchors the read coordinate (entryDir_ = 0)
        // col 5 is INSIDE the new window [2,10), so it stays (the re-anchor parks only
        // out-of-window columns) — the next step is col 6, readCoord 4 in the new window.
        p.advance(+1);
        CHECK(p.readCoord() == 4, "an in-window column survives the window change; readCoord is its new-window offset (4)");
    }

    if (g_fail == 0) std::printf("all green\n");
    else std::printf("%d FAILED\n", g_fail);
    return g_fail ? 1 : 0;
}
