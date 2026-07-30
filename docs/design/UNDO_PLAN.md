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

## Dice mode distinctions -- three genuinely different gestures (do not conflate)

This came up when considering whether to ditch trial/audition mode in favour of reversible mode.
The answer: they are DISTINCT gestures, none fully replaces the others. All three co-exist.

### 1. Trial / audition mode
- A stays FROZEN (the current committed pattern).
- Dice rolls generate candidates forward (B advances) against the fixed A reference.
- Mix knob morphs between frozen A and each candidate B, OR commit B to promote it.
- Direction: FORWARD only (new candidates, never revisit old ones without storing them).
- NOT reversible: once B is promoted to A, the old A cannot be reconstructed from the counter
  alone without the stored float arrays (LockedA/CandB). The state is in the buffers, not the
  counter.
- Intention: "keep this pattern as the reference, explore what else is possible from here."
- Already mutually exclusive with reversible mode in the engine: rhythmAuditionsAllowed() ==
  !rhythmReversible (PatternEngine.hpp:406).

### 2. Reversible mode
- Counter moves in BOTH directions; A and B are re-derived from the counter at each position.
- You can go backward to any previous roll -- the counter IS the history.
- A moves WITH the counter (no frozen reference).
- Undo of a dice roll = decrement the counter (PatternEngine draw counter, signed int64).
- Intention: "navigate the full pattern history in both directions, return to any prior state."
- Does NOT give you a frozen A to audition against -- A follows you.

### 3. Reversible A/B mix (the synthesis)
- In reversible mode: A = at(counter), B = at(counter+1), mix knob morphs between them.
- Richer than audition's all-or-nothing commit: continuous blend + full bidirectional history.
- "Advance three steps, listen at each, decide N-2 was best, rewind there" -- audition couldn't
  do that (one-step lookahead, no history).
- The one thing audition offers that this DOESN'T: A frozen while you roll candidates forward.
  In reversible A/B, advancing the counter moves A; the frozen-reference comparison is not
  available. A minor loss; the mix knob between adjacent positions partly compensates.
- Intention: "perform a morph between adjacent rolls in a rewindable history."

### Why audition is NOT replaced by reversible
Rodney's clarification: audition mode still allows rolling dice FORWARDS (generating candidates)
-- it just isn't reversible due to state (buffers not counter). The frozen-A-against-rolling-B
gesture is genuinely different from counter navigation: it's "stay here, see what's possible
ahead." Reversible mode doesn't offer a frozen reference; it navigates. Different musical
intentions, different workflow. Dropping either has a real musical cost.

### Decision: KEEP ALL THREE, understand them clearly
The surface complexity of three modes is justified because each serves a distinct intention.
The overlap (all involve the A/B buffers and Philox) is implementation overlap, not semantic
overlap -- they feel different to the performer. The engine already enforces the audition/
reversible mutual exclusion correctly (line 406); the three-way model just names what was
already there.

## Dice scrub model -- stateless counter-addressed blend (candidate direction)

### The idea
Extend reversible A/B mix to N positions (last 3-4 rolls): a SCRUB POSITION (float counter)
addresses any point in the roll history. The integer part = which roll (counter position), the
fractional part = blend toward the next. Same Philox A/B blend machinery, but the counter is
now continuous rather than stepped. The blend between any two adjacent rolls = at(floor(scrub))
blended with at(ceil(scrub)).

"Last N rolls" requires no new storage -- Philox gives you at(counter-N..counter) for free via
counter arithmetic. No float arrays needed. The scrub position IS the state.

UI: the Raffles A/B knob becomes a SCRUB POSITION knob over the last N rolls rather than a
fixed A-to-B crossfader. Park it at 2.5 = hear a blend of rolls 2 and 3 ago; advance it =
move toward the next roll; rewind it = go back further. One control, all three gestures:
- AUDITION: park the scrub, explore ahead (rolls in the +direction)
- REVERSIBLE: rewind the scrub, return to a prior roll
- A/B BLEND: fractional position between two adjacent rolls

### Why phase drive makes this NECESSARY, not just nice (Rodney)
The fundamental motivation is phase drive correctness. With phase drive the transport is a
continuous external signal -- it can move forward, backward, stall, loop, do anything. Float
buffer state (LockedA/CandB as stored arrays) is time-ordered: it represents "the last time we
rolled" which assumes forward-only time. When phase scrubs backward, the buffers hold values
that are temporally INCOHERENT with the current phase position. Same class of bug as the
Change Alley write-side remap (73 write sites, state that didn't match position under thread
interleaving).

