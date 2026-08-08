# Lock mode Phase 2 -- build spec

Phase 1 (the LockManager foundation) is on master and PARTIALLY threaded. This spec covers what's
left. Read LOCK_MODE_AUDIT.md (the ~15-site classification) and LOCK_SEMANTICS.md (the ruling table)
for the reasoning; this is the build recipe.

## State verified in code (Aug 2026) -- LOCK_MODE_RESUME is stale

LOCK_MODE_RESUME says the LockManager is "INERT so far -- nothing calls liveNow() yet". **That is no
longer true.** Actual state:

| Control | Category | Call sites threaded | Status |
|---|---|---|---|
| Spread | LATCH | 9 (ExpanderManager, SandsManager) | DONE |
| Lor | LATCH | 6 | DONE |
| Reseed | LATCH | 2 (Monsoon.cpp:320) | DONE |
| ABMix | LATCH | 2 (ModeController.cpp:65) | DONE |
| BigFive | LATCH | **0** | TODO |
| NoteSliders | LATCH | **0** | TODO |
| OctaveRange | LATCH | **0** | TODO |
| Pins | LATCH | **0** | TODO |
| Clock | LIVE | **0** | TODO (may need no gate -- verify) |
| Mute | LIVE | **0** | TODO (may need no gate -- verify) |
| Display | LIVE | **0** | TODO (may need no gate -- verify) |
| Transpose | LIVE | **0** | Expected: enum comment says "no call-site gate exists to migrate" |
| Scatter | QUEUE | **0** | Blocked on QUEUE mechanism (does not exist yet) |

4 of 13 threaded. The remaining 9 are the Phase 2 threading work, plus two enum entries that don't
exist yet (below), plus the QUEUE mechanism.

## GAP FOUND: Direction and Owner have no enum entries

LOCK_MODE_AUDIT:183-185 settles them: *"Owner -> LATCH. Twin with direction (same per-lane structural
control class). Resolves the earlier 'owner leans LIVE' -- it was wrong; owner and direction latch
together. Direction -> LATCH (already the OPEN ruling; confirmed alongside owner)."* And
LOCK_SEMANTICS:307 has the ruling row (array READ, like LOR -> LATCH).

But `MonsoonLockManager.hpp` has **no `Direction` and no `Owner` entry**. The enum has 13 controls;
these two are missing. Phase 2 must ADD them, not just thread them.

Their state lives in the store: `editor.laneDir[96]` (Monsoon.hpp:616), accessed via
`getLaneDir/setLaneDir` (:729-732) and `getMonoLaneDir/setMonoLaneDir`. Written from the Sands visual
editors (StraitsEastSandsVisual.cpp:514 DirCell, :922 `monoDirAuthority`). Owner similarly.

**This is one of the two behaviour-CHANGING rulings** (the other being transpose->LIVE): direction
currently does NOT latch, and under the ruling it should. So this is a real behaviour change, not a
consolidation refactor -- test carefully, and expect the change to be audible.

## Calling-convention inconsistency to normalise

Two conventions coexist:
- Instance: `lockManager.liveNow(Control::Reseed)` -- Monsoon.cpp:320.
- Static: `dotModular::LockManager::liveNow(Control::Spread, engine.locked)` -- everywhere else.

