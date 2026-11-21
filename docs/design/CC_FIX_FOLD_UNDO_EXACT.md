# CC FIX — SandsFold undo must restore the neighbours EXACTLY (Tessera does)

Read first: `docs/design/SEAMLESS_FOLD_PLAN.md`, `docs/design/CC_PROMPT_FOLD_RESIZE_SPIKE.md`, and
`src/SandsFold.cpp` (`applyWidth`, `FoldWidthChangeAction`).

## Rack feel-test findings (Rodney, 2026-11-22)
- Growing the panel pushes the modules to its right away. OK.
- Shrinking leaves a GAP. ACCEPTED: Tessera does the same; do not add pull-back on an explicit shrink.
- **UNDO leaves a gap. NOT OK:** Tessera's undo does not. After grow -> undo, every pushed module must be back at its
  ORIGINAL position, exactly.

## Cause (read the code, confirm)
`FoldWidthChangeAction` records only `oldHp` / `newHp`. `undo()` re-applies the old width through `applyWidth`, which
runs the same neighbour walk with a delta. A leftward `setModulePosForce` evidently does not close the gap (and the
code comment above the walk says it should, so it does not match the observed behaviour: find out why, and fix
the COMMENT to describe what really happens). Re-applying a width is the wrong model for undo: undo must restore
STATE, not re-run the operation.

## Do
1. `applyWidth` records, for every module it moves, `{ moduleId, oldPos, newPos }` (the position BEFORE the move and
   the position it was actually set to, read back after `setModulePosForce`, since Rack rounds/snaps).
2. `FoldWidthChangeAction` stores that list plus `oldHp` / `newHp`. 
   - `undo()`: restore the width (`oldHp`, no new history push), then set every recorded module back to `oldPos`
     (`setModulePosForce`).
   - `redo()`: width to `newHp`, then every recorded module to `newPos`.
   - A module that no longer exists (deleted since) is skipped, never a crash. A module the user has moved since the
     action ran: restore only if it is still at the position the action left it (`newPos` for undo); otherwise leave it.
     (Position check with a 1 px tolerance.)
3. Keep the push-on-grow behaviour. Do NOT add pull-back on shrink.
4. Order of ops: resize our own box FIRST for undo of a grow (shrink ourselves, then put neighbours back) so the
   module never overlaps a neighbour mid-restore; for undo of a shrink the reverse (move neighbours away, then grow).
   `setModulePosForce` pushes on overlap, so a wrong order can displace modules a second time.

## Rack-free test (add to the core, `test/run_all.sh`)
Extract the pure part into `src/ui/FoldPanelWidthCore.hpp`: given a list of `{id, x}` and a restore request, decide which
entries to restore (exists, still at the expected x within tolerance). Test: exact restore, a module moved by the user
is left alone, a missing module is skipped, empty list is a no-op, tolerance edge. Write it red first; mutation-check it
with an INDEPENDENT oracle (not the function under test).

## Cannot verify here (Rodney feel-tests in Rack)
- grow -> undo -> redo -> undo: neighbours land on their exact original / pushed positions each time.
- grow twice then undo twice.
- undo after the user has dragged a neighbour elsewhere: that neighbour stays where the user put it.
- Undo of a shrink with no gap closing.
- Patch save/load after undo/redo keeps the width and positions.
