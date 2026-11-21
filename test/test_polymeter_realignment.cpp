// test_polymeter_realignment — CLAIM (CANONICAL_CLAIM.md): per-lane LOR inside a global frame makes lanes
// POLYMETRIC against each other and the global cycle — they drift and REALIGN — and it is all reversible.
// (docs/design/CLAIMS_EVIDENCE.md A7.)  The literal example from the claim: a length-7 melody lane against
// a length-5 rhythm lane inside a 16-step global.
//
// Pinned on the REAL SequencerEngine (one poly voice, no voice window — the lanes' own windows only):
//   (1) each lane's tick at forward step t is exactly  t mod L_lane   (free-running against the 16 frame);
//   (2) the lanes REALIGN (both ticks back to 0) first at t = lcm(7,5) = 35, and every lcm thereafter;
//   (3) between realignments the pair (melTick, restTick) never repeats — 35 distinct joint states;
//   (4) the global column advances t mod 16 independently (lanes do not drag the global frame);
//   (5) REVERSE retraces the same joint states exactly (the polymeter is reversible).
// Several (La, Lb) pairs are swept so the claim is the general law, not one example.
//
// OBSERVATION (2026-11-21, measured, NOT asserted — needs Rodney's call): at G = 16 and G = 32 the lanes
// FREE-RUN across the global boundary (tick = t mod L, realign at lcm). At other G (8, 10, 12, 15, 24 probed)
// the lane ticks RESTART at 0 at every global boundary, so the lanes realign at the global loop edge instead.
// Both are coherent polymeter, but "free-run" vs "restart at the global edge" depending on G is worth
// confirming as intended before the claim is worded as 'drifting and realigning over the global loop'.
#include <cstdio>
#include <set>
#include <utility>
#include <vector>
#include "engines/SequencerEngine.hpp"
#include "VoiceResolver.hpp"
using dotModular::VoiceResolver; using LaneId = dotModular::LaneId;

static int g_fail = 0, g_cases = 0;
#define CHECK(c, m) do { if (!(c)) { if (g_fail < 40) std::printf("  FAIL: %s\n", (m)); ++g_fail; } } while (0)

static int gcd(int a, int b) { return b ? gcd(b, a % b) : a; }

static void run(int G, int La, int Lb) {
    SequencerEngine e; e.reset(); e.numPolyVoices = 2;
    const auto v = VoiceResolver::slot(2);
    const int A = SequencerEngine::PL_MELODY, B = SequencerEngine::PL_REST;
    e.polyLenERef(v, LaneId{A}) = La;
    e.polyLenERef(v, LaneId{B}) = Lb;
    e.updateWindow((float)G, 0.f, false, 0.f, 0.f, false);
    const int lcm = La / gcd(La, Lb) * Lb;
    const int N = 3 * lcm;
    std::vector<std::pair<int,int>> joint;
    std::set<std::pair<int,int>> seen;
    char msg[200];
    int firstAlign = -1;
    for (int t = 0; t < N; ++t) {
        e.advancePlayhead(1, nullptr);
        const int ta = e.laneTickV_[v.v][A], tb = e.laneTickV_[v.v][B];
        joint.push_back({ta, tb});
        std::snprintf(msg, sizeof msg, "G=%d La=%d Lb=%d t=%d: lane A tick %d != t mod La %d", G, La, Lb, t, ta, t % La);
        CHECK(ta == t % La, msg);
        std::snprintf(msg, sizeof msg, "G=%d La=%d Lb=%d t=%d: lane B tick %d != t mod Lb %d", G, La, Lb, t, tb, t % Lb);
        CHECK(tb == t % Lb, msg);
        std::snprintf(msg, sizeof msg, "G=%d La=%d Lb=%d t=%d: global column %d != t mod G %d", G, La, Lb, t, e.pos_.playedColumn(), t % G);
        CHECK(e.pos_.playedColumn() == t % G, msg);
        if (t < lcm) {
            std::snprintf(msg, sizeof msg, "G=%d La=%d Lb=%d t=%d: joint state repeated before the lcm", G, La, Lb, t);
            CHECK(seen.insert({ta, tb}).second, msg);
        }
        if (t > 0 && ta == 0 && tb == 0 && firstAlign < 0) firstAlign = t;
    }
    std::snprintf(msg, sizeof msg, "G=%d La=%d Lb=%d: first realignment at %d, expected lcm %d", G, La, Lb, firstAlign, lcm);
    CHECK(firstAlign == lcm, msg);
    std::snprintf(msg, sizeof msg, "G=%d La=%d Lb=%d: %zu distinct joint states, expected lcm %d", G, La, Lb, seen.size(), lcm);
    CHECK((int)seen.size() == lcm, msg);
    // (5) reverse retraces the forward joint states exactly
    for (int t = N - 1; t >= 1; --t) {
        e.advancePlayhead(-1, nullptr);
        const std::pair<int,int> r{e.laneTickV_[v.v][A], e.laneTickV_[v.v][B]};
        std::snprintf(msg, sizeof msg, "G=%d La=%d Lb=%d: reverse did not retrace joint state at fwd step %d", G, La, Lb, t - 1);
        CHECK(r == joint[t - 1], msg);
    }
    ++g_cases;
}

int main() {
    run(16, 7, 5);                         // the literal claim example
    run(16, 3, 4); run(16, 6, 9); run(16, 5, 8); run(16, 7, 7); run(16, 16, 15);
    run(32, 7, 5); run(32, 9, 6);          // the 32-step ring behaves the same
    if (g_fail) { std::printf("test_polymeter_realignment: %d FAILED\n", g_fail); return 1; }
    std::printf("test_polymeter_realignment: all %d (La,Lb) cases passed — lanes tick t mod L, realign at lcm, reverse retraces\n", g_cases);
    return 0;
}
