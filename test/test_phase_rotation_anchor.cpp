// test/test_phase_rotation_anchor.cpp — CC_PROMPT_POSITION_RESOLVER_SLICE2B_PHASE
//
// The PHASE-mode rotation anchor (Mode E). Slice 2 fixed gate/clock reverse rotation by
// feeding the derive the resolver's TRANSPORT-SIGNED phrase coordinate — but PHASE mode
// does not advance, it SNAPS (snapToPhaseStep -> pos_.snapTo(stepIndex), which sets the
// COLUMN only), so in phase:
//   - the direction is STALE (whatever the last gate/clock advance left; +1 from reset)
//     -> decreasing the phase knob "jumps forward" instead of reversing;
//   - after a reverse gate movement, even a FORWARD phase sweep reads one step past R.
//
// THE SPEC (Rodney): the voice lane playhead in phase snaps to R at BOTH extremes
// (phase 0% AND 100% — both are phrase entries, the 0% entry and one full cycle later);
// forward phase counts UP from R; backward phase counts DOWN from R; the jump back to R
// comes LAST in whichever direction is being played. This is slice 2's signed rule,
// driven by the phase sweep: forward phrasePos = absStep mod lenG (== wG, bit-identical);
// backward phrasePos = (−absStep) mod lenG (the steps back to the cycle-0 entry — the
// phase entries are the CYCLE-0 crossings, not the gate traversal's endStep boundary).
//
// Reproduce-first: gates 3+5 FAIL on the pre-fix branch (the backward sweep counts up —
// the stale +1; the stale −1 offsets the forward walk); gates 1/2/4 pass from a fresh
// engine (the forward path was already wG-correct — the doc's "direction-only" question,
// answered: the FORWARD path needed nothing, the BACKWARD sweep needed the phrasePos
// reconciliation in addition to the direction).
#include <cstdio>
#include "engines/SequencerEngine.hpp"
#include "VoiceResolver.hpp"
#include "ui/SandsRangeGestureCore.hpp"
using redDot::SandsRangeGestureCore;

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("  FAIL: %s\n", m); ++g_fail; } } while (0)
static constexpr int RING = 32;
static constexpr int PPS  = 6;    // PULSES_PER_STEP (NoteValues.hpp) — the production divider

struct Cfg { int lenG, offG, L, O, R; };
static const Cfg kCfgs[] = {
    {8, 4, 8, 0, 3},   // Rodney's slice-2b config (global [4,12), row = full length, R=3)
    {8, 4, 7, 1, 4},   // voice shorter than the global window
    {16, 0, 7, 2, 4},  // full-ring global, non-dividing voice
    {12, 5, 5, 2, 3},  // odd global window + voice
};

static int wrapm(long v, int m) { return (int)(((v % m) + m) % m); }

// The expected read of a Forward voice row at transport-signed coordinate sPos:
// wnow = (sPos + R) mod L; read = (wnow + O) & 31 (frameReadCell — the engine's ONE law).
static int readAt(const Cfg& c, long sPos) {
    const long wnow = ((sPos + c.R) % c.L + c.L) % c.L;
    return (int)((wnow + c.O) & (RING - 1));
}

static void setup(SequencerEngine& eng, const Cfg& c) {
    eng.reset();
    eng.updateWindow((float)c.lenG, 0.f, false, (float)c.offG, 0.f, false);
    eng.numPolyVoices = 4;
    eng.maskMacroKind_ = SequencerEngine::MASK_CYCLE;
    eng.macroVoiceLor_[SequencerEngine::LOR_LEN] = c.L;
    eng.macroVoiceLor_[SequencerEngine::LOR_OFF] = c.O;
    eng.macroVoiceLor_[SequencerEngine::LOR_ROT] = c.R;
}

// ── GATE 1+2+4: the FORWARD sweep (absStep 0 -> lenG) ──
// Gate 1: R at BOTH extremes (absStep 0 and absStep lenG — the cycle-0 entries).
// Gate 2: the walk counts UP from R, never leaving the window; the jump back to R lands
//         at the far extreme (absStep lenG).
// Gate 4: bit-identity — the walk IS the pre-slice-2 wG fold (read = ((wG+R) mod L + O)).
//         (For a Forward row the wG fold at wG=k is exactly readAt(+k).)
static void fwdSweep(const Cfg& c) {
    SequencerEngine eng; setup(eng, c);
    const int rot = SandsRangeGestureCore::wrapMod(c.O + (c.R % c.L), RING);
    char msg[160];
    bool extOK = true, walkOK = true, inWin = true;
    for (long a = 0; a <= c.lenG; ++a) {
        eng.snapToPhaseStep(a * PPS, PPS);
        const int r = eng.macroFrameRead();
        const int want = (a == c.lenG) ? rot : readAt(c, a);   // a==lenG: cycle 0 — the jump back to R
        if (a == 0 || a == c.lenG) extOK = extOK && r == rot;
        walkOK = walkOK && r == want;
        inWin = inWin && SandsRangeGestureCore::windowContains(c.O, c.L, r, RING);
    }
    std::snprintf(msg, sizeof msg, "FWD lenG=%d offG=%d L=%d O=%d R=%d: R at BOTH extremes (0%% and 100%%)",
                  c.lenG, c.offG, c.L, c.O, c.R);
    CHECK(extOK, msg);
    std::snprintf(msg, sizeof msg, "FWD lenG=%d offG=%d L=%d O=%d R=%d: counts UP from R, in-window, jump-to-R at the far extreme (== the wG fold, bit-identical)",
                  c.lenG, c.offG, c.L, c.O, c.R);
    CHECK(walkOK && inWin, msg);
}

