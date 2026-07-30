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
