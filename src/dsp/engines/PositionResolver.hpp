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
    // (NET-STEPS, Rodney 2026-11-25 — supersedes TRUE-RETRACE's entryDir_) THE read coordinate's state: one
    // signed counter of NET motion since reset. +1 forward, -1 back, 0 for a held event (the position does not
    // move, so the count does not either). The FIRST motion only ENTERS — the read lands AT the entry, so the
    // counter is 0 there and every later motion accumulates. netEntered_ marks that entry as taken.
    //
    // WHY THIS REPLACED entryDir_. readCoord used to sign its magnitude by the direction of the FIRST motion
    // (entryDir_), which made it a pure function of the played COLUMN and gave "same column -> same cell" for a
    // whole run — a RETRACE that held even after the traversal walked back PAST its own entry. That cost a bit of
    // hidden entry history, and it could only ever anchor ONE entry point. readCoord is now history-INDEPENDENT:
    // past the entry it reads exactly what a COLD start in that direction would read. The architecture already
    // draws this line — the model is stateless by default, and where genuine history replay IS wanted it is paid
    // for explicitly and sized (MonsoonChangeAlleyV2's true-reverse trajectory buffer), never smuggled in as a bit.
    //
    // The sign falls out of the counter, so there is no direction field here: C++ truncated % keeps the remainder's
    // sign on the dividend, which is what reproduces a cold reverse (negative) AND a forward run (positive). A
    // FLOORED mod would force the remainder positive and destroy cold reverse — see readCoord().
    int64_t netSteps_   = 0;
    bool    netEntered_ = false;

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

    // (NET-STEPS MODEL — Rodney 2026-11-25; see also the block above) the single signed
    // coordinate; pinned against the engine in test_netsteps_model and the atPhraseEdge
    // predicate below. See CC_PROMPT_POSITION_RESOLVER_C3_UNIT_OF_ABSOLUTENESS.

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
        netSteps_ = 0; netEntered_ = false;   // (NET-STEPS) reset is the coordinate's only boundary
        phraseOverridden_ = false;   // the traversal owns the phrase from here
        netSteps_ = 0; netEntered_ = false;   // (NET-STEPS MODEL) reset is the natural boundary — reverse is not carried across it
    }
    void setWindow(int length, int offset) {
        const bool realChange = (offset != startStep_) || (((length < 1) ? 1 : length) != len_);
        startStep_ = offset;
        endStep_ = (offset + length - 1) & (kMaskRing - 1);
        len_ = (length < 1) ? 1 : length;
        // Mirror setWindow's re-anchor: a played column outside the NEW window parks
        // just before its start (the next forward step lands ON the start — the same
        // law the engine applies to stepIndex).
        if (col_ != -1 && !inWindow(col_)) col_ = (startStep_ - 1 + kMaskRing) % kMaskRing;
        phraseOverridden_ = false;   // a window change invalidates the window-relative phrase
        // (NET-STEPS) A REAL window change RE-SEEDS the counter to the column's phase in the NEW window —
        // the signed phrasePos, which is exactly what readCoord returned here under the old entryDir_ law
        // (cleared entryDir_ fell back to dir_, giving +(col-start) forward / -(end-col) reverse).
        //
        // WHY THIS IS KEPT, when the past-entry retrace was not. The engine reads the position through TWO
        // faces: the READS fold readCoord (sites 861/1301/1419) and the EDGES — the draw edge, the LOR
        // latches and the lane-offset COMMIT gate (sites 506/837/939/1030/1227) — test phrasePos(), which is
        // still COLUMN-derived. Under the old law both faces were column-derived, so a window re-author moved
        // them together. Let the counter run free through a re-author and the two faces DESYNC: the commit
        // fires at a moment the reads no longer call a wrap (measured — test_bug6_commit_wrap_guard scenes
        // B/C, both global-length modulation, and test_global_dip_differential's 16->4->16 dip). That is the
        // edge-vs-read desync class this engine has been chasing, not a behaviour choice, so the re-anchor
        // stays until the edges are moved onto the same coordinate (the resolver-ownership follow-up).
        //
        // Retiring entryDir_ is UNAFFECTED by this: that bit governed the retrace SIGN past the entry, which
        // is a different concern from where a window re-author re-seeds. History-independence past the entry
        // — the point of the net-steps swap — is intact.
        // A REAL window change RE-SEEDS the counter to the column's phase in the NEW window — the signed
        // phrasePos, which is what readCoord returned here under the old entryDir_ law.
        //
        // WHY THIS IS STILL HERE, after the edges moved onto the counter. C2 rules that the walk should
        // KEEP COUNTING through a re-author and fold mod the new len_ ("global 8->4 at step 8 puts the next
        // boundaries at 12 AND 16"), which is what a free-running counter does — so this re-seed is the
        // thing standing between us and that ruling. It cannot go yet: position is read through a THIRD
        // face this swap has not touched. The per-voice voiceFramePhase_[v] holders — the probability-lane
        // latch level (parent = the voice, CC_PROMPT_CADENCE_C1_LOR_PHRASE_LATCH) — are still window-
        // anchored, so letting the global counter run free desyncs the lane commits from them: measured,
        // test_global_dip_differential's 16->4->16 dip fails 173 checks with this re-seed removed, all of
        // the form "no commit off the live anchor". Moving those holders onto the counter is the remaining
        // step; until then the re-seed keeps the three faces in step.
        //
        // The PARKED case re-ARMS rather than seeds. When the window shrinks, the line above parks an
        // out-of-window column just BEFORE the new start — a position no phase describes. Seeding from it
        // would capture that nonsense phase (e.g. 31) and accumulate from it, where the old column-derived
        // law re-derived each step and landed at 0 on the next motion.
        // RETIRE-RESEED: the in-window re-SEED is GONE — netSteps_ keeps counting through a
        // re-author and folds mod the new len_ (the C2 ruling: "global 8->4 at step 8 puts the
        // next boundaries at 12 AND 16"). It is safe to drop now that the COMMIT EDGE reads the
        // COLUMN (voiceFrameEdgeLive_: phrasePos / phrasePos-signed), not readCoord — so the read
        // and the edge no longer need forcing into congruence. The PARKED re-ARM stays: an
        // out-of-window column has no phase to count from, so it anchors at 0 (next motion enters).
        if (realChange) {
            if (col_ < 0 || !inWindow(col_)) { netSteps_ = 0; netEntered_ = false; }
        }
    }
    // THE ADVANCE — the traversal law verbatim (the seed/step/skip idiom stepIndex
    // uses; the ONE copy once the migration completes). Mirrors advancePlayhead's
    // block exactly: reverse seeds just after endStep so the first reverse step lands
    // ON endStep; forward seeds just before startStep; the skip scans wrap an
    // out-of-window landing back to the traversal's entry bound.
    void advance(int motion) {
        if (motion != 0) phraseOverridden_ = false;   // the traversal re-derives the phrase (a held event keeps it)
        // (NET-STEPS) the FIRST motion only ENTERS — the read lands AT the entry (netSteps_ == 0) — and every
        // later motion accumulates. A held event (motion == 0) does not move the position, so it does not count.
        // (entryDir_ is RETIRED — superseded by the history-independent netsteps readCoord; see the model note above.)
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
        // (NET-STEPS) A snap is a POSITION AUTHORITY, so it re-seeds the counter to the phrase it just
        // asserted — the same signed value readCoord returns while the override stands. Without this the
        // counter never learns about the snap, and the first clocked advance() (which clears the override)
        // would silently resume from the pre-snap count: the phase-drive -> clock handover would jump.
        // Under the old col_-derived law the snap persisted for free, because the snap set col_.
        netSteps_   = (dir_ > 0) ? (int64_t)phrasePos : -(int64_t)phrasePos;
        netEntered_ = true;   // the snap IS the entry: the next motion accumulates from here
    }

    // ── the PURE views ──
    int playedColumn() const { return col_; }
    int direction() const { return dir_; }
    // (NET-STEPS) the raw count, for tests/diagnostics — readCoord() is the folded view consumers read.
    int64_t netSteps() const { return netSteps_; }

    // (NET-STEPS, step 3) THE PHRASE EDGE on the SAME coordinate the reads fold. Mirrors phrasePos()==0's
    // guards exactly; the body is the counter instead of the column.
    //
    // WHY THIS EXISTS. The engine read position through two faces: the READS fold readCoord (the counter)
    // and the EDGES — draw edge, LOR latches, the lane-offset COMMIT gate — tested phrasePos()==0 (the
    // column). Two faces means they can disagree, which is the edge-vs-read desync class this engine has
    // chased for weeks (a commit firing where the reads no longer call a wrap).
    //
    // The boundary stays LIVE: len_ is read HERE, every call, so a mid-phrase length change moves the next
    // boundary immediately — the C1 live-boundary rule, and C2's keep-counting (global 8->4 at step 8 puts
    // boundaries at 12 AND 16, measured). Nothing is precomputed and nothing goes stale, so the emergent
    // cadence property survives: each level still asks its parent "are you at your wrap THIS step?".
    //
    // MEASURED vs phrasePos()==0: bit-identical on every pure run, forward and cold reverse, and through a
    // mid-phrase window change. They diverge only after a mid-run DIRECTION FLIP, where phrasePos re-anchors
    // the edge onto the new direction's entry while this stays anchored to the net-zero position — the same
    // history-independence the read law now has. That flip divergence is an audible cadence change (dice and
    // slew redraw, LOR latch, commit) and NEEDS A RACK FEEL-TEST.
    bool atPhraseEdge() const {
        if (phraseOverridden_) return phraseOverride_ == 0;
        if (col_ < 0) return true;   // the pre-start park: phrasePos() reads 0 there
        const int64_t lg = (len_ < 1) ? 1 : (int64_t)len_;
        return (netSteps_ % lg) == 0;
    }
    int phrasePos() const {
        if (phraseOverridden_) return phraseOverride_;   // the phase snap's raw phrase
        if (col_ < 0) return 0;   // the pre-start park: no phrase yet
        return (dir_ > 0) ? wrapRing(col_ - startStep_) : wrapRing(endStep_ - col_);
    }
    // (NET-STEPS) THE SIGNED READ COORDINATE the voice/macro reads fold: the NET steps travelled since the entry,
    // folded into the window. One law, no direction field, no entry history — where you are, not how you got here.
    //
    // THE FOLD IS mod len_, NOT mod the row length. readCoord is bounded by the GLOBAL window (len_ == lenG); the
    // consumers then fold it again into their own row/lane length L. Returning the raw count instead would be read
    // straight into the mod-L fold by the Macro frame path (which does NOT re-reduce mod lenG first) and diverge by
    // the global-lap remainder every lap when L does not divide lenG — the slice-4 regression. Reducing here is what
    // makes the two folds agree.
    //
    // TRUNCATED, NOT FLOORED. C++ % keeps the remainder's sign on the dividend, so a forward run reads positive and
    // a cold reverse reads negative — which is what the engine's reverse path expects. A floored mod (always
    // 0..len_-1) matches a reverse that has walked back past a FORWARD entry but destroys plain cold reverse, since
    // the two cases want opposite signs. Measured across forward / cold-reverse / both mixed flips: truncated is
    // exact on both pure runs, floored breaks every reverse-entered run.
    //
    // HISTORY-INDEPENDENT. Past its own entry this reads what a COLD start in that direction would read, so a column
    // revisited from the other side can report a different cell when L does not divide lenG. That retrace is
    // deliberately retired — see netSteps_ above. The phase-snap override keeps its own raw phrase (unchanged).
    int64_t readCoord() const {
        if (phraseOverridden_) return (dir_ > 0) ? (int64_t)phraseOverride_ : -(int64_t)phraseOverride_;
        if (col_ < 0) return 0;
        const int64_t lg = (len_ < 1) ? 1 : (int64_t)len_;
        return netSteps_ % lg;   // truncated: the sign rides the count
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