Philox + scrub removes this entirely: scrub position is just a number with no memory of how it
got there. Phase drives it directly: phase value -> counter position -> at(floor) blended with
at(ceil). Stall phase: scrub stalls, same blend. Reverse phase: scrub reverses, fully
counter-derivable, no buffer coherence problem. THE FLOAT ARRAYS BECOME UNNECESSARY because
any position's value is available on demand, stateless.

### Connection to the stateless-position principle
This is the same principle as the stateless lane-position model established earlier:
  position = pure function of totalStepsElapsed (not accumulated state)
Applied to dice: roll-blend = pure function of scrub counter (not stored LockedA/CandB).
Both resolve the same root problem: if the transport can be nonlinear (phase drive), state
that accumulates from forward-only history is architecturally incorrect. Position-derivable
state is the only model that composes correctly with arbitrary phase.

Motivation hierarchy (most to most fundamental):
1. Audition convenience -- hear candidates. (Nice.)
2. Reversibility -- undo rolls, navigate history. (Better.)
3. Phase drive coherence -- stateless, works under arbitrary/backward/nonlinear phase. (NECESSARY.)

### Phase gains native A/B blend (Rodney)
Phase drive currently has no A/B mix -- it drives step position directly (phase -> step, one-
to-one), so phrase boundary transitions are DISCRETE JUMPS even though phase itself is
continuous. The scrub model gives phase A/B blend FOR FREE, as an inherent property:

  phase value -> float scrub counter -> at(floor) blended with at(ceil)

The fractional part of the scrub position IS the blend. As phase moves continuously through a
phrase boundary (an integer counter position), instead of the pattern snapping to the new roll
it CROSSFADES into it, the blend proportion determined by where the phase currently sits. No
separate A/B mechanism needed -- the scrub makes phase natively a morph control over the roll
sequence, not just a position control.

This removes the last discontinuity in phase drive: currently smooth within a phrase but
discrete at boundaries. With the scrub model, phase is smooth ALL THE WAY THROUGH including
phrase transitions and roll changes. The blend at each point is determined by the fractional
position, continuously.

Implication for Change Alley: at a fractional scrub position between two rolls, the correlation
blends between correlationAt(floor) and correlationAt(ceil) -- a morphed correlation. Whether
musically useful or muddy at the blend is something to discover in play; structurally coherent.

### Status: CANDIDATE DIRECTION (not yet decided for build)
Changes Raffles control semantics and the PatternEngine A/B buffer model. Record now while
reasoning is sharp; build when the dice/reversible work is scheduled.

## Change Alley state tracking -- small enough for generous snapshot buffer + transform optimisation

### CA state is small enough to track generously -- session depth reality check (Rodney)
1MB buffer, no optimisation:
  - Pin matrix:       16 bytes (uint8_t src[16])
  - scatterCounter:    8 bytes (uint64_t -- MUST be tracked alongside pin state; without it,
                               restoring the pin matrix leaves the counter at its current value
                               so the next scatter gate draws from the wrong counter position,
                               producing a different permutation than expected)
  - Phrase-boundary:   4 bytes (uint32_t index, for phase-coherent restoration)
  - Per entry total:  28 bytes
  - 1MB / 28 bytes = ~36,000 entries

Session depth at realistic scatter trigger rates:
  - Aggressive (1 scatter/beat at 120bpm): 36,000 beats / 120bpm = 300 minutes (5 hours)
  - Typical live (1 scatter every 2-4 bars): many sessions worth
  - Realistic session depth: tens to low hundreds of scatter events, not thousands. 36,000
    entries is orders of magnitude more than any user or listener would perceive as distinct
    states in a session. Buffer sizing is essentially a non-issue in practice.
  - Conclusion: "enough for all sessions" not just "most" -- 1MB with no optimisation is
    genuinely sufficient without the transform optimisation. With it, even cheaper.

### Wrap policy (must be explicit even if never triggered in practice)
CIRCULAR OVERWRITE: when the buffer is full, the next write overwrites the oldest entry and
the read pointer advances past it. Behaviour:
- Undo history becomes shallower at the tail: you can undo the last N events where N = buffer
  depth, but not further. Same behaviour as Rack's native undo stack hitting its depth limit.
