// Per-lane direction (reverse) — pins the accumulated-tick read model and the flip
// semantics BEFORE the (un-compilable-here) engine change, per plans/lane_direction.md.
//
// Model under test (replica of the intended engine):
//   - each lane owns an accumulated tick, advanced per step by globalDir * laneSign
//   - cell = getStrandIdx(laneTick, len, off, rot)   [exact copy of the real impl]
//   - a flip promotes laneSignPending -> laneSign at a boundary:
//       StepEdge : promote at the top of every step (before advancing)   [built first]
//       Phrase   : defer promotion until the global window wraps          [layered on]
//
// Proves: all-forward is bit-identical to today (laneTick == totalStepsElapsed); a reverse
// lane RETRACES its cells (pendulum turn, no jump); StepEdge turns around to the adjacent
// window cell (not the negate-tick mirror jump); Phrase defers the flip to the wrap; and
// the sign composes with global Mode E reverse.
//
// Build: g++ -std=c++17 test/test_lane_direction.cpp
#include <cstdio>
#include <vector>
#include <string>
#include <algorithm>

static int passed = 0, failed = 0;
static void check(bool ok, const char* what) {
    if (ok) ++passed; else { ++failed; std::printf("  FAIL: %s\n", what); }
}

static const long DNA_LCM = 720720;                 // matches SequencerEngine
static long wrapT(long t) { return ((t % DNA_LCM) + DNA_LCM) % DNA_LCM; }

// Exact copy of SequencerEngine::getStrandIdx.
static int getStrandIdx(long tick, int len, int off, int rot) {
    int safeLen = std::max(1, len);
    int timelineIdx = (int)((((tick + rot) % safeLen) + safeLen) % safeLen);
    return (timelineIdx + off) % 16;
}

enum class Quant { StepEdge, Phrase };

struct Lane {
    long tick = 0;
    int  sign = +1;        // applied
    int  pending = +1;     // requested by UI
    // One sequencer step. `wrapped` = advancePlayhead reported a phrase boundary on THIS step.
    void step(int globalDir, Quant q, bool wrapped) {
        if (q == Quant::StepEdge) sign = pending;          // promote before advancing
        tick = wrapT(tick + (long)globalDir * sign);
        if (q == Quant::Phrase && wrapped) sign = pending; // defer promotion to the wrap
    }
};

int main() {
    std::printf("\033[1mPer-lane direction (accumulated tick)\033[0m\n");
    std::printf("%s\n", std::string(46, '=').c_str());

    const int L = 7, O = 5, R = 3;   // an arbitrary window (len, off, rot)

    // ── 1. All-forward is bit-identical to today ──────────────────────────────────
    {
        Lane ln;                       // sign=+1
        long total = 0;                // the engine's global tick
        bool same = true;
        for (int s = 0; s < 200; ++s) {
            ln.step(+1, Quant::StepEdge, false);
            total = wrapT(total + 1);
            if (getStrandIdx(ln.tick, L,O,R) != getStrandIdx(total, L,O,R)) same = false;
            if (ln.tick != total) same = false;
        }
        check(same, "forward lane == global totalStepsElapsed (bit-identical, no regression)");
    }

    // ── 2. A reverse lane RETRACES: forward then reverse revisits the same cells ───
    {
        Lane ln;
        std::vector<int> fwd;
        for (int s = 0; s < 5; ++s) { ln.step(+1, Quant::StepEdge, false); fwd.push_back(getStrandIdx(ln.tick,L,O,R)); }
        ln.pending = -1;                                   // request reverse
        std::vector<int> rev;
        for (int s = 0; s < 5; ++s) { ln.step(+1, Quant::StepEdge, false); rev.push_back(getStrandIdx(ln.tick,L,O,R)); }
        // After reaching fwd = [c1..c5], reverse retraces c4,c3,c2,c1,c0 (pivot's neighbour first).
        bool retrace = (rev[0]==fwd[3] && rev[1]==fwd[2] && rev[2]==fwd[1] && rev[3]==fwd[0]);
        check(retrace, "reverse retraces the cells just played (pendulum turn)");
    }

    // ── 3. StepEdge turnaround is ADJACENT — not the negate-tick mirror jump ───────
    {
        Lane ln;
        for (int s = 0; s < 9; ++s) ln.step(+1, Quant::StepEdge, false);   // tick = 9
        long pivot = ln.tick;
        ln.pending = -1;
        ln.step(+1, Quant::StepEdge, false);               // first reverse step
        check(ln.tick == pivot - 1, "StepEdge flip steps to the adjacent tick (turnaround)");
        // The stateless negate-tick model would instead jump to getStrandIdx(-pivot,...);
        // show the accumulated cell is the window-neighbour, generally != that mirror.
        int neighbourCell = getStrandIdx(pivot - 1, L,O,R);
        int mirrorJump    = getStrandIdx(-pivot,    L,O,R);
        check(getStrandIdx(ln.tick,L,O,R) == neighbourCell, "turnaround lands on the neighbour cell");
        check(neighbourCell != mirrorJump, "accumulated turnaround avoids the negate-tick jump");
    }

    // ── 4. Phrase quant DEFERS the flip until the wrap ────────────────────────────
    {
        Lane ln;
        ln.pending = -1;                                   // request reverse immediately
        // Steps 0..3 are mid-phrase (no wrap): sign must stay +1 (still advancing forward).
        long prev = ln.tick;
        for (int s = 0; s < 4; ++s) { ln.step(+1, Quant::Phrase, /*wrapped=*/false);
            check(ln.tick == prev + 1, "Phrase: mid-phrase step stays forward (flip deferred)"); prev = ln.tick; }
        // The wrap step promotes; the NEXT step goes reverse.
        ln.step(+1, Quant::Phrase, /*wrapped=*/true);      // this step still forward, promotes at end
        prev = ln.tick;
        ln.step(+1, Quant::Phrase, false);
        check(ln.tick == prev - 1, "Phrase: reverse takes effect on the step after the wrap");
    }

    // ── 5. Composition with global Mode E reverse ────────────────────────────────
    {
        // Follow-global lane under global reverse walks BACKWARD (tracks global).
        Lane a; a.step(-1, Quant::StepEdge, false);
        check(a.tick == wrapT(-1), "follow-global lane tracks Mode E reverse (tick--)");
        // Reverse lane under global reverse walks FORWARD (opposite of global; double negative).
        Lane b; b.sign = -1; b.pending = -1; b.step(-1, Quant::StepEdge, false);
        check(b.tick == +1, "reverse lane under Mode E goes forward (opposite-to-global)");
    }

    std::printf("%d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
