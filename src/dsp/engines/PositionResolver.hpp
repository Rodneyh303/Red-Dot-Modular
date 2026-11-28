#pragma once
#include <concepts>
#include <cstdint>
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
// The WALK LAW'S concept — namespace scope (C++20 concepts cannot be class-scope
// members; the WalkResolver skeleton below is nested in the resolver, the concept
// precedes it). The four concrete policies live in SequencerEngine.hpp next to
// laneTickFor — each declares { tick (via laneTickFor, UNCHANGED), anchorBias,
// entryEdge } and an incomplete policy is a LOCATED compile error at its
// declaration (the doc's payoff: "reverse forgot its right-edge rule" = a compile
// error, not a feel-test discovery).
template<typename P>
concept WalkPolicy = requires(int64_t t, int len) {
    { P::tick(t, len) }   -> std::same_as<int64_t>;
    { P::anchorBias() }   -> std::same_as<int64_t>;
    { P::entryEdge(len) } -> std::same_as<int64_t>;
};

struct PositionResolver {
    // ── the parameters (set by reset/setWindow; not accumulated state) ──
    int startStep_ = 0;
    int endStep_   = kPatternLengthCap - 1;   // the transport default = the full ring (the 16-triage option-C ruling)
    int len_       = kPatternLengthCap;
    int dir_       = +1;                      // the last motion's sign (the traversal + phrasePos direction)
    // (TRUE-RETRACE, Rodney 2026-11-21) the direction of the FIRST motion since reset / a window change: the
    // READ coordinate (readCoord) stays anchored to THIS entry, so reversing the transport mid-phrase walks BACK
    // along the same coordinate (an exact retrace) instead of re-anchoring to the new direction's entry (which
    // returned the voice read one step ahead and by a wrap-dependent offset). 0 = no motion yet. phrasePos()
    // (the phrase EDGE predicate) is deliberately UNCHANGED — it keeps meaning "the current-direction entry".
    int entryDir_  = 0;

    // ── THE ONE STATE: the played column (today's stepIndex; -1 = the pre-start park) ──
    int col_ = -1;
    // (SLICE 2b — the PHASE-MODE phrase override) phase mode does not traverse, it
    // SNAPS, and its phrase entries are the CYCLE-0 crossings (phase 0% AND 100% —
    // both land on cycle 0), NOT the gate traversal's boundary crossings: the raw
    // phrase position is fed BY THE SNAP (wrapMod(absStep, lenG) forward — == wG,
    // bit-identical; wrapMod(-absStep, lenG) backward — the steps back to the cycle-0
    // entry, the walk R, R-1, ... with the jump back to R LAST). The override is
    // cleared by advance()/setWindow()/reset() — the traversal re-derives the phrase
    // from the column in gate/clock mode.
    int  phraseOverride_    = 0;
    bool phraseOverridden_  = false;

    // (NET-STEPS MODEL — Rodney 2026-11-25) a candidate single coordinate: netSteps_ is the net
    // signed displacement since reset (+1/-1 per step, 0 on hold); netEntered_ makes the first
    // motion the entry (read at netSteps_==0). The read is (netSteps_ + R) mod L — no second
    // coordinate, no re-park. Parallel to col_ for now; pinned against the engine in
    // test_netsteps_model. See CC_PROMPT_POSITION_RESOLVER_C3_UNIT_OF_ABSOLUTENESS.
    int64_t netSteps_   = 0;
    bool    netEntered_ = false;