- No crash, no corruption, no special handling at the boundary. Silent drop of the oldest entry.
- Edge case: if the write pointer laps the read pointer (new scatter events recorded while
  simultaneously navigating undo history), the overwritten region is marked unavailable.
  In practice this means "more scatter events than buffer depth while navigating history" --
  essentially impossible in real use, but the policy handles it correctly rather than leaving
  it undefined.
- Implementation: standard circular buffer (head/tail indices into a fixed pre-allocated array).
  Pre-allocate generously (e.g. 1,000 entries = 28KB) -- simple, no dynamic allocation, wrap
  policy is correct by construction.

With transform optimisation (invertible ops stored as op-codes ~2-5 bytes, not 28-byte entries)
the effective depth for non-invertible events is even greater, since invertible transforms
consume almost no buffer space.

### CA state is small enough to track generously (Rodney)
The pin matrix is 16 bytes (uint8_t src[16]). A circular snapshot buffer of 32-64 entries =
512B-1KB. Trivially small. This means:
- Scatter undo depth can be as generous as dice counter-rewind undo in practice, even though
  the mechanism differs (snapshots vs counter arithmetic).
- Phase coherence path: snapshots tagged with phrase-boundary index give the phase-driven
  sequencer a way to restore the right correlation when phase scrubs backward past a boundary
  ("at phrase boundary N, pin state was X"). Not as clean as pure counter-rewind, but
  manageable given the state size.
- Effective depth: 32-64 scatter events before the oldest snapshot is overwritten. More than
  enough for any realistic performance session.

### Transform storage optimisation: invertible vs fan-in (Rodney)
Not all CA state changes need a 16-byte snapshot. Transforms split into two categories:

**INVERTIBLE transforms -- store the OP, not the state (near-zero cost):**
- Reflect: self-inverse. Undo = re-apply. Store: 1 byte (transform type).
- Rotate +k: undo = Rotate -k. Store: transform type + k parameter (~2 bytes).
- ScatterRows: genuine Fisher-Yates permutation. Undo = inverse permutation, or re-derive
  from same seed. Store: transform type + seed (~5 bytes).
No pin matrix snapshot needed for any of these. Cost is essentially free.

**FAN-IN transforms -- must snapshot the pin state (16 bytes):**
- Scatter: fan-in (many->one), no inverse transform. Undo requires pre-transform state.
- interScatter: same.
- Collapse: fan-in, "no inverse" (ChangeAlleyTransforms.hpp). Same.
Cost: 16 bytes per event.

**Tiered undo buffer:**
The undo buffer only spends its 16 bytes on fan-in events. Invertible transforms are an
op-code + parameter entry (a few bytes each). So effective undo depth is much greater than
raw event count suggests -- a large fraction of transform events are cheap. The same tiering
applies across the whole undo system: anything with a clean inverse (Reflect, Rotate, direction
cycle, knob drag before/after) costs near-zero; only state-destroying operations (Scatter,
Collapse, dice promotion in non-reversible mode) need the full snapshot.

## Change Alley scatter -- the remaining state dependency (and why it's categorically different)

With phase gated to reversible mode, the dice buffer incoherence (LockedA/CandB vs nonlinear
transport) is contained -- non-reversible mode is forward-only so buffers are coherent there by
construction. The scrub model is the right eventual fix if phase ever lifts its reversible
restriction, but not urgent now.

That leaves Change Alley scatter as the MAIN REMAINING state dependency.

### What the state is
scatterCounter[CA::SIDES * CA::TYPES * 2] (MonsoonChangeAlleyV2.hpp:42): an ACCUMULATED GATE
EVENT COUNT. Each scatter trigger gate increments the counter; the counter seeds the Philox
correlation draw for that scatter permutation. Output at any moment depends on HOW MANY SCATTER
TRIGGERS HAVE FIRED SINCE RESET -- a time-ordered historical fact, not a phase position.

### Why it's categorically different from dice buffers
Dice buffer state COULD become phase-derivable (scrub model: phase -> counter position ->
at(floor) blended at(ceil)). Scatter counter CANNOT -- even in principle -- because it counts
EXTERNAL GATE EVENTS (CV input), not phase. You cannot look at the phase position and know how
many scatter gates have fired; that depends on an external signal independent of phase. There is
no "phase-derivable" version of scatter state. This is a permanent architectural reality for
event-driven, externally-triggered state.

