# LOR + direction undo — plan

Goal: make LOR and direction edits undoable via Rack's native Ctrl+Z (APP->history),
uniformly across Macro, Mono, and East. Deferred until now on purpose; this is the uniform
cross-module pass the de-param work kept pointing at.

## What we already have (from the de-param)
- `StoreEditAction<TModule>` + `applyAndPushStoreEdit` + `pushStoreEdit` + `StoreEditCoalescer`
  (src/ui/StoreEditAction.hpp, 20/20 tests). Records an already-applied store edit as one
  Rack history action, re-resolving the module by id (survives delete/undo-recreate).
- StoreKnob already USES the coalescer for drag edits -- so store-backed KNOBS already undo.
  This plan is only about the two control types that DON'T: the direction DirCell (a cycling
  cell) and the LOR grid (editor-state snapshots).

## The two controls are different problems

### Direction (DirCell) -- the easy one
- Edit gesture: `cycle()` -> `setStateFn(nxt)` writes the store directly. One discrete
  before/after per click. No undo today.
- Fix: route `cycle()` through `applyAndPushStoreEdit` instead of calling setStateFn raw.
  DirCell needs the module ptr + a float setter + old/new. It already has setStateFn(int);
  add an optional `pushUndoFn(int oldV, int newV)` the host wires to applyAndPushStoreEdit,
  OR (cleaner) give DirCell an optional `undoLabel` + `moduleFn` and let it call the helper
  itself. Prefer the latter: DirCell owns the gesture, so it should own the history push.
- Store-backed sites: 2 today (Macro globalDir, Mono monoLaneDir) + East when migrated.
  All go through the same cycle() path, so fixing cycle() fixes all three at once.
- Param-backed DirCells (if any remain): leave alone -- Rack params already undo natively.
- Gate-mod cycle (the CV-driven direction cycle, MonsoonSandsVisualExpander.cpp ~485 and the
  Macro equivalent): this is an AUTOMATED edit, not a user gesture. It must NOT push undo
  (you don't want every gate pulse in the undo stack). Keep it a raw setMonoLaneDir. Only the
  mouse-click cycle() pushes history.

### LOR grid -- the harder one
- Edit gesture: drag on the grid changes `currentState.lanes[l].{length,offset,rotation}`,
  then `saveToHistory()` snapshots the WHOLE VoiceState into an internal `undoHistory` deque
  (SandsVisualEditorV4.hpp:190). saveLOR()/loadLOR() then sync that state to the store.
- KEY FINDING: the editor's own undo()/redo() (lines 364/372) are NEVER CALLED -- no keybind,
  no button wires to them. So the grid's internal history is dead-end machinery: it records
  but can't replay. That's why LOR "has no undo" -- not that it's param-based, but that its
  replay path was never hooked up.
- Two options:
  A. Bridge the grid's existing snapshot history into Rack's Ctrl+Z. On saveToHistory(), also
     push a Rack history::Action whose undo/redo swap whole VoiceState snapshots (or just the
     LOR triple per lane). Heavier: VoiceState is large and includes probabilities etc., so a
     full-snapshot action is coarse (one Ctrl+Z reverts an entire grid gesture, which is
     actually fine/expected).
  B. Treat LOR like the knobs: since saveLOR() already writes the store per-lane
     (setLorBase/setGlobalLor), wrap the DRAG on each LOR sub-control (length/offset/rotation)
     in a StoreEditCoalescer, pushing one StoreEditAction per lane-item per drag. Finer-
     grained, reuses exactly the knob machinery, and doesn't touch the big VoiceState.
- RECOMMEND B where LOR is edited as discrete draggable sub-controls, A only if LOR is edited
  as an opaque grid gesture with no per-item drag boundary. Need to confirm which by reading
  the grid's LOR drag handler (is there a per-item onDragStart/onDragEnd, or one grid-wide
  gesture?). That read is step 0.

## Order
0. Read the LOR grid drag handler -> decide A vs B. (One file, one function.)
1. Direction: route DirCell::cycle() through applyAndPushStoreEdit. Fixes Macro+Mono+East
   together. Leave gate-mod cycle raw. Test: click cycles undo, gate pulses don't.
2. LOR: implement the chosen approach. Test: grid drag undoes, and one Ctrl+Z reverts one
   gesture (not 16 tiny steps, unless B naturally coalesces per lane-item).
3. Uniformity check: same undo label style across all three modules ("change direction",
   "change LOR length" etc.), so the Edit menu reads consistently.
4. East: when East's DirCell/LOR migrate, they inherit both fixes for free (same widgets).

## Watch-fors (from the de-param traps)
- Re-resolve by module id, never a captured pointer (StoreEditAction already does).
- Coalesce per gesture, not per frame -- a drag must be ONE undo entry (use StoreEditCoalescer,
  the knobs' proven path).
- Automated/CV-driven edits (gate-mod direction cycle) never push history.
- Match sibling behaviour: do all three modules in one pass so undo isn't inconsistent between
  them -- the whole reason this was deferred to a uniform pass.
