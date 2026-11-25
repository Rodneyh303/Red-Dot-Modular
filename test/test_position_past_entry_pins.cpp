// test_position_past_entry_pins.cpp — CC_PROMPT_POSITION_RESOLVER_NETSTEPS
//
// THE COVERAGE GAP THIS CLOSED (step 1). Nothing in the suite pinned what the read does
// once the transport travels PAST its own phrase entry. test_reverse_rotation_anchor's
// gate 3 marches forward lenG+3 and then reverses down to fwdCol[0] — it stops exactly
// ON the entry and never crosses it. That is why the slice-4 readCoord swap attempt was
// caught only by unrelated goldens, and only where lenG == L.
//
// WHAT THE READ LAW IS NOW (step 2, the net-steps swap). readCoord() is one signed
// counter of NET motion since the entry, folded into the global window:
//
//     readCoord = netSteps % lenG          (C++ truncated: the sign rides the count)
//
// There is no entryDir_ and no direction field in it. The consequence these gates pin is
// HISTORY INDEPENDENCE:
//
//     the read is a PURE FUNCTION OF netSteps — where you are, not how you got there.
//
// Two runs standing at the same net offset read the same cell, whether they arrived by
// marching straight there or by overshooting and coming back past their own entry.
//
// WHAT THIS REPLACED. readCoord used to sign its magnitude by entryDir_ — the direction
// of the FIRST motion since reset/window change — which made it a pure function of the
// played COLUMN instead, giving "same column -> same cell" for a whole run. That was a
// RETRACE that held even after the traversal walked back past its own entry, and it cost
// a bit of hidden entry history that could only ever anchor ONE entry point. Gates 1 and
// 2 pinned that old law in step 1 and are re-pinned here to the new one: they FAIL on the
// pre-swap resolver and PASS after, which is the reproduce-first proof of the change.
//
// NOT CHANGED by the swap, and still pinned here: containment, the rotation at every
// phrase entry (gate 3), and the behaviour of every pure single-direction run — forward
// and cold reverse are bit-identical under both laws. Config 5 is the CONTROL, where L
// divides lenG so the two laws agree everywhere; if the control ever moves, a change did
// more than the past-entry one it was supposed to make.
//
// Registered with $SE $GS $PE $CE.
#include <cstdio>
#include "engines/SequencerEngine.hpp"
#include "VoiceResolver.hpp"
#include "ui/SandsRangeGestureCore.hpp"
using redDot::SandsRangeGestureCore;

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("  FAIL: %s\n", m); ++g_fail; } } while (0)
static constexpr int RING = 32;
static constexpr int SPAN = 64;   // net offsets tabulated either side of the entry

struct Cfg { int lenG, offG, L, O, R; };
static const Cfg kCfgs[] = {
    {8,  4, 7, 1, 4},   // voice shorter than the global window (L does not divide lenG)
    {16, 0, 7, 2, 4},   // full-ring global, non-dividing row
    {12, 5, 5, 2, 3},   // odd global window + odd row
    {5,  0, 3, 1, 2},   // the net-steps worksheet's shape (small, L does not divide lenG)
    {8,  0, 4, 0, 1},   // CONTROL: L divides lenG — both laws agree; must not move
};

static void configure(SequencerEngine& eng, const Cfg& c) {
    eng.reset();
    eng.updateWindow((float)c.lenG, 0.f, false, (float)c.offG, 0.f, false);
    eng.numPolyVoices = 4;
    eng.maskMacroKind_ = SequencerEngine::MASK_CYCLE;
    eng.macroVoiceLor_[SequencerEngine::LOR_LEN] = c.L;
    eng.macroVoiceLor_[SequencerEngine::LOR_OFF] = c.O;
    eng.macroVoiceLor_[SequencerEngine::LOR_ROT] = c.R;
}

// The resolver's own accumulation rule, mirrored so the test can name a step's net offset:
// the FIRST motion only ENTERS (the read lands AT the entry, net 0); later motions count.
struct NetCount {
    long net = 0; bool entered = false;
    void step(int motion) { if (motion == 0) return; if (!entered) entered = true; else net += motion; }
};

// A cold run in ONE direction tabulates the read at each net offset it reaches. These are
// the reference tables: both laws agree on pure runs, so they are stable ground truth.
static void tabulateCold(const Cfg& c, int dir, int reads[SPAN]) {
    for (int i = 0; i < SPAN; ++i) reads[i] = -1;
    SequencerEngine eng; configure(eng, c);
    NetCount n;
    for (int t = 0; t < SPAN; ++t) {
        eng.advancePlayhead(dir, nullptr);
        n.step(dir);
        const long mag = (n.net < 0) ? -n.net : n.net;
        if (mag >= SPAN) break;
        const int rd = eng.macroFrameRead();
        if (rd >= 0 && reads[mag] < 0) reads[mag] = rd;
    }
}

