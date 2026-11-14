#pragma once
#include "PatternEngine.hpp"   // dotModular::kMaskRing / kPatternLengthCap + the global using block

// PositionResolver — the ONE position authority (DESIGN_POSITION_UNIFICATION.md). SLICE 1 (CC_PROMPT_POSITION_RESOLVER.md):
// a PURE MIRROR — it advances alongside the live systems (stepIndex + globalWalk_) and
// CONSUMES NOTHING; the golden-identity harness (test_position_resolver_identity.cpp)
// proves it reproduces both bit-identically across the sweep. No consumer reads it
// until slice 2; behaviour change is slice 2+ by design.
//
// THE MODEL (design decision 3, the POSITION_HOLDER_MODEL discipline): ONE
// transport-position state — the played column — and every other quantity is a PURE
// view of it + the live parameters:
//   playedColumn()  the state itself (today's stepIndex; will drive output/UI/sync).
//   phrasePos()     UNSIGNED steps since the CURRENT-DIRECTION phrase entry: 0 at the
//                   first-played step in BOTH directions (forward startStep, reverse
//                   endStep), counting up —
//                     wrapMod(dir>0 ? col-startStep : endStep-col, ring).
//                   The coordinate the derive will fold (slice 2) and the rotation
//                   anchor (phrasePos==0, direction-agnostic — slice 5). Forward it
//                   equals globalWalk_ BIT-IDENTICALLY (the harness pins this); in
//                   reverse it is the FIXED relation — the current globalWalk_ drifts
//                   against the reverse entry by a direction- and history-dependent ±1
//                   (the bug test_reverse_rotation_anchor reproduces), this cannot,
//                   because it is defined by the SAME traversal that produces the
//                   played column.
//   direction()     the transport's current direction (the sign of the last motion;
//                   CLOCK keeps it +1 — decision 1). The MULTIPLICATIVE per-level
//                   composition (global x voice-frame x lane, via the engine's
//                   laneTickFor + laneDirSign products) stays in the engine's existing
//                   helpers this slice; slice 2 brings the composition API here when
//                   the holders migrate (computed from this ONE state so no two
//                   consumers can disagree).
// The resolver OWNS the window traversal (decision 2): the start/end bounds, the
// contiguous-window containment and the seeding/skip — the ONE copy of the traversal
// law (today split between stepIndex's seed/skip idiom and globalWalk_'s accumulate).
// The containment test (wrapMod(col-startStep, ring) < len) is isStepInWindow's mask
// test for the contiguous windows setWindow builds ([offset, offset+length)) — folded
// in, not special-cased.
struct PositionResolver {
    // ── the parameters (set by reset/setWindow; not accumulated state) ──
    int startStep_ = 0;
    int endStep_   = kPatternLengthCap - 1;   // the transport default = the full ring (the 16-triage option-C ruling)
    int len_       = kPatternLengthCap;
    int dir_       = +1;                      // the last motion's sign (the traversal + phrasePos direction)

    // ── THE ONE STATE: the played column (today's stepIndex; -1 = the pre-start park) ──
    int col_ = -1;

    static int wrapRing(int v) { return ((v % kMaskRing) + kMaskRing) % kMaskRing; }
    // The contiguous-window containment — == isStepInWindow's mask test for the windows
    // setWindow builds. NOTE: the PARKED column (startStep-1, after setWindow re-anchors
    // a played column out of the new window) is deliberately OUT — it mirrors the engine.
    bool inWindow(int col) const { return wrapRing(col - startStep_) < len_; }

    void reset() {
        startStep_ = 0;
        endStep_ = kPatternLengthCap - 1;
        len_ = kPatternLengthCap;
        dir_ = +1;
        col_ = -1;   // the pre-start park (mirrors stepIndex)
    }
    void setWindow(int length, int offset) {
        startStep_ = offset;
        endStep_ = (offset + length - 1) & (kMaskRing - 1);
        len_ = (length < 1) ? 1 : length;
        // Mirror setWindow's re-anchor: a played column outside the NEW window parks
        // just before its start (the next forward step lands ON the start — the same
        // law the engine applies to stepIndex).
        if (col_ != -1 && !inWindow(col_)) col_ = (startStep_ - 1 + kMaskRing) % kMaskRing;
    }
    // THE ADVANCE — the traversal law verbatim (the seed/step/skip idiom stepIndex
    // uses; the ONE copy once the migration completes). Mirrors advancePlayhead's
    // block exactly: reverse seeds just after endStep so the first reverse step lands
    // ON endStep; forward seeds just before startStep; the skip scans wrap an
    // out-of-window landing back to the traversal's entry bound.
    void advance(int motion) {
        if (motion < 0) {
            dir_ = -1;
            if (col_ == -1) col_ = (endStep_ + 1) & (kMaskRing - 1);
            col_ = (col_ - 1 + kMaskRing) & (kMaskRing - 1);
            if (!inWindow(col_)) col_ = endStep_;
            for (int s = 0; s < kMaskRing && !inWindow(col_); ++s)
                col_ = (col_ - 1 + kMaskRing) & (kMaskRing - 1);
        } else if (motion > 0) {
            dir_ = +1;
            if (col_ == -1) col_ = (startStep_ - 1 + kMaskRing) % kMaskRing;
            col_ = (col_ + 1) & (kMaskRing - 1);
            if (!inWindow(col_)) col_ = startStep_;
            for (int s = 0; s < kMaskRing && !inWindow(col_); ++s)
                col_ = (col_ + 1) & (kMaskRing - 1);
        }
        // motion == 0: a held event — the position does not move (advancePlayhead is
        // only called with ±1; the guard is defence-in-depth).
    }
    // The phase-snap paths (snapToPhaseStep) set the column directly — mirror the
    // FINAL value (after the engine's own window defence-in-depth).
    void snapTo(int col) { col_ = col; }

    // ── the PURE views ──
    int playedColumn() const { return col_; }
    int direction() const { return dir_; }
    int phrasePos() const {
        if (col_ < 0) return 0;   // the pre-start park: no phrase yet
        return (dir_ > 0) ? wrapRing(col_ - startStep_) : wrapRing(endStep_ - col_);
    }
};
