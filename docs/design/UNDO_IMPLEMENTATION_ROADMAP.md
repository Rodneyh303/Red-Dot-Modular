# Undo implementation roadmap (branch: feat/undo-implementation)

The full undo suite, in dependency/simplicity order. Items 1-3 are the pre-existing plan;
items 4-5 are the dice + Change Alley work designed in the recent sessions (UNDO_PLAN.md).

## Existing infrastructure (done)
- src/ui/StoreEditAction.hpp: StoreEditAction (rack::history::Action), pushStoreEdit,
  applyAndPushStoreEdit, StoreEditCoalescer. The foundation for all scalar/store undo.
- Store-backed KNOBS already undo via StoreKnob + StoreEditCoalescer. (Item 3: nothing to do.)

## Item 1 -- Direction (DirCell::cycle) [QUICKEST WIN, START HERE]
- src/ui/OwnerCell.hpp:129 cycle() currently calls setStateFn(nxt) RAW, no undo.
- Fix: route cycle() through applyAndPushStoreEdit (before/after the 0..3 state).
- One function fixes Macro + Mono + East (all share DirCell::cycle()).
- Gate-mod CV cycle stays RAW (automated, not a user gesture -- never push history).
- Test: cycling a direction cell, Ctrl+Z reverts one step.

## Item 2 -- LOR grid (approach A: bridge saveToHistory)
- src/ui/SandsVisualEditorV4.hpp. STEP 0 ANSWERED: LOR uses ONE grid-wide drag gesture
  (dragState captured onButton:917, saveToHistory() fires on GLFW_RELEASE:946). NOT per-item.
- The grid's own undo()/redo() (dead-end deque, never called) stays unused.
- Fix (approach A): in saveToHistory(), ALSO push a Rack history::Action that snapshots/restores
  the VoiceState (or the per-lane LOR triple). Bridge that one call site into APP->history->push.
- Test: grid drag undoes; one Ctrl+Z reverts one drag gesture.

## Item 3 -- Knobs [DONE]
- Store-backed knobs already undo via StoreEditCoalescer. Nothing to do.

## Item 4 -- Dice undo
- INTERIM (current model, scrub not yet built): a committed dice roll in REVERSIBLE mode is a
  counter position. Undo = (before_counter, after_counter) scalar via StoreEditAction. Wire the
  roll commit to push that scalar pair. Trial/audition rolls: undo = discard pending draw (not a
  counter step -- nothing committed). Non-reversible free-run: dice undo UNDEFINED (no stable
  counter), do not offer.
- FUTURE (scrub model): dice state = float scrub counter; undo trivially (before, after) scalar,
  no array snapshots. Supersedes interim when scrub is built.
- Test: roll in reversible mode, Ctrl+Z steps the counter back one roll.

## Item 5 -- Change Alley undo (snapshot stack)
- Per UNDO_PLAN two-structures design: UNDO STACK (user-time, recency-ordered), 24 bytes/entry
  (pin_matrix[16] + scatter_counter[8]), push on action, pop on Ctrl+Z.
- Invertible transforms (Reflect/Rotate/ScatterRows): op-code entry only (~2-5 bytes), undo =
  re-apply inverse. Fan-in (Scatter/interScatter/Collapse): 16-byte pin snapshot + counter.
- Manual pin edits: also StoreEditAction (Ctrl+Z) as now; the stack entry handles CA-specific
  (pin+counter) state.
- MUST track scatter_counter alongside pin matrix (restoring pins without the counter leaves the
  next scatter drawing the wrong permutation).
- Test: scatter/transform/manual edit each undo correctly; counter restored so next scatter is
  deterministic.

## NOT in this branch (separate, later)
- REVERSIBLE MODE buffer (transport-time, 28-byte position-indexed circular buffer) -- that's
  reverse mode, not undo. CA reverse + dice scrub reverse are the "new A/B mode / CA reverse"
  work item AFTER lock mode. This branch is UNDO (Ctrl+Z) only.
- Lock mode: separate, AFTER undo (undo is its foundation).

