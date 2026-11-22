# CC prompt — Fold resize: double-push, undo gaps, 90 HP Monsoon clobber

Branch: `fix/fold-resize-spike` (continue on it). Rack cannot run in Rodney's
review environment, so every Rack claim below is a HYPOTHESIS from code reading
+ his feel-test numbers. Verify against the SDK and report what you find.

## Symptoms (Rodney's feel-test, 2026-11-22)
1. Widening can clobber an adjacent module — seen ONLY with an adjacent Monsoon and ONLY at 90 HP.
2. Undo sometimes restores cleanly, sometimes leaves a gap.
3. Widening sometimes pushes other modules too far. 38 -> 60 HP (delta 22 HP):
   Monsoon stays adjacent (moved ~22 HP, correct) but Tessera Roll moved ~44-47 HP (about double).

## Hypothesis (code: `SandsFoldWidget::applyWidth`, `restoreMoves`)
`applyWidth` iterates `APP->engine->getModuleIds()` (creation order, NOT x order)
and calls `setModulePosForce(nw, pos + shift)` on every module right of us.
`setModulePosForce` already shoves modules it overlaps. If a nearer module is
visited before a farther one, its move shoves the farther one; the farther one
is then moved by the full `shift` on top -> pushed twice. Order-dependent, hence "sometimes".

Same loop corrupts undo: `oldX` is captured at visit time (after earlier shoves),
so undo "restores" to a shoved position; modules shoved without being visited
are never recorded.

Not explained by this: the 90 HP clobber (a final-position error, not a gap).
Monsoon is a SandsSnap host (0.1 s resnap, force re-seat of owned subpanels,
"intruder shoved out" — see `src/StraitsBase.cpp` resnapRun ~868-925 and
`src/ui/SandsSnap.hpp`) and may fight the move. Needs data, not guesses.

## Fix
1. **Pure helper in `src/ui/FoldPanelWidthCore.hpp`** (Rack-free, namespace `redDot::fold`):
   - Input: our left x, our oldHp/newHp, and a list of candidates `{id, x, y}` plus our y.
   - Select neighbours: same row (`|y - ourY| < RACK_GRID_HEIGHT`, pass the constant in) and `x >= oldRight - 1`.
   - Sort by x. Output an ordered move list `{id, oldX (TRUE original), newX = oldX + shift}`.
   - Order: GROW -> farthest first (each moves into free space, nothing shoved). SHRINK -> nearest first.
   - Final positions are computed up front, never from read-back mid-walk.
2. **`applyWidth`**: build candidates from the rack, call the helper, then apply moves in the returned
   order with `setModulePosForce`. Record the helper's true oldX/newX (read back newX only to log a mismatch).
   Our own `setOwnWidth` ordering stays as is.
3. **Undo/redo (`restoreMoves`)**: apply in the same direction rule. Undoing a grow = nearest first
   (neighbours move left into the space we vacated after we shrink first); redo of a grow = farthest first.
   Keep the 1 px `shouldRestoreNeighbour` gate.
4. **Temporary logging** (`DEBUG`/`INFO`, easy to strip): for each moved module log model slug, id,
   before x, target x, read-back x; plus for the fold itself old/new HP and box. Also log any module
   whose x changed during the walk but is NOT in the move list (a shoved bystander). This is how we
   learn the 90 HP Monsoon case. If it is the SandsSnap resnap fighting, say so with the log lines;
   do not guess a fix for it in this change.

## Test (failing first)
Add `test/test_fold_neighbour_order.cpp`, register in `test/run_all.sh`.
- Write an INDEPENDENT oracle: a small simulator of `setModulePosForce`-style shoving
  (placing a module at x pushes any overlapping module right by the overlap, cascading),
  with module widths. Do not derive the oracle from the helper.
- Cases: 3-5 neighbours in contiguous row, creation order scrambled; grow 38->60 and 12->90;
  shrink; grow then undo then redo. Assert: final x of each module == oldX + shift exactly
  (no double push), undo returns every module to its exact original x, no overlaps at any step.
- Include a gap neighbour (module not adjacent) and a different-row module (must not move).
- Mutation-check: (a) iterate in creation order -> test must fail; (b) reverse the grow order -> fail;
  (c) capture oldX after shoving -> fail. Show red first, then green.
- Run: `CPLUS_INCLUDE_PATH=/home/claude/simde bash test/run_all.sh fold_neighbour_order` then the full suite.

## Report back
- Red/green evidence and mutation results.
- Whether the SDK's `setModulePosForce` semantics match the oracle (cite the source).
- Anything in the log analysis plan that should change.
Rodney will feel-test in Rack: 38->60 with Roll beyond Monsoon, 90 HP next to Monsoon, undo/redo each.

Shrink gap on an explicit shrink stays ACCEPTED (Tessera does the same). Undo must be exact.
