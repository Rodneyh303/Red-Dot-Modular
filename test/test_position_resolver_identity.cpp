// test/test_position_resolver_identity.cpp — DESIGN_POSITION_UNIFICATION SLICE 1:
// the GOLDEN-IDENTITY HARNESS. The Fullerton resolver is a PURE MIRROR — advanced
// alongside the live systems (stepIndex + globalWalk_), consumed by NOTHING. This
// proves it reproduces both bit-identically across the sweep, so slice 2's migration
// (the derive reads phrasePos; test_reverse_rotation_anchor goes green) starts from a
// proven-equivalent base.
//
// GATES:
// 1. playedColumn() == stepIndex at EVERY step — every window, every direction program
//    (forward, reverse, forward-then-reverse, reverse-then-forward-then-reverse),
//    from reset, across mid-run direction flips, across mid-run setWindow re-authors,
//    and after the phase snap.
// 2. phrasePos() vs globalWalk_ in the FORWARD segments — the MEASURED relation (the
//    first draft asserted plain equality and the rev->fwd->rev program FAILED it —
//    the prompt's stop-and-understand case, now understood and pinned exactly):
//      * No reverse since the anchor: phrasePos == globalWalk_ BIT-IDENTICALLY (the
//        from-reset forward identity — the invariant (col-startStep) == globalWalk_
//        moves +1 on both sides).
//      * After reverse history: phrasePos == globalWalk_ + 1 (mod len) — CONSTANT.
//        The from-reset reverse SEED (stepIndex=-1 -> seed endStep+1 -> land on
//        endStep) puts the entry at globalWalk_ == len-2 while the column is at
//        endStep: a +1 offset that BOTH accumulators then preserve through every
//        +/-1 motion and every wrap (their difference mod len is invariant). The
//        offset is HISTORY-DEPENDENT (the design's thesis, now measured): it does
//        not exist until a reverse-from-parked occurs, and it never goes away.
//        This is the drift slice 2 removes (the derive reads the traversal-tied
//        phrasePos; globalWalk_'s raw congruence stops being folded).
// 3. phrasePos() == the direction-aware offset computed from the ENGINE's own bounds
//    (startStep/endStep) — the resolver's copies cannot drift from the engine's.
// 4. The entry law: phrasePos == 0 EXACTLY when stepIndex == (dir>0 ? startStep :
//    endStep) — the direction-stable truth (the oracle's gate-3 key).
// 5. The LaneDir matrix (voice x lane, incl. Reverse x Reverse and Pendulum x
//    PingPong) does NOT disturb the transport mirror — and the composed lane direction
//    follows the MULTIPLICATIVE rule (R x R walks the same way as F x F; F x R the
//    opposite) — pinned through the existing reads.
// 6. CLOCK mode (modeSelect=0): the global forward-only; the mirror holds.
// 7. The DEFAULT config (post-reset, no updateWindow — the cap window): the mirror
//    holds.
//
// FINDING (documented, not pinned): a mid-run setWindow re-author re-bounds
// globalWalk_ unilaterally ((tick-1) mod newLen) while the played column keeps its
// place (or parks) — a THIRD desync class beyond the reverse one (which the oracle
// reproduces). The re-author march therefore pins only the playedColumn mirror;
// slice 2's migration (the derive reads the traversal-tied phrasePos) resolves this
// class with the reverse one.

#include <cstdio>
#include "engines/SequencerEngine.hpp"
#include "VoiceResolver.hpp"

using dotModular::VoiceResolver;

static int g_pass = 0, g_fail = 0;
static void check(bool ok, const char* what) {
    if (ok) { ++g_pass; }
    else    { ++g_fail; std::printf("  FAIL: %s\n", what); }
}

struct WinCfg { int len, off; };
static const WinCfg kWins[] = {
    {16, 0}, {8, 4}, {12, 5}, {10, 3}, {32, 0}, {7, 0},
};

struct Seg { int dir, n; };
struct Prog { const char* tag; Seg segs[3]; };
static const Prog kProgs[] = {
    {"fwd",            {{+1, 40}, { 0, 0}, { 0, 0}}},
    {"rev",            {{-1, 40}, { 0, 0}, { 0, 0}}},
    {"fwd->rev",       {{+1, 13}, {-1, 27}, { 0, 0}}},
    {"rev->fwd->rev",  {{-1,  7}, {+1, 21}, {-1, 15}}},
};