### The correct resolution: pin-state snapshot undo (already specced)
Scatter's undo is a PIN-STATE SNAPSHOT (UNDO_PLAN section above), not a counter rewind. The
snapshot captures the pin matrix BEFORE the scatter fires; undo restores it. This is the right
mechanism for event-driven permutation -- different from the Philox counter rewind for dice, but
correct for what scatter IS. Already in the Change Alley design.

### Phase coherence
Under nonlinear phase (if the restriction were ever lifted), scatter would be out of sync with
musical position -- the counter reflects "N gates fired" which backward phase cannot unwind.
This is an ACCEPTABLE limitation for event-driven state: scatter is a deliberate human gesture
(a gate input you patch and trigger), not a continuous generative parameter. The user controls
when scatter fires; accepting that those events don't rewind with phase is reasonable. The
alternative (making scatter phase-derivable) is architecturally impossible since the gate input
is independent of phase.

## Clarification: undo vs reversible mode for dice and Change Alley (IMPORTANT DISTINCTION)

The snapshot buffer discussion above conflated TWO different mechanisms. They need the same
raw data but serve different purposes with different access patterns:

**UNDO (Rack Ctrl+Z, user-initiated)**
- User says "I didn't want that event." Pop most recent snapshot, restore state. One step at
  a time, backward only, user-triggered. Feeds the Rack history stack (StoreEditAction).

**REVERSIBLE MODE (transport-driven, phase rewind)**
- Transport scrubs backward; engine reconstructs what state WAS at that position automatically.
  Seek to the nearest boundary-indexed snapshot, transport-driven, may jump many entries.

Different data structures: stack for undo, indexed circular buffer for reversible. A single
buffer could serve both but the access patterns differ and should be decided deliberately.

### Dice: reversible mode is already coherent and implemented
"Reverse roll" = decrement the Philox counter, re-derive the pattern. The counter IS the state;
nothing else to restore. One operation, complete. Already implemented and correct.

### Change Alley: undo is specced; reversible mode is an OPEN PROBLEM
**CA undo** (Ctrl+Z): specced above. Snapshot-based (pin matrix + scatter counter per event).
Straightforward. Invertible transforms store op-code only; fan-in transforms store 16-byte
snapshot. This is the mechanism the snapshot buffer spec above describes.

**CA reversible mode** (transport-driven): does NOT yet exist and is NOT straightforward.
There is no "reverse roll" equivalent for CA because:
- CA state isn't counter-addressed the way dice is. A scatter event permutes pins AND advances
  the scatter counter; reversing it requires restoring BOTH -- a snapshot restore, not a counter
  decrement.
- Unlike dice where the counter uniquely re-derives the state, the current pin matrix depends on
  the ENTIRE HISTORY of scatter events + manual edits + transform triggers since last reset. No
  single counter value re-derives it.
- CA state changes come from MULTIPLE SOURCES (scatter gates, manual drags, transform triggers)
  with different reversal mechanisms. Dice has one source (roll) with one reverse (counter
  rewind). CA has several sources, one of which (manual edits) is already accepted as
  non-reversible.

Status: CA reversible mode CAN work off the same snapshot buffer IF manual edits are also
written to it (alongside the Rack history stack). The buffer then contains the COMPLETE history
of CA pin state regardless of source (scatter gate, transform, manual drag). Transport rewind
becomes: seek backward through the buffer to the snapshot nearest the target phrase-boundary
index, restore it. Same data, same buffer, different access pattern from undo (seek vs pop).

Requirement: manual drag gestures write to TWO targets on completion:
  1. Rack history (StoreEditAction) -- for Ctrl+Z, as now.
  2. Snapshot buffer entry with current phrase-boundary tag -- for phase-coherent restoration.
At 28 bytes per entry this is not expensive. The provisional "manual edits non-reversible"
decision is SUPERSEDED -- they become reversible via the buffer.

Boundary tagging for manual edits: tag with the CURRENT phrase-boundary index at the time of
the drag completion. On rewind, a manual edit made at phrase boundary 47 is restored when
transport rewinds to boundary 47. Correct -- it reconstructs the correlation state the user
had set up at that point in the session.