## Order
1. Item 1 (direction) -- one function, proves the pattern.
2. Item 2 (LOR) -- bridge saveToHistory.
3. Item 4 (dice interim) -- scalar counter undo in reversible mode.
4. Item 5 (Change Alley) -- snapshot stack + counter.
(Item 3 done. Reverse mode + lock mode are separate later branches.)

## Item 2 analysis (LOR grid) -- target identified, ready to implement next session

Key finding: the editor's currentState is NOT the authority. It is a two-way CACHE of the module
store:
- Loaded FROM store: visualEditor->currentState.lanes[l].{length,offset,rotation} =
  monsoon->getLorBase(slot, bank, c)  (MonsoonSandsVisualExpander.cpp:366-368; c: 0=len,1=off,
  2=rot).
- Written TO store/engine: syncEditorToPatternEngine / setLorBase.
- Store authority: Monsoon::editor.lorBase[slot*18 + bank*3 + c] (Monsoon.hpp:688-689).

Therefore approach A should snapshot/restore the STORE (getLorBase/setLorBase), NOT the editor's
currentState -- the store survives widget destruction (no editor-pointer lifetime problem in the
history action) and is the persistent authority.

Implementation plan:
- Capture the LOR triple (len,off,rot) for the affected (slot,bank) at DRAG START (onButton when
  a handle is grabbed -- dragState already captures grabOffset etc at :917).
- At DRAG RELEASE (onButton GLFW_RELEASE, where saveToHistory() already fires :946), push a Rack
  history::Action restoring the store triple via setLorBase(slot,bank,c,val) for the before/after,
  then re-sync the editor from the store on undo/redo.
- The action holds (module, slot, bank, before[3], after[3]) -- all values, no widget pointer.
- Editor lane -> (slot, bank) mapping: use the same mapping the load path uses (line 366-368
  pattern; note editor lanes are editor-indexed, mono uses kMonoSlot).
- The editor's own undoHistory/undo()/redo() deque stays dead (unused) -- do not wire it.

Left the existing dead-end deque alone; only bridging saveToHistory() -> APP->history->push.

## Status at end of session
Item 1 (direction + ownership) DONE and verified in Rack (interleaved undo works). Item 2
analysed, store target identified, ready to implement. Items 3 (knobs) done. Items 4 (dice
interim) + 5 (Change Alley snapshot stack) still to do per roadmap order.

## Item 4 analysis (dice undo, interim) -- deferred/mode-gated, start here next

More intricate than items 1-2 (immediate store writes). Dice rolls are DEFERRED and MODE-GATED:

- User gesture sites (Monsoon.cpp): DA_* dice actions dispatch setPendingRhythmRoll(),
  setPendingRhythmTrial(), setPendingRhythmLastRoll(), etc. (~lines 368-399, 831). These set a
  PENDING flag -- the roll does not commit at button-press.
