// test_engine_reverse_roundtrip — CLAIM: "time-reversible: scrub / reverse retraces the SAME states".
// (docs/design/CLAIMS_EVIDENCE.md A1.)  Drive the REAL SequencerEngine forward N steps, then reverse the
// TRANSPORT N steps: the state observed after reverse step k must equal the state observed after the
// matching forward step (the step BEFORE it) — playhead column, the voice READ, and EVERY lane's tick of
// the voice — for each voice-lane direction (Forward/Reverse/Pendulum/PingPong), with polymetric lane
// lengths, offsets and rotations, in CYCLE and ABSOLUTE (abs1) modes.
// HISTORY (2026-11-21): this test first MEASURED a gap — with a voice window the voice READ and per-lane ticks
// came back one step ahead (8960 slips over the sweep) because the read coordinate re-anchored to the NEW
// direction's phrase entry on a mid-phrase reversal. Rodney ruled TRUE RETRACE: PositionResolver::readCoord is
// anchored to the traversal's ENTRY direction, so reversing mid-phrase walks the SAME coordinate backwards.
// A reverse that starts FRESH at the phrase entry still enters at R (readCoord == the old signed phrasePos).
// Everything below is now ASSERTED (column, every lane tick, and the voice read), voice window or not.
// This pins the POSITION spine of reversibility; the draw values themselves are counter-addressed
// (test_PhiloxRng, test_ca_true_reverse).
#include <cstdio>
#include <vector>
#include <array>
#include "engines/SequencerEngine.hpp"
#include "VoiceResolver.hpp"
using dotModular::VoiceResolver; using LaneId = dotModular::LaneId;
using LD = SequencerEngine::LaneDir;

static int g_fail = 0, g_cases = 0;
#define CHECK(c, m) do { if (!(c)) { if (g_fail < 40) std::printf("  FAIL: %s\n", (m)); ++g_fail; } } while (0)

struct State { int col, read; std::array<int, dotModular::NUM_STRANDS> lane; };

static State snap(SequencerEngine& e, int v) {
    State s; s.col = e.pos_.playedColumn(); s.read = e.voiceRead_[v];
    for (int l = 0; l < dotModular::NUM_STRANDS; ++l) s.lane[l] = e.laneTickV_[v][l];
    return s;
}
static bool same(const State& a, const State& b) { return a.col == b.col && a.read == b.read && a.lane == b.lane; }

static void run(int G, int O, int L, int R, LD dir, bool abs1, int lenMel, int lenRest, const char* name, bool voiceWindow) {
    SequencerEngine e; e.reset(); e.numPolyVoices = 2;
    const auto v = VoiceResolver::slot(2);
    e.polyLenERef(v, LaneId{SequencerEngine::PL_MELODY}) = lenMel;
    e.polyLenERef(v, LaneId{SequencerEngine::PL_REST})   = lenRest;
    if (voiceWindow) {
        e.voiceLorRef(v, SequencerEngine::LOR_LEN) = L;
        e.voiceLorRef(v, SequencerEngine::LOR_OFF) = O;
        e.voiceLorRef(v, SequencerEngine::LOR_ROT) = R;
        e.setVoiceLorLocalEast(v, true);
        e.voiceLorDir_[v.v] = dir;
        e.maskLocalEast_[v.v] = true;
        e.maskKind_[v.v] = abs1 ? SequencerEngine::MASK_DYNAMIC : SequencerEngine::MASK_CYCLE;
    }
    e.updateWindow((float)G, 0.f, false, 0.f, 0.f, false);

    const int N = 3 * G + 7;
    std::vector<State> fwd;
    for (int t = 0; t < N; ++t) { e.advancePlayhead(1, nullptr); fwd.push_back(snap(e, v.v)); }
    ++g_cases;
    for (int t = N - 1; t >= 1; --t) {            // reverse step from fwd[t] lands on fwd[t-1]
        e.advancePlayhead(-1, nullptr);
        State r = snap(e, v.v);
        const State& f = fwd[t - 1];
        char msg[260];
        std::snprintf(msg, sizeof msg, "%s: COLUMN did not retrace (G=%d O=%d L=%d R=%d %s lens=%d/%d) fwd step %d: %d vs %d",
                      name, G, O, L, R, abs1 ? "abs1" : "cycle", lenMel, lenRest, t - 1, r.col, f.col);
        CHECK(r.col == f.col, msg);
        std::snprintf(msg, sizeof msg, "%s: %s READ/LANES did not retrace (G=%d O=%d L=%d R=%d %s) fwd step %d: read %d vs %d",
                      name, voiceWindow ? "voice-window" : "baseline", G, O, L, R, abs1 ? "abs1" : "cycle", t - 1, r.read, f.read);
        CHECK(r.read == f.read && r.lane == f.lane, msg);
    }
}

int main() {
    const struct { LD d; const char* n; } dirs[] = {
        {LD::Forward, "FWD"}, {LD::Reverse, "REV"}, {LD::Pendulum, "PEND"}, {LD::PingPong, "PING"} };
    const int Gs[] = {16, 12};
    for (auto& d : dirs)
        for (int G : Gs) {
            run(G, 0, G, 0, d.d, false, 7, 5, d.n, /*voiceWindow=*/false);   // baseline: lanes' own polymeter only
        }
    for (auto& d : dirs)
        for (int G : Gs)
            for (int abs1 = 0; abs1 < 2; ++abs1)
                for (int L : {G, 8, 5})
                    for (int O : {0, 3})
                        for (int R : {0, 2})
                            run(G, O, L, R, d.d, abs1 != 0, 7, 5, d.n, /*voiceWindow=*/true);
    if (g_fail) { std::printf("test_engine_reverse_roundtrip: %d FAILED (%d cases)\n", g_fail, g_cases); return 1; }
    std::printf("test_engine_reverse_roundtrip: all %d cases passed (column, every lane tick and the voice read retrace)\n", g_cases);

    return 0;
}