// ── GATE 3: the BACKWARD sweep (absStep lenG -> 0, decreasing) ──
// The walk counts DOWN from R (R at lenG, R-1 at lenG-1, ...); the jump back to R lands
// LAST (absStep 0). The per-snap direction is -1 (decreasing absStep).
static void revSweep(const Cfg& c) {
    SequencerEngine eng; setup(eng, c);
    const int rot = SandsRangeGestureCore::wrapMod(c.O + (c.R % c.L), RING);
    char msg[160];
    bool walkOK = true, lastIsR = false, inWin = true;
    for (long a = c.lenG; a >= 0; --a) {
        eng.snapToPhaseStep(a * PPS, PPS);
        const int r = eng.macroFrameRead();
        const int want = (a == 0) ? rot : readAt(c, a - c.lenG);   // sPos = a - lenG (counts down); a==0: the jump back to R
        walkOK = walkOK && r == want;
        inWin = inWin && SandsRangeGestureCore::windowContains(c.O, c.L, r, RING);
        if (a == 0) lastIsR = (r == rot);
    }
    std::snprintf(msg, sizeof msg, "REV lenG=%d offG=%d L=%d O=%d R=%d: counts DOWN from R (decreasing the knob reverses), in-window",
                  c.lenG, c.offG, c.L, c.O, c.R);
    CHECK(walkOK && inWin, msg);
    std::snprintf(msg, sizeof msg, "REV lenG=%d offG=%d L=%d O=%d R=%d: the jump back to R lands LAST (absStep 0)",
                  c.lenG, c.offG, c.L, c.O, c.R);
    CHECK(lastIsR, msg);
}

// ── GATE 5: the STALE-DIRECTION repro (the real-world regression) ──
// A reverse GATE movement leaves dir_ = -1; switching to phase and sweeping FORWARD must
// still walk the FORWARD sequence (after the first snap — the direction is only known
// between consecutive quantisations). Pre-fix: the whole sweep reads the stale-reverse
// fold (one step past R at phase 0 — the doc's off-by-one).
static void staleDir(const Cfg& c) {
    SequencerEngine eng; setup(eng, c);
    const int rot = SandsRangeGestureCore::wrapMod(c.O + (c.R % c.L), RING);
    eng.advancePlayhead(-1, nullptr);        // a reverse gate movement: dir_ = -1 (stale for phase)
    eng.snapToPhaseStep(0, PPS);             // the priming snap at phase 0 (R either way)
    char msg[160];
    bool ok = eng.macroFrameRead() == rot;
    for (long a = 0; a <= c.lenG; ++a) {    // the forward sweep
        eng.snapToPhaseStep(a * PPS, PPS);
        const int want = (a == c.lenG) ? rot : readAt(c, a);
        ok = ok && eng.macroFrameRead() == want;
    }
    std::snprintf(msg, sizeof msg, "STALE lenG=%d offG=%d L=%d O=%d R=%d: after a reverse gate movement, a FORWARD phase sweep walks the FORWARD sequence (R at 0%%, counting up)",
                  c.lenG, c.offG, c.L, c.O, c.R);
    CHECK(ok, msg);
}

int main() {
    std::printf("== test_phase_rotation_anchor ==\n");
    std::printf("gate 1+2+4: FORWARD — R at both extremes, counts up (== the wG fold, bit-identical)\n");
    for (const Cfg& c : kCfgs) fwdSweep(c);
    std::printf("gate 3: BACKWARD — counts down from R, the jump back to R last\n");
    for (const Cfg& c : kCfgs) revSweep(c);
    std::printf("gate 5: STALE-DIRECTION — a reverse gate movement then a forward phase sweep\n");
    for (const Cfg& c : kCfgs) staleDir(c);
    if (g_fail) { std::printf("FAILED: %d check(s)\n", g_fail); return 1; }
    std::printf("OK\n");
    return 0;
}