- The counter (int64_t rhythmDrawCtr/melodyDrawCtr, PatternEngine:384, signed "can go negative
  on reverse") advances via advanceRhythmDraw(dir)/advanceMelodyDraw(dir) (:426-7) when the
  pending roll COMMITS at the next phrase boundary -- NOT at press.

Implication for undo: the (before,after) counter delta must be captured at COMMIT time (phrase
boundary), not press time. Wiring a StoreEditAction at the deferred commit point, not the gesture.

Mode gating (must respect):
- REVERSIBLE mode: counter is the state; undo = decrement (before,after) scalar. THIS is where
  dice undo is defined. rhythmAuditionsAllowed() == !rhythmReversible (:406).
- TRIAL/audition (A frozen): undo = discard the pending draw, not a counter step. Nothing
  committed to rewind.
- FREE-RUN / non-reversible: dice undo UNDEFINED (no stable counter). Do not offer.

Design question to resolve at implementation: where exactly does the pending->committed
transition happen (find the phrase-boundary commit that calls advanceRhythmDraw), and capture
before/after there. StoreEditAction wraps the scalar; the setter writes the counter and
re-derives the pattern (same Philox re-derivation the engine already does on reverse).

Deferred to next session -- wants careful capture-at-commit design, not a rushed gesture-time
wiring. Items 1 (direction+ownership) and 2 (LOR) DONE + verified in Rack (item 2: cross-tab
undo confirmed correct, validating store-as-authority). Item 3 (knobs) done. Item 5 (Change
Alley) after item 4.

## Item 5 analysis (Change Alley undo) -- commit point mapped, thread-safety is the crux

### What already has undo (verified)
- MANUAL pin edits: store-backed via applyAndPushStoreEdit (MonsoonChangeAlleyV2.hpp:678).
  Already Ctrl+Z-undoable.
- RESET pins: whole-table ResetPinsAction snapshot (MonsoonChangeAlleyV2.hpp:699). Already
  undoable.
So item 5 is specifically TRANSFORM + SCATTER undo (reflect/rotate/scatter/collapse), which is
what currently has NO undo.

### Active module + state
- ACTIVE: modelMonsoonChangeAlleyV2 (Monsoon.cpp:1041; V1 MonsoonChangeAlleyExpander commented
  out at :1040). Work against V2.
- Pin matrix state: ca->rhythmSrc[16], ca->melodySrc[16] (uint8_t, persisted in dataToJson).
- Scatter counter: Temasek's tk->scatterCounter[] (advances on scatter, must be captured too).

### The transform COMMIT POINT (where undo must attach)
NOT in CA's process() -- that only LATCHES pending actions on trigger. The actual pin mutation
happens in the MANAGER at the boundary:
- src/dsp/managers/MonsoonExpanderManager.cpp:145
    dotModular::ca::apply(t, tbl, active, p.blk, p.scatterSeed);   // tbl = rhythmSrc/melodySrc
- Temasek transforms: :164, :206  dotModular::ca::applyTemasek(...), also advancing
    tk->scatterCounter[ci] += p.scatterDelta  (:163).
So a transform snapshot = capture tbl[16] (the affected rhythmSrc or melodySrc) + the relevant
scatterCounter entry BEFORE ca::apply/applyTemasek, and after.

### THE CRUX: thread safety
This commit point is on the MANAGER thread (audio/process side), NOT a UI widget gesture. Rack's
history (APP->history->push) must be called from the UI THREAD, not audio. So transform undo
CANNOT push history directly at the commit point. Options:
1. Queue the (before,after) snapshot on the commit thread into a lock-free ring; drain it on the
   UI thread (widget step()) and push history there. Clean separation, standard pattern.
2. Snapshot into the CA module's own undo STACK (the 24-byte design from UNDO_PLAN) on the commit
   thread (just memory, no Rack history), and expose Ctrl+Z via a CA-level history action that
   pops that stack. The module-owned stack is the two-structures design anyway -- this is also
   the REVERSIBLE MODE groundwork (same snapshots).
   -> RECOMMENDED: option 2 aligns with the two-structures design. The module-owned snapshot
      stack (pin matrix + scatter counter per transform) serves BOTH undo (pop) and later
      reversible mode (seek). Pushing to Rack history becomes a thin action that pops the stack.

### Plan (next session)
- Add the CA module-owned undo stack: entries {rhythmSrc/melodySrc snapshot (which side), scatter
  counter value, transform id}. Push on the manager thread at the commit point (:145/:164/:206),
  before ca::apply mutates tbl.
- Invertible transforms (reflect/rotate/scatterRows): can store op-code only, but for v1 uniform
  snapshot is simpler (16 bytes, cheap) -- matches UNDO_PLAN recommendation.
- Bridge to Ctrl+Z: a rack::history::Action whose undo() pops the CA stack and restores tbl +
  counter (module-id resolved, survives deletion like ResetPinsAction).
- This stack IS the reversible-mode groundwork (two-structures design): undo pops, reverse will
  seek the position-indexed variant later.

### Status
Items 1 (direction+ownership), 2 (LOR) DONE + verified. Item 3 (knobs) done. Item 4 (dice)
DEFERRED -- will fall out of the scrub redesign trivially, not built on the throwaway current
model (decided: do CA undo now, dice later via scrub). Item 5 commit point mapped; thread-safety
(manager thread vs UI-thread history) is the design crux -- recommended module-owned snapshot
stack (also reverse-mode groundwork). Ready to implement next session.