Pick ONE before threading 9 more controls, or the inconsistency multiplies. **Lean static** -- it's
the majority convention (14 of 15 existing sites), it needs no manager instance in scope (helpful in
managers that don't already hold one), and it makes the lock-state dependency explicit at each site.
Migrate Monsoon.cpp:320 to match.

## The work

### 1. Normalise the calling convention (do FIRST, small)
Migrate the one instance-style call to static. Now every site reads the same way.

### 2. Add Direction + Owner to the Control enum
```cpp
    // --- LATCH: generation-section shaping ---
    ...
    Direction,     // per-lane traversal direction (editor.laneDir). Array READ, like LOR -> LATCH.
                   // LOCK_MODE_AUDIT:185. BEHAVIOUR CHANGE: does not currently latch.
    Owner,         // per-lane owner select. Twin with Direction (LOCK_MODE_AUDIT:183-184) -> LATCH.
                   // BEHAVIOUR CHANGE: does not currently latch.
```
Add both to `categoryOf()`'s LATCH set. Enum grows 13 -> 15.

### 3. Thread the LATCH controls (the substantive work)
For each, find every site that reads/writes the control's state during generation and gate it with
`liveNow(Control::X, engine.locked)`. Per-control notes:

- **BigFive** -- the 5 rhythm knobs (NOTE_VALUE / VARIATION / LEGATO / REST / ACCENT) + their CV.
  Sites: wherever these params are sampled into the generation path. Also Junction expander maps to
  BigFive (LOCK_MODE_RESUME expander classification) -- gate the Junction read too.
- **NoteSliders** -- the 12 semitone weights -> `semiWeights`. One logical group, one gate (not 12).
  Interchange expander also maps here -- gate its writes.
- **OctaveRange** -- OCT_LO / OCT_HI. Separate entry from NoteSliders (they're separate hardware).
  Interchange maps here too.
- **Pins** -- the Change Alley pin matrix. Note the manual-pin-edit path already goes through
  `StoreEditAction` for undo; the LOCK gate is separate from the undo path -- don't conflate them.
- **Direction / Owner** (new) -- MUST MATCH LOR'S PATTERN (Rodney's ruling). LOR's mechanism, verified
  at SandsManager.cpp:260-268:
  ```cpp
  // 1. Compute the value freely -- NOT gated. base + CV + deltas all resolve normally.
  baseLen = clamp(baseLen + eastDelta(...) + macroDelta(...), 1.f, 16.f);
  // 2. Gate the PUSH into engine state.
  if (LockManager::liveNow(Control::Lor, engine.locked)) {
      engine.setStrand(StrandWriter::MONO, strand, baseLen, baseOff, baseRot);
  }
  ```
  The in-code comment states the principle: *"under lock, do NOT re-push the strand window -- the
  engine's LOR state persists, so skipping the write holds the pre-lock resolved value (base +
  latched CV)."*

  So the gate is on **the push to engine state** -- not the read, not the store write. It works
  because engine state is PERSISTENT rather than recomputed per block: skipping the write leaves the
  pre-lock value in place. This is also why the migration is a clean no-op (`liveNow(LATCH) ==
  !locked`).

  Apply the same shape to Direction: let the editor gesture write the store freely
  (`setLaneDir` still works, the UI still moves), and gate the point where lane direction is PUSHED
  INTO the engine's traversal state. Find that push site -- it is the analogue of `engine.setStrand`.
  User-visible result matches LOR exactly: the control moves under lock, but the change doesn't take
  effect until unlock.

  **OWNER may already be latched for free.** The same comment continues: *"This also latches OWNER
  for free: owner selects which base feeds baseLen above, and freezing the apply freezes that
  selection's effect too."* So before adding an Owner gate, CHECK whether owner is already
  effectively latched via the LOR push gate. If it is, the `Owner` enum entry may be
  model-completeness only (like Transpose) with no call site -- which would shrink this step.
  Verify before threading.

### 4. Verify the LIVE controls need no gate
Clock, Mute, Display, Transpose are LIVE = "never obeys lock". If no gate exists, that IS correct
behaviour -- LIVE means unconditional. So for these, the work is **confirming no stray `if (!locked)`
gates them today**, not adding calls. If a gate is found, REMOVE it (that's a behaviour fix).
Transpose specifically: the enum comment already asserts no gate exists; verify and close.

### 5. Build the QUEUE mechanism (the other half of Phase 2)
Currently QUEUE is a category with no machinery. Needed:
- **Boundary events**: the manager needs to know when a phrase boundary occurs (the fire point) and
  when unlock occurs (the commit point). The engine already knows both -- SequencerEngine has the
  `wrapped` signal used for phrase-boundary logic (SequencerEngine.cpp:283 uses `wrapped && !locked`).
- **The queue itself**: arm-on-gesture, fire-at-boundary. One armed state per QUEUE control (only
  Scatter today).
- **Absorb the existing shadow state**: `MonsoonExpanderManager`'s `caV2PrevStep_` / `caV2PrevLocked_`
  currently implement an ad-hoc version of this. Moving them into the manager is "step b" of the CA
  cleanup per LOCK_MODE_RESUME. Verify these two fields are the whole shadow state before absorbing.
- **Thread Scatter** through the new queue.

### 6. Causeway
MASTER_PLAN inventory says "Causeway -- ship-ready modulo lock Phase 2". Identify what Causeway
specifically needs (likely one of the LATCH controls above, threaded through its expander read) and
confirm it's covered by steps 3-4. If it needs its own entry, that's a 16th control.

## Build order + guard rails

1. Normalise calling convention (mechanical, no behaviour change).
2. Add Direction + Owner enum entries + categoryOf rows (no behaviour change yet -- nothing calls them).
3. Thread the four pure-consolidation LATCH controls: BigFive, NoteSliders, OctaveRange, Pins. These
   should be **behaviour-preserving** if the audit's finding holds ("every audited LATCH site already
   implements LATCH correctly" -- so migrating `if (!locked)` to `liveNow(LATCH)` is a no-op since
   `liveNow(LATCH) == !locked`). **Tests must stay green with no audible change.** If behaviour
   changes here, the audit missed a site -- investigate before proceeding.
