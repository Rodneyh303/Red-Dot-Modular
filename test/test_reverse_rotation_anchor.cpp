// test_reverse_rotation_anchor.cpp — CC_FIX_REVERSE_ROTATION_ANCHOR
//
// The cycle phrase anchor pins the voice rotation to wG==0 — the FORWARD phrase
// entry (startStep / the window's lower bound). Going forward that's the first-
// played step, so the voice is at R there (correct). Going BACKWARD the first-
// played step is the OTHER end (endStep / the O+L upper bound), but the anchor
// stays at wG==0, so the rotation lands mid-phrase instead of at the reverse entry
// (Rodney, Rack). RULE (confirmed): the rotation marks the phrase ENTRY in whatever
// direction it is played — forward enters at R and counts up, reverse enters at R
// and counts down.
//
// Reproduce-first: the REVERSE gate FAILS today (the read is not R at the first
// reverse step). FORWARD and offset-0 are unchanged. Registered with $SE $GS $PE $CE.
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
    {8, 4, 8, 0, 3},   // Rodney's config (global [4,12), row = full length, R=3)
    {8, 4, 7, 1, 4},   // voice shorter than the global window
    {16, 0, 7, 2, 4},  // full-ring global, non-dividing voice
    {12, 5, 5, 2, 3},  // odd global window + voice
};

// The voice is at its rotation (phase R) at EVERY phrase entry, in BOTH directions.
// The phrase entry is the first played step after reset and recurs every lenG steps,
// so (t-1) % lenG == 0 marks it whichever way we play.
static void driveMacro(const Cfg& c, int dir, const char* tag) {
    SequencerEngine eng; eng.reset();
    eng.updateWindow((float)c.lenG, 0.f, false, (float)c.offG, 0.f, false);
    eng.numPolyVoices = 4;
    eng.maskMacroKind_ = SequencerEngine::MASK_CYCLE;
    eng.macroVoiceLor_[SequencerEngine::LOR_LEN] = c.L;
    eng.macroVoiceLor_[SequencerEngine::LOR_OFF] = c.O;
    eng.macroVoiceLor_[SequencerEngine::LOR_ROT] = c.R;
    const int rot = SandsRangeGestureCore::wrapMod(c.O + (c.R % c.L), RING);
    char msg[160];
    for (int t = 0; t < 3 * c.lenG; ++t) {
        eng.advancePlayhead(dir, nullptr);
        const int r = eng.macroFrameRead();
        if (r < 0) continue;
        std::snprintf(msg, sizeof(msg), "%s lenG=%d offG=%d L=%d O=%d R=%d: read left the window",
                      tag, c.lenG, c.offG, c.L, c.O, c.R);
        CHECK(SandsRangeGestureCore::windowContains(c.O, c.L, r, RING), msg);
        if (((t) % c.lenG) == 0) {   // the phrase entry (first played step after reset, recurring)
            std::snprintf(msg, sizeof(msg), "%s lenG=%d offG=%d L=%d O=%d R=%d: rotation not at the first-played step (got %d, want %d)",
                          tag, c.lenG, c.offG, c.L, c.O, c.R, r, rot);
            CHECK(r == rot, msg);
        }
    }
}

// After running FORWARD for a while and THEN reversing, the rotation must still land
// at the first-played reverse step — detected directly by stepIndex == endStep (the
// window's upper bound), not by step count. This guards against the from-reset
// anchor being a transient.
static void driveFwdThenRev(const Cfg& c) {
    SequencerEngine eng; eng.reset();
    eng.updateWindow((float)c.lenG, 0.f, false, (float)c.offG, 0.f, false);
    eng.numPolyVoices = 4;
    eng.maskMacroKind_ = SequencerEngine::MASK_CYCLE;
    eng.macroVoiceLor_[SequencerEngine::LOR_LEN] = c.L;
    eng.macroVoiceLor_[SequencerEngine::LOR_OFF] = c.O;
    eng.macroVoiceLor_[SequencerEngine::LOR_ROT] = c.R;
    const int rot     = SandsRangeGestureCore::wrapMod(c.O + (c.R % c.L), RING);
    const int endStep = (c.offG + c.lenG - 1) & (RING - 1);   // the window's upper bound
    char msg[180];
    for (int t = 0; t < c.lenG + 3; ++t) eng.advancePlayhead(+1, nullptr);   // forward first
    int seen = 0;
    for (int t = 0; t < 3 * c.lenG; ++t) {
        eng.advancePlayhead(-1, nullptr);
        if (eng.stepIndex == endStep) {   // a reverse phrase entry
            ++seen;
            std::snprintf(msg, sizeof(msg), "FWD->REV lenG=%d offG=%d L=%d O=%d R=%d: rotation not at endStep=%d (got %d want %d)",
                          c.lenG, c.offG, c.L, c.O, c.R, endStep, eng.macroFrameRead(), rot);
            CHECK(eng.macroFrameRead() == rot, msg);
        }
    }
    std::snprintf(msg, sizeof(msg), "FWD->REV lenG=%d: never hit endStep", c.lenG);
    CHECK(seen >= 1, msg);
}

int main() {
    std::printf("== test_reverse_rotation_anchor ==\n");
    std::printf("gate 1: FORWARD — rotation at the phrase entry (unchanged)\n");
    for (const Cfg& c : kCfgs) driveMacro(c, +1, "FWD");
    std::printf("gate 2: REVERSE — rotation at the first-played (upper-bound) step\n");
    for (const Cfg& c : kCfgs) driveMacro(c, -1, "REV");
    std::printf("gate 3: FORWARD then REVERSE — rotation at endStep after a direction change\n");
    for (const Cfg& c : kCfgs) driveFwdThenRev(c);
    if (g_fail) { std::printf("FAILED: %d check(s)\n", g_fail); return 1; }
    std::printf("OK\n");
    return 0;
}