    static int wrapRing(int v) { return ((v % kMaskRing) + kMaskRing) % kMaskRing; }
    // Negative-safe mod (mirrors SequencerEngine::wrapMod's body verbatim — unreachable
    // from here by include order; the identity harness pins the equality).
    static int wrapModTick_(int64_t v, int m) {
        int64_t r = v % m;
        if (r < 0) r += m;
        return (int)r;
    }
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
        entryDir_ = 0;
        phraseOverridden_ = false;   // the traversal owns the phrase from here
        netSteps_ = 0; netEntered_ = false;   // (NET-STEPS MODEL) reset is the natural boundary — reverse is not carried across it
    }
    void setWindow(int length, int offset) {
        if (offset != startStep_ || ((length < 1) ? 1 : length) != len_) entryDir_ = 0;   // a REAL window change re-anchors the read coordinate
        startStep_ = offset;
        endStep_ = (offset + length - 1) & (kMaskRing - 1);
        len_ = (length < 1) ? 1 : length;
        // Mirror setWindow's re-anchor: a played column outside the NEW window parks
        // just before its start (the next forward step lands ON the start — the same
        // law the engine applies to stepIndex).
        if (col_ != -1 && !inWindow(col_)) col_ = (startStep_ - 1 + kMaskRing) % kMaskRing;
        phraseOverridden_ = false;   // a window change invalidates the window-relative phrase
    }
    // THE ADVANCE — the traversal law verbatim (the seed/step/skip idiom stepIndex
    // uses; the ONE copy once the migration completes). Mirrors advancePlayhead's
    // block exactly: reverse seeds just after endStep so the first reverse step lands
    // ON endStep; forward seeds just before startStep; the skip scans wrap an
    // out-of-window landing back to the traversal's entry bound.
    void advance(int motion) {
        if (motion != 0) phraseOverridden_ = false;   // the traversal re-derives the phrase (a held event keeps it)
        if (motion != 0 && entryDir_ == 0) entryDir_ = (motion < 0) ? -1 : +1;
        // (NET-STEPS MODEL — Rodney 2026-11-25) a single signed counter: the FIRST motion since
        // reset only enters (the read lands at the entry, netSteps_==0); every later motion
        // displaces it. No seed/skip/re-park and no re-arm on a window change — the whole point:
        // it stays drift-free across a mid-run window re-author. read = (netSteps_ + R) mod L.
        if (motion != 0) { if (!netEntered_) netEntered_ = true; else netSteps_ += motion; }
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
    // FINAL value (after the engine's own window defence-in-depth) — AND feed the
    // sweep direction + the raw phrase position (SLICE 2b: phase direction is known
    // only between two consecutive PPQN quantisations, i.e. per snap as the sign of
    // the absStep change; the phrase entries are the cycle-0 crossings).
    void snapTo(int col, int dir, int phrasePos) {
        col_ = col;
        dir_ = (dir < 0) ? -1 : +1;
        phraseOverride_ = phrasePos;
        phraseOverridden_ = true;
    }

    // ── the PURE views ──
    int playedColumn() const { return col_; }
    int direction() const { return dir_; }
    int64_t netSteps() const { return netSteps_; }   // (NET-STEPS MODEL) the single signed counter; read = (netSteps()+R) mod L
    int phrasePos() const {
        if (phraseOverridden_) return phraseOverride_;   // the phase snap's raw phrase
        if (col_ < 0) return 0;   // the pre-start park: no phrase yet
        return (dir_ > 0) ? wrapRing(col_ - startStep_) : wrapRing(endStep_ - col_);
    }
    // (TRUE-RETRACE) THE SIGNED READ COORDINATE the voice/macro reads fold: the transport-signed steps since the
    // ENTRY-direction phrase entry — +(col-start) when the traversal was entered forward, -(end-col) when entered
    // in reverse — NOT re-anchored by a later mid-phrase reversal. Equals the old (dir>0 ? phrasePos : -phrasePos)
    // whenever the current direction IS the entry direction (every forward-only and every fresh-reverse run —
    // byte-identical there); after a flip it keeps walking the SAME coordinate backwards (exact retrace).
    // The phase-snap override keeps its own raw phrase (unchanged).
    int64_t readCoord() const {
        if (phraseOverridden_) return (dir_ > 0) ? (int64_t)phraseOverride_ : -(int64_t)phraseOverride_;
        if (col_ < 0) return 0;
        const int e = (entryDir_ != 0) ? entryDir_ : dir_;
        return (e > 0) ? (int64_t)wrapRing(col_ - startStep_) : -(int64_t)wrapRing(endStep_ - col_);
    }
    // (SLICE 5 — CC_PROMPT_POSITION_RESOLVER_SLICE5_PHASECLOCK, option (a)) The abs-mode
    // read coordinate: the transport's absolute phase in the window, ≡ the retired
    // globalWalk_ (wrapMod(tick − 1, len)). A VIEW (no stored state) — len_ is the
    // resolver's own, transportTick the engine's ONE clock. The invariant was proven
    // BEFORE the deletion (the identity harness, phase 1): the accumulator's three write
    // sites (the setWindow re-bind, the anchorTransport seed, the tickHolders increment)
    // all leave it ≡ this closed form, so it carries no state the clock lacks.
    // (b) PARKED: this is the TICK phase, which SURVIVES a window re-author (re-bound
    // from the tick, not reset to the window top); it is NOT the column — redefining
    // abs on playedColumn() is a behaviour change gated on the C3 holder model.
    int phaseClock(int64_t transportTick) const { return wrapModTick_(transportTick - 1, len_); }

    // ── POSITION RESOLVER CONSOLIDATION (POSITION_RESOLVER_CONSOLIDATION.md) ─────

    // The PRIMITIVES face of the slot law (the runtime callers — the strip feeds
    // carry a runtime direction; the bias is SequencerEngine::anchorBias(dir), the
    // SAME constant the read law folds). Moved verbatim from SandsRangeGestureCore
    // (where T1 landed it) — the gesture core stays rack-free; the position law
    // lives HERE (the doc's item 4: the content↔slot mapping must be the SAME
    // mapping the engine read uses, served from one place).
    static int64_t playheadSlotBar(int64_t readCell, int len, int64_t off, int64_t rot,
                                   int64_t reverseBias, int ring) {
        const int64_t l = (len < 1) ? 1 : len;
        const int64_t o = ((off % ring) + ring) % ring;
        // THE IN-WINDOW FRAME VALUE — recover wnow = (readCell − o) mod ring, which is
        // in [0, len) because frameReadCell guarantees the read cell IS in the window.
        // (The bridge test caught the T1-shipped pre-wrap form — wrapMod(readCell,
        // ring) FIRST, then subtract — drifting on RING-WRAPPING windows whose len
        // does not divide the ring (O=31, L=3: the ring wrap shifts by 32 ≢ 0 mod 3,
        // so the entry slot landed one box off). The recovery form is total: for
        // non-wrapping windows it is bit-identical to the T1 expression (the T1
        // test's O {0,2} sweep is unchanged), and for wrapping windows it is the
        // correct box-offset — the latent T1 defect the consolidation's bridge
        // exposed, fixed here.)
        int64_t wnow = ((readCell - o) % ring + ring) % ring;
        int64_t m = (wnow - rot - reverseBias) % l;
        if (m < 0) m += l;
        int64_t b = (o + m) % ring;
        if (b < 0) b += ring;
        return b;
    }

    // (1) THE SIGNED PHRASE COORDINATE — the transport-signed phrase position plus
    // the per-voice advance offset, mod the current global length. Byte-identical
    // re-expression of voiceFrameEdgeLive_'s inlined vCoord
    // (wrapMod((dir>0 ? phrasePos : -phrasePos) + advanceOffset, lenG)) — the
    // coordinate the commit edge and the reads fold; one definition instead of a
    // re-derivation per site (the doc's item 1).
    static int64_t signedPhraseCoord(int64_t phrasePos, int dirSign, int64_t advanceOffset, int64_t lenG) {
        const int64_t s = (dirSign > 0) ? phrasePos : -phrasePos;
        int64_t v = (s + advanceOffset) % lenG;
        if (v < 0) v += lenG;
        return v;
    }

    // ── (2) THE WALK LAW'S concept + skeleton — the SandsModifierPolicy pattern's
    // ── true transfer (the doc's ruling: the CORE has no concept shape — ONE
    // ── traversal law — but the STRATEGY AXES do). PositionResolver cannot name
    // ── LaneDir or laneTickFor (SequencerEngine includes THIS header — the types
    // ── live there), so the concept is GENERIC over the policy and the four
    // ── concrete policies live in SequencerEngine.hpp right next to laneTickFor.
    // ── Each policy declares
    // ──   { tick (via SequencerEngine::laneTickFor, UNCHANGED — the goldens depend
    // ──     on it), anchorBias (7aae5ee9's reverse-only read anchor), entryEdge
    // ──     (the direction-entry box-offset: LEFT 0 forward / RIGHT len-1 reverse) }
    // ── and WalkResolver<Policy> folds rotation / offset / box-offset /
    // ── content↔slot ONCE. An incomplete policy is a LOCATED compile error at its
    // ── declaration — "reverse forgot its right-edge rule" becomes a compile error
    // ── instead of a feel-test discovery, and anchorBias stops being a ~10-site
    // ── fan-out and becomes one declared member of the reverse policy.
    // ── BYTE-IDENTICAL BRIDGE: every value here is a pure re-expression of the
    // ── shipped inline formulas; test_position_consolidation_bridge sweeps the full
    // ── (dir × tick × len × rot × off) grid pinning bit-identity, and the battery
    // ── passing unchanged is the audio-safety proof (the reads flow through the
    // ── migrated sites).
    template<WalkPolicy P>
    struct WalkResolver {
        // THE ONE FOLD — the rotation-anchored read's frame value (the walk + the
        // anchor bias). The ~10-site fan-out's core (`laneTickFor(d,t,len) +
        // anchorBias(d)`), defined ONCE; the sites call the engine's runtime
        // dispatcher walkFold, which lands here per policy.
        static int64_t frameValue(int64_t t, int len) { return P::tick(t, len) + P::anchorBias(); }
        // The direction-ENTRY edge of a len window as a BOX-OFFSET: the commit
        // edge's wEdge AND the playhead slot's entry edge — one definition (was
        // re-expressed in voiceFrameEdgeLive_ and re-derived by every new surface).
        static int64_t entryEdge(int len) { return P::entryEdge(len); }
        // The CONTENT↔SLOT mapping (CC_PROMPT_REVERSE_VOICE_ROTATION_EDGE): the
        // drawn playhead bar = the read cell unmixed of the rotation fold and the
        // anchor bias, so the sweep ENTERS at P's entry edge reading rotation
        // content R for EVERY R. The reverse policy's +1 IS its anchorBias() — the
        // position twin of the read anchor; "reverse forgot its right-edge rule" is
        // now a compile error.
        static int64_t playheadSlotBar(int64_t readCell, int len, int64_t off, int64_t rot, int ring) {
            return PositionResolver::playheadSlotBar(readCell, len, off, rot, P::anchorBias(), ring);
        }
    };

    // ── (3) THE PHASE-HOLDER SKELETON — CA5 (CONCEPT_ODR_AUDIT CA5; ──
    // ── CC_PROMPT_REVERSE_COMPLETIONS_THEN_CA5 Part 2). "Maintain a bounded walk ──
    // ── PHASE congruent to a driver coordinate, re-seed when the walk KEY changes" ──
    // ── was written out FIVE times in tickHolders (the global walk, the Macro row's ──
    // ── frame, the voice frames, the per-voice lanes, the mono lanes) and re-rolled ──
    // ── in anchorTransport's re-anchor path — ONE law now. The KEY arrives OPAQUE ──
    // ── (the engine's walkKeyOf(dir, len), or the global walk's bare length): the ──
    // ── skeleton does not know what is in it, so the KNOWN incompleteness ──
    // ── (walkKeyOf omits the parent cycle — the CA5 blind spot) stays the engine's ──
    // ── KEY POLICY, deliberately unchanged by this extraction (the gated, ──
    // ── reproduce-first NEXT step owns it — no behaviour change here). ──
    // ──    advance: same key → phase + k (the driver's raw delta); key change → ──
    // ──              the closed form at the driver's t0 (the re-anchor law). ──
    // ──    reseed:  the anchor IS a re-entry — the closed form unconditionally. ──
    // ── BYTE-IDENTICAL BRIDGE: both forms are the shipped inline ternaries moved ──
    // ── verbatim (wrapModTick_ == the engine's wrapMod, pinned); ──
    // ── test_phase_holder_bridge sweeps the closed-form grid AND the engine marches ──
    // ── (retrace, free-run, stop, rewind, the anchor) pinning the holders' congruence ──
    // ── phase ≡ t0 (mod period); the battery passing unchanged is the proof. ──
    struct PhaseHolder {
        // tickHolders' law. Maintains the stored key; returns the new phase.
        static int advance(int& storedKey, int phase, int key, int64_t t0, int64_t k, int period) {
            const int p = (storedKey != key)
                ? wrapModTick_(t0, period)
                : wrapModTick_(phase + k, period);
            storedKey = key;
            return p;
        }
        // anchorTransport's law (the re-anchor path). Maintains the stored key.
        static int reseed(int& storedKey, int key, int64_t t0, int period) {
            storedKey = key;
            return wrapModTick_(t0, period);
        }
    };
};