// One march: fresh engine + the window + the program; assert the mirror at EVERY step.
static void march(const WinCfg& w, const Prog& p, bool clockMode) {
    SequencerEngine eng; eng.reset();
    if (clockMode) eng.modeSelect = 0;
    eng.updateWindow((float)w.len, 0.f, false, (float)w.off, 0.f, false);
    char msg[200];
    bool colOK = true, fwdOK = true, defOK = true, entryOK = true, rangeOK = true;
    bool reverseHistory = false;
    for (int s = 0; s < 3 && p.segs[s].n > 0; ++s) {
        const int dir = p.segs[s].dir;
        if (clockMode && dir < 0) break;   // CLOCK's global is forward-only (decision 1)
        if (dir < 0) reverseHistory = true;
        for (int k = 0; k < p.segs[s].n; ++k) {
            eng.advancePlayhead(dir, nullptr);
            const int col = eng.pos_.playedColumn();
            const int pp  = eng.pos_.phrasePos();
            if (col != eng.stepIndex) colOK = false;
            if (dir > 0) {
                const int diff = ((pp - eng.globalWalk_) % w.len + w.len) % w.len;
                if (!reverseHistory) { if (pp != eng.globalWalk_) fwdOK = false; }
                else                 { if (diff != 1) fwdOK = false; }   // the measured +1 (mod len), constant
            }
            // the definition, computed from the ENGINE's own bounds (the resolver's
            // copies cannot drift from the engine's)
            const int want = (eng.pos_.direction() > 0)
                ? Fullerton::wrapRing(col - eng.startStep)
                : Fullerton::wrapRing(eng.endStep - col);
            if (pp != want) defOK = false;
            // the entry law: pp == 0 exactly at the direction's entry column
            const int entryCol = (eng.pos_.direction() > 0) ? eng.startStep : eng.endStep;
            if ((pp == 0) != (col == entryCol)) entryOK = false;
            if (col >= 0 && (pp < 0 || pp >= eng.cachedLength)) rangeOK = false;
        }
    }
    std::snprintf(msg, sizeof msg, "%s%s len=%d off=%d: playedColumn() == stepIndex at every step",
                  clockMode ? "clock " : "", p.tag, w.len, w.off);
    check(colOK, msg);
    std::snprintf(msg, sizeof msg, "%s%s len=%d off=%d: phrasePos()==globalWalk_ forward (exact pre-reverse; +1 mod len after reverse history — the measured seed offset)",
                  clockMode ? "clock " : "", p.tag, w.len, w.off);
    check(fwdOK, msg);
    std::snprintf(msg, sizeof msg, "%s%s len=%d off=%d: phrasePos() == the engine-bounds offset definition",
                  clockMode ? "clock " : "", p.tag, w.len, w.off);
    check(defOK, msg);
    std::snprintf(msg, sizeof msg, "%s%s len=%d off=%d: phrasePos==0 exactly at the entry column (startStep fwd / endStep rev)",
                  clockMode ? "clock " : "", p.tag, w.len, w.off);
    check(entryOK, msg);
    std::snprintf(msg, sizeof msg, "%s%s len=%d off=%d: phrasePos in [0, len) while playing",
                  clockMode ? "clock " : "", p.tag, w.len, w.off);
    check(rangeOK, msg);
}

// GATE 5 — the LaneDir matrix: the per-level dirs must NOT disturb the transport
// mirror, and the composed lane direction follows the multiplicative rule.
static void matrixMarch() {
    struct { SequencerEngine::LaneDir voice, lane; const char* tag; } const set[] = {
        {SequencerEngine::LaneDir::Forward,  SequencerEngine::LaneDir::Forward,  "F x F"},
        {SequencerEngine::LaneDir::Forward,  SequencerEngine::LaneDir::Reverse,  "F x R"},
        {SequencerEngine::LaneDir::Reverse,  SequencerEngine::LaneDir::Forward,  "R x F"},
        {SequencerEngine::LaneDir::Reverse,  SequencerEngine::LaneDir::Reverse,  "R x R"},
        {SequencerEngine::LaneDir::Pendulum, SequencerEngine::LaneDir::PingPong, "P x PP"},
    };
    int ff = 0, fr = 0, rf = 0, rr = 0;
    for (const auto& m : set) {
        SequencerEngine eng; eng.reset();
        eng.updateWindow(16.f, 0.f, false, 0.f, 0.f, false);
        eng.numPolyVoices = 2;
        eng.macroVoiceLorDir_ = m.voice;                                   // the voice-frame level
        eng.laneDir_[0] = m.lane; eng.laneDirPending_[0] = m.lane;         // the mono lane level
        eng.macroLaneDir_[0] = m.lane;                                     // the macro lane level
        char msg[120];
        bool colOK = true;
        int  firstDelta = 0; bool deltaSignConsistent = true;
        int  prev = 0;
        for (int k = 0; k < 30; ++k) {
            eng.advancePlayhead(+1, nullptr);
            colOK = colOK && eng.pos_.playedColumn() == eng.stepIndex;
            if (k == 8) prev = eng.macroLaneTick_[0];
            if (k > 8 && k <= 14) {
                const int d = eng.macroLaneTick_[0] - prev;
                if (k == 9) firstDelta = (d > 0) - (d < 0);
                else if (((d > 0) - (d < 0)) != firstDelta || firstDelta == 0) deltaSignConsistent = false;
                prev = eng.macroLaneTick_[0];
            }
        }
        for (int k = 0; k < 20; ++k) {
            eng.advancePlayhead(-1, nullptr);
            colOK = colOK && eng.pos_.playedColumn() == eng.stepIndex;
        }
        std::snprintf(msg, sizeof msg, "MATRIX %s: the dirs do not disturb the mirror (col == stepIndex)", m.tag);
        check(colOK, msg);
        std::snprintf(msg, sizeof msg, "MATRIX %s: the composed lane direction is a consistent sign (+1/-1 per step)", m.tag);
        check(deltaSignConsistent, msg);
        if      (m.tag[0] == 'F' && m.tag[4] == 'F') ff = firstDelta;
        else if (m.tag[0] == 'F' && m.tag[4] == 'R') fr = firstDelta;
        else if (m.tag[0] == 'R' && m.tag[4] == 'F') rf = firstDelta;
        else if (m.tag[0] == 'R' && m.tag[4] == 'R') rr = firstDelta;
    }
    // THE MULTIPLICATIVE RULE: the double-reverse composes to forward; a single
    // reverse flips it — the per-level signs are a PRODUCT, never a single flag.
    check(ff != 0 && ff == rr,
          "MATRIX: F x F and R x R walk the SAME way (the double-reverse composes to forward)");
    check(fr != 0 && fr == rf && fr != ff,
          "MATRIX: F x R and R x F walk the OPPOSITE way (a single reverse flips it)");
}

