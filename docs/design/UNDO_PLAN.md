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

## Dice + Change Alley transform undo (Philox-grounded)

Now that Change Alley draws through the shared PhiloxRng (counter-addressable) on its OWN
correlation stream, undo of stochastic actions has a clean, principled answer that differs by
whether the action is counter-addressed (rewindable) or fan-in (snapshot-only).

### Dice undo -- counter-rewind, ONLY in reversible mode
A dice roll is a POSITION in a counter-addressed stream, not a mutation. PhiloxRng::at(pos) is a
pure function of (pos, key), so:
- **Reversible mode:** undo of dice = DECREMENT the draw counter and re-derive. Near-free, no
  snapshot. This is exactly what the library was built for ("replay draws backwards within one
  key"). Redo = increment again.
- **Non-reversible / free-run mode:** there is no stable counter to rewind TO (a fresh key / full
  reseed each roll), so dice undo is UNDEFINED, not merely disabled. Do not offer it there.
So: dice undo is a reversible-mode-only feature, and in that mode it is counter arithmetic, not
history snapshots. (Rodney's instinct confirmed and sharpened: not "only in reversible mode" as a
policy choice -- it is the ONLY mode where the operation is even defined.)

### Dice MODES -- what each does under undo
The four dice modes (live, trial, last-dice, last-trial) already QUEUE under lock (LOCK_SEMANTICS
3): a press while locked arms a redraw that fires at the next unlocked phrase boundary. Undo
interacts per mode:
- **live / last-dice:** commit a new draw at a boundary. Counter-rewindable in reversible mode
  (undo steps the counter back one roll).
- **trial / last-trial:** preview draws that are NOT yet committed. Undo of a trial is discard
  (drop the pending draw), not a counter step -- nothing was committed to rewind.
Comment to carry in code near the dice trigger: "undo of a COMMITTED dice roll is a counter
rewind (reversible mode only); undo of a TRIAL is a discard of the pending draw."

### Change Alley transform undo -- two mechanisms by transform type
The transforms split by INVERTIBILITY, which the code now documents at each function:
- **Reflect (ReflectRows/Values):** SELF-INVERSE (apply twice = identity). Undo = re-apply.
- **Rotate (rotateRows/Values, blockOffset):** a shift by +k. Undo = shift by -k.
  -> Reflect + Rotate are INVERTIBLE BY TRANSFORM: undo needs no stored state, just the inverse.
- **Collapse (collapse*/interCollapse*):** FAN-IN (many rows -> one source), NO inverse
  (documented at the transpose section: "has no inverse").
- **Scatter (scatter, interScatter):** re-source with FAN-IN allowed (NOT a permutation), so NO
  inverse transform -- even though it is seeded/reproducible.
- **ScatterRows:** the exception WITHIN scatter -- a genuine Fisher-Yates PERMUTATION, so it IS
  invertible (inverse permutation, or re-derive from the same correlation-counter).
  -> Collapse + Scatter (not ScatterRows) are FAN-IN: undo ONLY by restoring the pre-transform
     pin state (a StoreEditAction snapshot of the 16-entry pin matrix).

Comment to carry near applyTemasek: "Reflect/Rotate/ScatterRows are invertible (undo by inverse
transform); Collapse/Scatter are fan-in (undo by pin-state snapshot). Scatter draws from the
correlation stream, so a reversible-mode undo can also re-derive via counter -- but the SIMPLE,
always-correct undo is the snapshot."

### RECOMMENDATION for v1: uniform snapshot undo for all four transforms
Snapshot works for ALL four (it is the general case), and the pin matrix is 16 bytes -- storing a
before-image per transform is trivially cheap. The invertible-by-transform path (Reflect/Rotate/
ScatterRows) is an OPTIMISATION that avoids storing 16 bytes; it is NOT worth a second code path
in v1. So: one StoreEditAction snapshot of the pin matrix per transform apply, same mechanism as
a manual pin edit (LOCK_SEMANTICS: manual pin edit = config, store-snapshot undo). This also
composes with the manual-pin ruling -- a scatter-undo and a manual-pin-undo are the SAME kind of
history entry, so the Edit menu reads uniformly.
- Dice undo stays SEPARATE (counter-rewind, reversible-mode-only) because dice is a stream
  position, not a pin-state edit -- do not fold it into the transform snapshot path.

### Undo memory DEPTH
- **Store-knob / pin / transform edits:** ride Rack's native history stack (APP->history). Rack
  owns the depth (a bounded deque; the host trims oldest). We add nothing per-edit beyond the
  16-byte before-image. So transform-undo depth = Rack's global undo depth, shared with every
  other module -- no Change-Alley-private history to size.
- **Dice counter-rewind (reversible mode):** CONFIRMED against PatternEngine -- a MAIN dice roll
  ADVANCES the draw counter (rhythmDrawCtr/melodyDrawCtr, signed int64, "can go negative on
  reverse") WITHOUT reseeding (PatternEngine:310-311), distinct from a reseed-roll. In reversible
  mode the counter IS "the current index" into Philox, a "keyed bijection with NO floor/ceiling"
  (PatternEngine:391-400). So undo of a committed main dice roll = DECREMENT the draw counter and
  re-derive via at(index); the counter is the undo state, no snapshot. Depth: because Philox is a
  bijection with no floor, dice undo in reversible mode can walk back PAST intermediate points to
  index 0 (or negative) within the CURRENT KEY -- it is only bounded by a KEY CHANGE (a reseed
  installs a new key = a new sequence; you cannot rewind across that). So the floor is the last
  RESEED, but within a key the rewind is unbounded, not roll-count-limited. (Better than an
  earlier draft that said "rolls since last reseed" -- it is the whole index range of the key.)
- Comment to carry: "transform undo depth = Rack's global history; dice undo (reversible mode) =
  rewind the draw counter within the current key (bijection, no floor); a reseed/key-change is
  the only floor -- cannot cross it."

## Seed detail to CHECK (flagged, not fixed): do rhythm and melody share a key?

PatternEngine's reproducible seed path derives the SAME 64-bit key for both streams:
seedRhythmPhilox(seedFloat) and seedMelodyPhilox(seedFloat) compute sd identically from the same
float (PatternEngine:432-441), so rhythmPhilox and melodyPhilox get the IDENTICAL key (both
counters start at 0). The entropy path (seed*PhiloxFull) does NOT -- each takes a fresh
rack::random::u64(), so it is independently keyed. Only the REPRODUCIBLE float-seed path collapses
them to one key.

Concern: same key = same bijection. Rhythm and melody then draw from the SAME underlying sequence
(read differently), so their randomness is CORRELATED, not independent -- one seed value yields a
rhythm stream and a melody stream that are deterministic transforms of the same numbers. This can
surface as unintended rhythm/melody "rhyming". Two independent streams want two DIFFERENT keys.

Probable fix (mirrors the Change Alley correlation-stream domain separation): on the reproducible
path, derive rhythmKey = f(sd, RHYTHM_DOMAIN), melodyKey = f(sd, MELODY_DOMAIN) with distinct
nonces, so one user-facing seed still gives reproducibility but the two streams are orthogonal.

BUT do not "fix" blindly -- it may be INTENTIONAL: (1) the reproducible-seed feature may WANT one
seed number to define the whole pattern (rhythm+melody) as a single reproducible object, in which
case a shared key is the design; (2) the correlation may be inaudible because the draw patterns
diverge immediately. So: CHECK with Rodney whether the shared key is deliberate before changing
it. Unlike the Change Alley RNG duplication (a clear bug), this is a design question.
