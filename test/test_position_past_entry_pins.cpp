// test_position_past_entry_pins.cpp — CC_PROMPT_POSITION_RESOLVER_NETSTEPS, step 1 (widen the pins)
//
// THE COVERAGE GAP THIS CLOSES. Nothing in the suite pins what the read does once the
// transport travels PAST its own phrase entry. test_reverse_rotation_anchor's gate 3
// (driveFwdThenRev) marches forward N = lenG+3 and then reverses down to fwdCol[0] — it
// stops exactly ON the entry and never crosses it. So the whole past-entry region is
// unpinned. That is why the slice-4 readCoord swap attempt was caught only by unrelated
// goldens, and only in configurations where lenG == L.
//
// WHAT IS PINNED HERE (today's behaviour, master). PositionResolver::readCoord() signs
// its magnitude by entryDir_ — the direction of the FIRST motion since reset/window
// change — NOT by the current direction. So once the traversal walks past its own entry,
// readCoord keeps the ENTRY side's coordinate (wrapped into the window), which makes
// readCoord a pure function of the played COLUMN. The observable consequence, and the
// thing gates 1 and 2 pin, is:
//
//     SAME COLUMN -> SAME MACRO CELL, for the whole run, in either direction.
//
// WHY IT MATTERS. The net-steps proposal (readCoord = netSteps % lenG in C++ truncated
// form, retiring entryDir_) is history-INDEPENDENT: past the entry it reads what a COLD
// start in that direction would read. A column revisited from the other side then yields
// a DIFFERENT cell whenever L does not divide lenG. Gates 1 and 2 are therefore EXPECTED
// TO MOVE when that swap lands. They exist so the blast radius is explicit and the swap
// PR must re-pin them deliberately, with the rationale recorded, rather than discovering
// the change through an unrelated golden. Gate 3 is the invariant that must SURVIVE the
// swap untouched: window containment, and the rotation at every phrase entry.
//
// Config 5 is the CONTROL: L divides lenG, so the two laws are congruent and gates 1-2
// must stay green through the swap. If the control ever moves, the swap did more than
// the past-entry change it is supposed to make.
//
// These gates PASS on master. This is a CHARACTERIZATION pin, not a reproduce-first bug
// test — there is no bug here; the behaviour is simply undocumented by the suite.
// Registered with $SE $GS $PE $CE.
#include <cstdio>
#include "engines/SequencerEngine.hpp"
#include "VoiceResolver.hpp"
#include "ui/SandsRangeGestureCore.hpp"
using redDot::SandsRangeGestureCore;

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("  FAIL: %s\n", m); ++g_fail; } } while (0)
static constexpr int RING = 32;

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

// Walk firstN steps in firstDir, then thenN steps in the OPPOSITE direction — far enough
// to cross the entry and complete a global lap beyond it. Every column that recurs must
// report the macro cell it reported the first time it was played.
static void pinSameColumnSameCell(const Cfg& c, int firstDir, int firstN, int thenN, const char* tag) {
    SequencerEngine eng; configure(eng, c);
    int seen[RING];
    for (int i = 0; i < RING; ++i) seen[i] = -1;
    char msg[224];
    int revisits = 0;

    auto step = [&](int dir) {
        eng.advancePlayhead(dir, nullptr);
        const int col = eng.playedColumn();
        const int rd  = eng.macroFrameRead();
        if (col < 0 || col >= RING || rd < 0) return;
        std::snprintf(msg, sizeof(msg),
            "%s lenG=%d offG=%d L=%d O=%d R=%d: macro read %d left the window [O,O+L)",
            tag, c.lenG, c.offG, c.L, c.O, c.R, rd);
        CHECK(SandsRangeGestureCore::windowContains(c.O, c.L, rd, RING), msg);
        if (seen[col] < 0) { seen[col] = rd; return; }
        std::snprintf(msg, sizeof(msg),
            "%s lenG=%d offG=%d L=%d O=%d R=%d: column %d read %d but read %d when first played "
            "(same column -> same cell broken past the entry)",
            tag, c.lenG, c.offG, c.L, c.O, c.R, col, rd, seen[col]);
        CHECK(seen[col] == rd, msg);
        ++revisits;
    };

    for (int t = 0; t < firstN; ++t) step(firstDir);
    for (int t = 0; t < thenN;  ++t) step(-firstDir);

    std::snprintf(msg, sizeof(msg), "%s lenG=%d: no column was revisited — the pin proved nothing",
                  tag, c.lenG);
    CHECK(revisits >= 1, msg);
}

// The invariant that must survive the swap: a pure single-direction run keeps the read
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

    std::printf("gate 1: FWD then REVERSE PAST THE ENTRY — same column still reads the same cell\n");
    for (const Cfg& c : kCfgs) pinSameColumnSameCell(c, +1, 3, 2 * c.lenG, "FWD->REV-past-entry");

    std::printf("gate 2: COLD REVERSE then FORWARD PAST THE ENTRY — the mirror\n");
    for (const Cfg& c : kCfgs) pinSameColumnSameCell(c, -1, 3, 2 * c.lenG, "REV->FWD-past-entry");

    std::printf("gate 3: pure runs across laps — containment + rotation at every phrase entry (must survive the swap)\n");
    for (const Cfg& c : kCfgs) pinPureRun(c, +1, "FWD-pure");
    for (const Cfg& c : kCfgs) pinPureRun(c, -1, "REV-pure");

    if (g_fail) { std::printf("FAILED: %d check(s)\n", g_fail); return 1; }
    std::printf("OK\n");
    return 0;
}