int main() {
    std::printf("== test_position_resolver_identity (slice 1: the pure mirror) ==\n");

    // GATES 1-4: the sweep — every window x every program
    for (const WinCfg& w : kWins) {
        for (const Prog& p : kProgs) march(w, p, /*clockMode=*/false);
    }
    // GATE 6: CLOCK mode — the global forward-only (the reverse segments skipped)
    for (const WinCfg& w : kWins) march(w, kProgs[0], /*clockMode=*/true);
    // GATE 7: the DEFAULT config (post-reset, no updateWindow — the cap window)
    {
        SequencerEngine eng; eng.reset();
        bool colOK = true, fwdOK = true;
        for (int k = 0; k < 40; ++k) {
            eng.advancePlayhead(+1, nullptr);
            colOK  = colOK && eng.pos_.playedColumn() == eng.stepIndex;
            fwdOK  = fwdOK && eng.pos_.phrasePos() == eng.globalWalk_;
        }
        check(colOK, "DEFAULT (the cap window, no re-author): playedColumn() == stepIndex");
        check(fwdOK, "DEFAULT: phrasePos() == globalWalk_ forward");
    }
    // The mid-run setWindow RE-AUTHOR: the col mirror holds through the park/re-anchor
    // (the globalWalk_ equivalence does NOT — the documented third desync class; slice
    // 2's migration resolves it with the reverse one).
    {
        SequencerEngine eng; eng.reset();
        eng.updateWindow(16.f, 0.f, false, 0.f, 0.f, false);
        bool colOK = true;
        for (int k = 0; k < 10; ++k) {
            eng.advancePlayhead(+1, nullptr);
            colOK = colOK && eng.pos_.playedColumn() == eng.stepIndex;
        }
        eng.updateWindow(8.f, 0.f, false, 5.f, 0.f, false);   // 16@0 -> 8@5 mid-run
        for (int k = 0; k < 14; ++k) {
            eng.advancePlayhead(+1, nullptr);
            colOK = colOK && eng.pos_.playedColumn() == eng.stepIndex;
        }
        eng.updateWindow(12.f, 0.f, false, 3.f, 0.f, false);  // then 12@3, and reverse
        for (int k = 0; k < 14; ++k) {
            eng.advancePlayhead(-1, nullptr);
            colOK = colOK && eng.pos_.playedColumn() == eng.stepIndex;
        }
        check(colOK, "REAUTHOR: the mirror holds across mid-run setWindow re-authors (the park + re-anchor)");
    }
    // The phase snap (snapToPhaseStep): the mirror follows the directly-set column
    {
        SequencerEngine eng; eng.reset();
        eng.updateWindow(16.f, 0.f, false, 0.f, 0.f, false);
        for (int k = 0; k < 10; ++k) eng.advancePlayhead(+1, nullptr);
        eng.snapToPhaseStep(48, 24);
        bool ok = eng.pos_.playedColumn() == eng.stepIndex;
        for (int k = 0; k < 6; ++k) {
            eng.advancePlayhead(+1, nullptr);
            ok = ok && eng.pos_.playedColumn() == eng.stepIndex;
        }
        check(ok, "SNAP: the mirror follows the phase snap (and continues after it)");
    }
    // GATE 5: the LaneDir matrix
    matrixMarch();

    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
