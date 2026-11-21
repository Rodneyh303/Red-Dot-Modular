# CC PROMPT — Position Resolver SLICE 4: unify the advance — derive stepIndex from pos_ (collapse the duplicate walk)

Follow-on to slices 1-3 (merged on `lane-expander-refactor`). `pos_` is the proven mirror
(`test_position_resolver_identity`: `pos_.playedColumn() == stepIndex` at EVERY step — every
window, direction, re-author, the phase snap). This slice retires the DUPLICATE: `stepIndex` is
currently walked by hand in the authority sites, and `pos_.advance()` / `pos_.setWindow()` already
walk IDENTICALLY. Make `pos_` the authority and DERIVE `stepIndex` from it — `stepIndex =
pos_.playedColumn()` — deleting the hand walk. Reference: `DESIGN_POSITION_UNIFICATION.md`
migration step 4 (the stepIndex half; globalWalk_ is a SEPARATE thread — see Non-scope).

## CARDINAL RULE (non-negotiable)
VERIFY AGAINST CODE AND TESTS, never a plausible narrative. This is a PURE REFACTOR — it must be
BIT-IDENTICAL. The identity harness is the oracle: it already pins `playedColumn() == stepIndex`
at every step, so a correct delegation cannot drift, and any drift fails it. No Rack here.

## BRANCH
Off `lane-expander-refactor`. PR back into it.

## THE DUPLICATE (verified — this is what collapses)
`pos_.advance(dir)` runs FIRST in `advancePlayhead` (SequencerEngine.cpp ~:478, slice 2), THEN
`stepIndex` is walked by hand (~:480-493) doing the SAME thing `pos_.advance` just did: seed at
`endStep+1` (reverse) / `startStep-1` (forward) from the `-1` park, step ±1, skip to the window
bound, scan to the next in-window cell. `pos_`'s own `advance()` (PositionResolver.hpp) is
line-for-line that walk. Likewise `setWindow` re-bounds `stepIndex` by hand (~:285-287) with the
exact law `pos_.setWindow` (~:73) already applied to `col_` (park to `startStep-1` when outside the
new window).

## THE SLICE (exactly this)
1. **advancePlayhead** — DELETE the hand `stepIndex` walk (~:480-493, both the reverse and forward
   arms) and set, right after `pos_.advance(dir)`:
       stepIndex = pos_.playedColumn();
   Derive the phrase-boundary flag from the resolver instead of the per-arm `stepIndex ==
   startStep` / `== endStep` tests:
       const bool wrapped = (prevStep != -1 && pos_.phrasePos() == 0);
   `pos_.phrasePos() == 0` is EXACTLY `col == startStep` (forward) / `col == endStep` (reverse) by
   the resolver's own definition — direction-agnostic, and equal to the two arms it replaces.
   Keep `prevStep = stepIndex` captured BEFORE `pos_.advance` (unchanged). Everything downstream of
   `wrapped`/`stepIndex` (the direction promote, the holders, the result) is untouched.
2. **setWindow** — DELETE the hand `stepIndex` re-bound (~:285-287) and set, right after
   `pos_.setWindow(length, offset)`:
       if (stepIndex != -1) stepIndex = pos_.playedColumn();
   (`pos_.setWindow` already parked `col_`; the `-1` park is preserved because `playedColumn()`
   returns `-1` when `col_ == -1`.) Leave the `globalWalk_` re-bound on the next line ALONE.
3. **snapToPhaseStep** — it already sets `stepIndex = phaseStep` and snaps `pos_` to it, so they
   agree. For CONSISTENCY (pos_ is now the authority everywhere), set `stepIndex =
   pos_.playedColumn()` after the `pos_.snapTo(...)` call instead of `= phaseStep` (same value —
   `pos_` was just snapped to `phaseStep`). Small, optional-but-tidy; do it so no site writes
   `stepIndex` from anything but `pos_`.

## KEEP (do NOT delete, do NOT touch)
- The `stepIndex` FIELD stays — it is now a DERIVED mirror (written only from `pos_.playedColumn()`).
  Deleting it is a LATER step, after the widget reads (step 3b) and globalWalk_ are gone.
- The `stepIndex` READS stay as they are — the window-check `isStepInWindow(stepIndex)` guards, the
  `cachedMelodyStepIndex`/`cachedRhythmStepIndex` caches (PatternEngine), the consumers already on
  `playedColumn()`. They read the derived field; nothing to change.
- `pos_.advance` / `pos_.snapTo` / `pos_.setWindow` — the authority, unchanged.

## NON-SCOPE (separate threads)
- **globalWalk_** — the OTHER position system. It is a SIGNED phrase accumulator ((t−1) mod lenG,
  advanced by signMotion), NOT the walked column, so it does not reduce to `pos_.playedColumn()` and
  the abs-mode derive still uses it. It needs a design pass (does the resolver gain a phrase-clock
  view?), NOT this slice. Leave every `globalWalk_` line exactly as is.
- Widget `stepIndex` reads (step 3b) — Rodney's Rack leg.
- Deleting the `stepIndex` field (step 4-final) — after 3b + globalWalk_.

## ACCEPTANCE (bit-identical)
- `GOLDEN_A/B/BS`, `UA_NONDIV` BIT-IDENTICAL; `test_position_resolver_identity` stays green (it is
  the proof that the delegation can't drift); the reverse + phase oracles
  (`test_reverse_rotation_anchor`, `test_phase_rotation_anchor`) stay green; full battery green.
- STRUCTURAL pin (extend `test_played_column_consumers` or add one): `advancePlayhead` and
  `setWindow` no longer contain the hand `stepIndex` walk/re-bound — `stepIndex` is assigned from
  `pos_.playedColumn()`, and `wrapped` reads `pos_.phrasePos()`. So a regression that re-introduces
  an independent `stepIndex` walk is caught headlessly.

## DELIVERABLE
The three sites delegating `stepIndex` to `pos_` (hand walk + re-bound deleted, `wrapped` from
`phrasePos()==0`), the field + reads + globalWalk_ untouched, the structural pin. Full battery
green, goldens + identity harness + both oracles bit-identical. PR confirms the `wrapped`
equivalence held across the direction programs (fwd / rev / fwd→rev) the identity harness sweeps.