// Walk firstN steps in firstDir, then thenN in the OPPOSITE direction — far enough to
// cross the entry and complete a lap beyond it. At EVERY step the read must equal the
// cold-run read for that net offset: the path taken must not matter.
static void pinHistoryIndependence(const Cfg& c, int firstDir, int firstN, int thenN, const char* tag) {
    int coldFwd[SPAN], coldRev[SPAN];
    tabulateCold(c, +1, coldFwd);
    tabulateCold(c, -1, coldRev);

    SequencerEngine eng; configure(eng, c);
    NetCount n;
    char msg[256];
    int crossed = 0;

    auto step = [&](int dir) {
        eng.advancePlayhead(dir, nullptr);
        n.step(dir);
        const int rd = eng.macroFrameRead();
        if (rd < 0) return;
        std::snprintf(msg, sizeof(msg),
            "%s lenG=%d offG=%d L=%d O=%d R=%d: macro read %d left the window [O,O+L)",
            tag, c.lenG, c.offG, c.L, c.O, c.R, rd);
        CHECK(SandsRangeGestureCore::windowContains(c.O, c.L, rd, RING), msg);

        const long mag = (n.net < 0) ? -n.net : n.net;
        if (mag >= SPAN) return;
        const int want = (n.net < 0) ? coldRev[mag] : coldFwd[mag];
        if (want < 0) return;
        if (firstDir > 0 ? (n.net < 0) : (n.net > 0)) ++crossed;   // we are past the entry
        std::snprintf(msg, sizeof(msg),
            "%s lenG=%d offG=%d L=%d O=%d R=%d: at net %+ld read %d but a COLD run reads %d there "
            "(the read depends on the path — history independence broken)",
            tag, c.lenG, c.offG, c.L, c.O, c.R, n.net, rd, want);
        CHECK(rd == want, msg);
    };

    for (int t = 0; t < firstN; ++t) step(firstDir);
    for (int t = 0; t < thenN;  ++t) step(-firstDir);

    std::snprintf(msg, sizeof(msg), "%s lenG=%d: never crossed the entry — the pin proved nothing",
                  tag, c.lenG);
    CHECK(crossed >= 1, msg);
}

// The invariant the swap must NOT touch: a pure single-direction run keeps the read
// inside [O,O+L) and lands on the rotation at every phrase entry, across several laps.
static void pinPureRun(const Cfg& c, int dir, const char* tag) {
    SequencerEngine eng; configure(eng, c);
    const int rot = SandsRangeGestureCore::wrapMod(c.O + (c.R % c.L), RING);
    char msg[224];
    for (int t = 0; t < 3 * c.lenG; ++t) {
        eng.advancePlayhead(dir, nullptr);
        const int rd = eng.macroFrameRead();
        if (rd < 0) continue;
        std::snprintf(msg, sizeof(msg),
            "%s lenG=%d offG=%d L=%d O=%d R=%d: macro read %d left the window [O,O+L)",
            tag, c.lenG, c.offG, c.L, c.O, c.R, rd);
        CHECK(SandsRangeGestureCore::windowContains(c.O, c.L, rd, RING), msg);
        if ((t % c.lenG) == 0) {
            std::snprintf(msg, sizeof(msg),
                "%s lenG=%d offG=%d L=%d O=%d R=%d: rotation not at the phrase entry (got %d, want %d)",
                tag, c.lenG, c.offG, c.L, c.O, c.R, rd, rot);
            CHECK(rd == rot, msg);
        }
    }
}

int main() {
    std::printf("== test_position_past_entry_pins ==\n");

    std::printf("gate 1: FWD then REVERSE PAST THE ENTRY — the read matches a COLD REVERSE at the same net offset\n");
    for (const Cfg& c : kCfgs) pinHistoryIndependence(c, +1, 3, 2 * c.lenG, "FWD->REV-past-entry");

    std::printf("gate 2: COLD REVERSE then FORWARD PAST THE ENTRY — the mirror\n");
    for (const Cfg& c : kCfgs) pinHistoryIndependence(c, -1, 3, 2 * c.lenG, "REV->FWD-past-entry");

    std::printf("gate 3: pure runs across laps — containment + rotation at every phrase entry (unchanged by the swap)\n");
    for (const Cfg& c : kCfgs) pinPureRun(c, +1, "FWD-pure");
    for (const Cfg& c : kCfgs) pinPureRun(c, -1, "REV-pure");

    if (g_fail) { std::printf("FAILED: %d check(s)\n", g_fail); return 1; }
    std::printf("OK\n");
    return 0;
}