CA reversible mode is therefore NOT an open problem if the buffer includes all sources. It
becomes: seek the buffer by boundary index, restore (pin matrix + scatter counter). The phrase-
boundary index already in the 28-byte entry spec is exactly what enables this.

## Manual Change Alley pin edits -- reversibility TBD, likely accepted as non-reversible

Manual pin drags are a different character from transform events:
- They happen continuously during a drag gesture (many per second).
- They're already handled by StoreEditAction snapshots via Rack's native Ctrl+Z undo.
- Tracking them for phase-coherent reversal would require either buffering every drag frame
  (expensive, noisy) or coarsening to gesture-level snapshots (complex boundary detection).
- Complexity-to-benefit ratio is poor.

The natural scope boundary for phase-coherent reversibility is ENGINE-DRIVEN state changes
(what the engine does autonomously as the sequencer runs). Manual edits are USER-INITIATED;
accepting them as committed state is both simpler and arguably CORRECT -- the user meant to
change that pin.

Two separate undo mechanisms serving different questions:
- Ctrl+Z (StoreEditAction): "I changed my mind about that edit." Already works.
- Phase-coherent reversal: "take me back to where the ENGINE was at position N." Applies to
  scatter events and transform triggers, not manual gestures.

TBD: confirm in play that not having manual edits phase-reversible doesn't cause real friction.
Expectation: it won't, because manual editing and phase-scrubbing are different modes of use
(you don't typically scrub phase while actively dragging pins). Likely ACCEPTED as non-reversible.

## Undo / reversible mode interaction policy

The two mechanisms index the same snapshot buffer from different directions with different
semantics. UNDO is user-time (what did I do last); REVERSIBLE is transport-time (where is the
transport now). Explicit policy needed because they can conflict.

### The four cases

**Case 1: Transport stopped, user undoes.** Clean. No conflict. Pop most recent buffer entry.

**Case 2: Transport running forward, no undo.** Clean. Reversible mode advances with transport,
buffer grows. No conflict.

**Case 3: Transport reverses, no undo.** Clean. Reversible mode seeks backward in the buffer,
restores state at each phrase boundary. Undo stack UNTOUCHED -- the rewind does not pop undo
entries, it seeks the buffer. The undo stack still reflects what the user DID, not where the
transport is.

**Case 4: Transport reversed AND user hits Ctrl+Z simultaneously.** THE CONFLICT.
Transport has seeked to position N; user undoes -- but undo of WHAT? The most recent user action
(which may be ahead of the current transport position), or the state at the transport position?

### Two principled options

**Option A -- UNDO IS USER-TIME, INDEPENDENT OF TRANSPORT POSITION (RECOMMENDED)**
Ctrl+Z always undoes the most recent user action regardless of where transport has seeked. If
transport is at boundary 30 but user's last action was at boundary 45, Ctrl+Z removes boundary
45's entry. Transport continues from boundary 30 unaffected.
- Clean separation: undo = user-time, seek = transport-time, they don't interfere.
- Simple implementation: undo pointer is purely recency-ordered, not position-sensitive.
- Edge case: user can "undo a future action" (an action ahead of the current transport
  position). This may feel slightly odd but is unambiguous and correct -- the user said "that
  didn't happen," regardless of where the transport currently is.

**Option B -- UNDO IS RELATIVE TO TRANSPORT POSITION**
Ctrl+Z undoes the most recent user action AT OR BEFORE the current transport position. Entries
after the transport's current position are treated as "in the future" and ignored by undo.
- More musically coherent: you can't undo something that "hasn't happened yet" from the
  transport's perspective.
- More complex: undo pointer becomes a function of BOTH recency AND transport position.
  Makes the undo stack position-sensitive, harder to implement and reason about.

### Recommended policy: OPTION A
Clean separation, simple implementation. Avoids making Ctrl+Z position-sensitive. The "undo a
future action while transport is rewound" edge case is unambiguous even if slightly odd.
Option B is a possible refinement if play reveals that the edge case causes real confusion.

### Forward-advance after undo
If the user undoes an entry and the transport then advances past the undone region: the entry
is gone from the buffer; the transport uses whatever state the buffer currently holds. This is
correct -- the undo was intentional ("that didn't happen") and the transport advancing again
simply uses the current state. No special handling needed.