4. Thread Direction (+ Owner IF it isn't already latched for free via the LOR push gate -- check
   first, see step 3). Follow LOR's shape exactly: compute freely, gate the PUSH into engine state.
   **THIS IS A REAL BEHAVIOUR CHANGE** -- direction doesn't latch today and will after. Rack-verify:
   lock the module, change lane direction, confirm the control MOVES but the traversal doesn't change;
   unlock, confirm the change commits. Should feel identical to how LOR behaves under lock -- that's
   the acceptance test.
5. Verify LIVE controls have no gates (remove any found).
6. Build QUEUE + thread Scatter. Rack-verify: arm scatter under lock, confirm it fires at the next
   phrase boundary, not immediately.
7. Confirm Causeway's requirement is covered.

Guard rails:
- 30/30 tests green after each step.
- Steps 1-3 and 5 should be behaviour-NEUTRAL. Any audible change there means a missed site.
- Steps 4 and 6 are behaviour-CHANGING by design. Rack-verify each specifically.
- The engine keeps its OWN freeze checks (PatternEngine.cpp:176, :286 `if (in.locked) return;`) --
  these are audio-thread and must NOT round-trip through the manager. Do not migrate them.

## Cross-refs
- LOCK_MODE_AUDIT.md -- the ~15-site classification; :183-185 the Direction/Owner ruling.
- LOCK_SEMANTICS.md -- the full ruling table; :186-189 the read-vs-map principle; :307 direction row.
- LOCK_MODE_RESUME.md -- the "pick it up later" note (its INERT claim is now stale; otherwise good).
- LOCK_MODE_PLAN.md, LOCK_MODE_STARTING_POINT.md, LOCK_GHOST_UX.md, LOCK_QUEUE_STATUS.md -- background.
- src/dsp/managers/MonsoonLockManager.hpp -- the manager (enum, categoryOf, liveNow).
- PatternEngine.cpp:176,286 -- the engine's own freeze checks (leave alone).
- SequencerEngine.cpp:283 -- `wrapped && !locked`, the phrase-boundary signal QUEUE needs.
- Monsoon.hpp:611-616,729-732 -- laneDir storage + accessors (Direction/Owner gate targets).

## STEP 3 RULING (Rodney, Aug 2026): Big-5 + scale/range ARE behaviour-changing -- thread them, split the commits

CC's Step 3 trace is CORRECT and the build spec's "neutral" label was WRONG. Verified in code:
- The lock freeze (PatternEngine.cpp:286) latches only the DRAW ARRAYS
  (melodyRandom/octaveRandom/rhythmRandom).
- Playback recomputes pitch LIVE via genPitchLive (SequencerEngine.cpp:434), reading current
  semiWeights/octaveLo/octaveHi. Frozen melodyPitchV[] is persistence/display only.
- Big-5 (REST/LEGATO/ACCENT/NOTE_VALUE) enter executeStep as LIVE args; only r_rest is frozen, not
  restProb.
So moving Big-5 or scale/range under lock is AUDIBLE today. LOCK_SEMANTICS §9 rules these LATCH
(Tier V Vermona baseline -- "move anything under lock, hear nothing until unlock"). The ruling was
always clear; only the spec's neutrality assumption was wrong. These steps ARE behaviour-changing.

Implementation (approved): gate the WRITES in ModeController::updatePatternInput() -- under lock, skip
refreshing restProb / semiWeights[] / octaveLo / octaveHi / variationAmount (and skip the live
accentProb / getLegato / getNoteValue refresh at the executeMode sites). Skipping the write holds the
pre-lock resolved value == the lock-on knob+CV snapshot §9 asks for, same persistent-state mechanism as
LOR. This is the LOR pattern applied to the Big-5/scale/range write site.

CAVEAT -- DO NOT GATE TRANSPOSE: updatePatternInput also refreshes currentPatternInput.transpose
(adjacent line). Transpose is ruled LIVE, not LATCH -- it must keep refreshing unconditionally under
lock. The write-gate must skip restProb/semiWeights/octaveLo/octaveHi/variationAmount but LEAVE transpose
refreshing. Easy to sweep in by accident since it's the neighbouring assignment.

Landing strategy: SPLIT into two commits, each one audible axis, for independent Rack verification:
- Commit A (pitch axis): semiWeights[], octaveLo, octaveHi. Verify: under lock, move scale faders +
  octave range -> pitch content must not change until unlock.
- Commit B (rhythm/articulation axis): restProb, variationAmount, + accentProb/getLegato/getNoteValue
  at the executeMode sites. Verify: under lock, move REST/LEGATO/ACCENT/NOTE_VALUE -> articulation must
  not change until unlock.
Rationale for the pitch-vs-rhythm split (vs CC's Big-5/scale-range boundary): if a Rack pass surprises,
"did pitch move or did timing move?" is the fastest diagnostic and maps cleanly onto which commit to
inspect. CC's group-based split is also acceptable; either beats one commit. The rulings above (thread=yes,
write-gate=yes, split=yes, transpose-stays-live) are the fixed points; the exact split boundary is CC's call.
