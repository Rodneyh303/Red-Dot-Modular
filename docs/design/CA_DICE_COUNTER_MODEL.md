# Change Alley scatter counters -> true dice Philox model (8 streams, addressable position)

## What was wrong before
- Collapse 8->2 (commit bfdb586, REVERTED a521734): wrong. The 8 counters are real -- the panel
  exposes separate jacks for Intra/Inter x rhythm/melody x domain/codomain, each an independent
  scatter control. Forward/back is the SIGN of scatterDelta (+1/-1), not a separate counter.
  8 = SIDES(Intra/Inter=2) * TYPES(rhythm/melody=2) * (domain/codomain=2).

## The correct reform (Rodney): keep 8, make them DICE-STYLE
Not fewer counters -- BETTER counters. Adopt the main-dice Philox model for each of the 8:
- Counter type = signed int64 (same as dice rhythmDrawCtr/melodyDrawCtr). Negative allowed.
- The counter IS the addressable POSITION in that stream's Philox sequence -- NOT a seed/key.
  Draw via rng.at(position), so at(N) and at(N-1) are stable neighbouring draws.
- Back-jack does counter-- -> at(N-1) returns EXACTLY the previous draw. NO reseeding. Forward
  counter++ -> at(N+1). Pure addressable rewind, exactly like dice reverse.
- Each of the 8 is domain-separated (its own fixed key) so the 8 streams don't correlate and each
  rewinds independently. The FIXED key encodes which of the 8 (ci); the COUNTER is pure position.

### Before (seed-keying -- what we're replacing)
applyCorrelation extracted seed = counter & 0xFFFFFFFF and each transform built
correlationRng(seed) then drew rng.at(voice). => counter was the KEY (a new random sequence per
count), voice was the position. Decrementing the counter jumped to a DIFFERENT permutation, not a
smooth addressable step back.

### After (addressable position -- dice model)
The correlation RNG is keyed by a FIXED per-stream domain (ci in 0..7, mixed with the correlation
nonce). The counter is the position page: voice v of counter N draws rng.at(N*16 + v) (a stable
16-wide page per counter). counter-- returns to the prior page exactly. No reseed.

## Deferred (separate concern -- Rodney): pre-scatter PIN / fan-in undo
scatter() has fan-in (multiple voices can source the same slot) => NO inverse TRANSFORM. Today undo
is a pre-scatter pin-state snapshot (store snapshot). The addressable-position counter makes the
DRAW reversible, but the fan-in means re-deriving the board by stepping the counter back is not the
same as restoring the pins. Reconcile "counter rewind" vs "pin snapshot" undo as its own step AFTER
this counter-model change lands. Do not touch the pin-snapshot undo in this change.

## Undo restores pin POSITION; reverse-dice does NOT (Rodney) -- draw-reversible != state-restorable
Key distinction (corrects the muddle of calling scatter "reversible" as if that answered undo):

- UNDO restores the pin matrix to its prior STATE (via the snapshot ring). Puts the pins back where they
  were. A STATE restoration.
- REVERSE DICE (scatterCounter--) re-derives the previous DRAW (Philox at(N-1), exact) -- but that's the
  previous random VALUE, which drives a scatter TRANSFORM. It does NOT restore the previous pin position.

### Why they differ: scatter is a STATE-DEPENDENT TRANSFORM, not a set-to-value
Reverse-dice reverses the DRAW SEQUENCE; undo reverses the STATE. Not the same "back". Reverse-dice is NOT
an undo: decrementing the counter and re-scattering does NOT return the matrix to its previous config,
because scatter TRANSFORMS the current pins -- applying "the previous scatter draw" to the CURRENT (already-
transformed) pins yields a NEW state, not the OLD one. Scatter is PATH-DEPENDENT (result depends on where
the pins currently are), so re-running an earlier DRAW doesn't reproduce an earlier STATE. Only the
snapshot (which recorded the actual state) can restore the state.

### Why the code has BOTH mechanisms (not redundant)
- Snapshot ring / Rack history exists PRECISELY because reverse-dice can't restore pin state. If reverse-
  dice could undo, no snapshot would be needed -- just decrement. The snapshot provides the state-
  restoration reverse-dice doesn't.
- Reverse-dice is a MUSICAL/GENERATIVE gesture (scrub the scatter sequence backward -- hear earlier
  scatters applied to current pins), NOT an edit-undo.

### The clean statement
- Want the previous pin POSITION back -> UNDO (snapshot restores state).
- Want to hear/apply the previous scatter DRAW -> REVERSE DICE (re-derives the draw, transforms current
  pins with it).
Draw-reversibility (generative scrubbing, signed Philox counter) and state-undo (editing, snapshot ring)
are DIFFERENT NEEDS served by DIFFERENT MECHANISMS. Conflating them is the error: draw-reversible !=
state-restorable, because scatter is a state-dependent transform. So "the scatter draw is reversible" is
TRUE but does NOT mean "reverse-dice restores the pins" -- it doesn't; undo does.

Cross-ref: MonsoonChangeAlleyV2.hpp:62-67 (signed scatterCounter, at(N-1) rewinds the DRAW exactly),
:92-106 (snapshot ring + Rack history = the STATE undo), the CA scatter-reversibility discussion (this
resolves it: draw reversible, state restored by snapshot not by reverse-dice; scatter is path-dependent)." 

## State-dependent scatter (current) vs a stateless/absolute alternative (Rodney) -- the design fork
Two models:
- STATE-DEPENDENT (current): scatter TRANSFORMS the current pin state -> new state. The draw = "how to
  rearrange FROM HERE". Path-dependent.
- STATELESS/ABSOLUTE (alternative): the draw specifies an ABSOLUTE pin config directly -> "the pins ARE
  this", regardless of what they were. Each draw = a complete target state = f(counter, key). Path-
  independent.

### Comparison
| Dimension                     | State-dependent (current)        | Stateless/absolute            |
| Reverse-dice = undo?          | NO (needs the snapshot ring)     | YES (decrement restores)      |
| Mechanisms                    | Two (signed counter + snapshot)  | One (counter alone)           |
| Respects hand-placed pins?    | YES (perturbs them)              | NO (obliterates them)         |
| Successive scatters           | EVOLVE (a trajectory)            | Independent teleports         |
| Composes with rotate/reflect? | YES (all 4 verbs chain)          | NO (scatter = reset, odd one) |
| Reproduce a state from        | initial + path                   | counter ALONE                 |

### What each wins
- STATELESS wins on SIMPLICITY: reverse-dice IS undo (at(N-1) reproduces the exact config -> snapshot ring
  UNNECESSARY, one mechanism serves scrub + undo), and any state re-derivable from its counter alone (no
  history). Cleaner.
- STATE-DEPENDENT wins MUSICALLY: scatter BUILDS ON what you did -- hand-place pins, scatter PERTURBS from
  there (your manual structure preserved-and-varied); successive scatters ACCUMULATE/EVOLVE (a trajectory
  of related configs); and all four verbs COMPOSE as a transform pipeline on the evolving matrix. Stateless
  OBLITERATES hand-placed pins on every scatter (absolute draw ignores your edits), makes successive
  scatters INDEPENDENT jumps, and breaks verb composition (an absolute scatter is a RESET, not a transform
  -- rotate-then-absolute-scatter loses the rotate).

### Why STATE-DEPENDENT is right FOR CA specifically
CA is a correlation AUTHORING + PERTURBATION instrument: you HAND-PIN correlations, then scatter/rotate/
collapse to VARY them. That workflow REQUIRES state-dependence -- scatter must build on your pins (not wipe
them), and the four verbs must COMPOSE as a pipeline. A stateless scatter would turn CA from "shape-and-
perturb" into "hand-place OR randomize", losing the INTERPLAY between manual + generative that's the whole
point. The snapshot-ring cost (and reverse-dice != undo) is the PRICE of that richness -- and worth paying
FOR CA, because perturb-your-own-structure is central.

Explains the snapshot ring: it exists as a DELIBERATE COST of state-dependence, not an accident. If CA were
stateless, no snapshot needed (reverse = undo). The two-mechanism design is the chosen trade for musical
perturb-and-compose.

### When stateless would be right
A DIFFERENT module -- one where scatter IS meant to be a teleport to fresh configs, and simplicity /
reverse-as-undo / counter-alone reproducibility matter more than perturb-and-compose. Genuine "depends what
the module is for" fork; CA's purpose (author + perturb + compose) picks state-dependent.

Cross-ref: the undo-vs-reverse-dice section above (why the two mechanisms exist -- this explains the
state-dependence BEHIND that), MonsoonChangeAlleyV2.hpp:92-106 (the snapshot ring = the cost of state-
dependence), :182 (scatter as a transform verb that composes with collapse/rotate/reflect)." 

## Snapshot ring: size + limits (Rodney)
UNDO_RING = 16 (MonsoonChangeAlleyV2.hpp:104). 16-slot single-producer/single-consumer ring. Each slot
(TransformUndoSnapshot, :96-103) holds: beforeR/M[N_VOICES] + afterR/M[N_VOICES] (uint8 pin sources
before+after) + counterBefore/After[8] (int64 scatter counters before+after) = a full before/after bracket
of one transform's effect on BOTH the pin matrix AND the 8 scatter counters. ~192 bytes/slot x16 ~= 3 KB.
Trivial memory.

### It is a THREAD HAND-OFF ring, NOT a deep undo history
Transforms run on the AUDIO thread where APP->history->push is illegal (UI-thread only). So the audio
thread SNAPSHOTS into this ring; the widget step() on the UI thread DRAINS it and turns each snapshot into
a proper Rack history action (:939+). So the 16 slots buffer audio-produce -> UI-consume; they are NOT the
undo depth. Real undo depth = Rack's own history stack (which these feed).
- The "16" is a DRAIN-LATENCY cushion, not an undo cap: push only if h - t < 16 else DROP ("drop if UI
  hasn't drained (never in practice)", :238). Overflow needs 16 transform-commits between two UI frames --
  never happens (transforms commit at phrase boundaries, UI drains ~60Hz+). 16 = generous produce/consume
  headroom.
- Granularity: one commit = one snapshot = one undo step (:195,207 "a multi-change gesture is a single
  snapshot"). A phrase-boundary commit applying several armed rows = ONE undo step (undo the gesture, not
  each row).
KEY: 16 is NOT your undo depth -- it's the thread-safety buffer size. Undo far more than 16 (Rack history);
the ring just needs to be big enough the audio thread never outruns the UI drain.

## PROPOSAL (Rodney): add a TRUE REVERSE on CA, alongside dice-reverse + undo
Three distinct "backs", each for a different NEED -- true reverse fills a real gap:
- UNDO = EDIT (restore a state, discrete, UI-thread, Rack history).
- DICE REVERSE = generative DRAW-scrub (scatterCounter--, re-derive the draw, apply to CURRENT pins -> new
  state; does NOT restore state).
- TRUE REVERSE (proposed) = PERFORMANCE: replay the actual STATE TRAJECTORY backward, audio-rate, as a
  played gesture. Neither of the others does this (dice = draws not states; undo = edits not performance).

### It must be TRAJECTORY-REPLAY (well-defined), NOT transform-inversion (partial/redundant)
- (1) Trajectory replay: walk the recorded STATE history backward, musically -- replay the actual states
  passed through. ALWAYS well-defined (uses recorded states, not inverse math). ROBUST reading.
- (2) Transform inversion: invert each verb. Rotate^-1/reflect^-1/scatter^-1 exist (permutations), but
  COLLAPSE is likely LOSSY -> non-invertible. So (2) is only PARTIALLY defined (breaks on collapse) AND
  redundant for the invertible verbs (rotate-back = rotate-other-way, already available). WEAKER.
So build it as (1): musical replay of the recorded state trajectory backward.

### It needs its OWN state-history buffer (new storage)
The snapshot ring is a 16-slot thread HAND-OFF (not a deep trajectory); Rack history is EDIT-oriented
(discrete, UI). Neither is an audio-rate-PERFORMABLE state trajectory. So true reverse needs a BOUNDED
state-trajectory buffer -- a "reverse ring" of past pin-states playable backward at audio rate -- distinct
from the SPSC hand-off ring and Rack history. Cost = that storage + a depth limit (how far back you can
reverse = N states, like a delay line of states).

### Why it composes cleanly (not redundant)
Three backs, three intents: EDIT (undo) / GENERATE (dice reverse) / PERFORM (true reverse). Each does what
the others can't. True reverse = the PERFORMANCE-domain back neither undo nor dice-reverse provides (play a
scatter evolution forward, then reverse it LIVE -- a retrograde/palindrome of the correlation journey).

### Design questions to pin
- Buffer DEPTH (how many past states reversible through).
- Momentary (hold to play backward, release to resume forward) vs toggle (flip to permanent retrograde).
- Does it interact with dice-reverse (are they exclusive, or does true-reverse subsume the scrub)?

Cross-ref: the state-dependent-vs-stateless section (path-dependence is WHY dice-reverse != state restore,
hence the need for a recorded trajectory to true-reverse), the undo-vs-reverse-dice section (the two
existing backs; true reverse is the third), MonsoonChangeAlleyV2.hpp:96-107 (snapshot ring -- the hand-off,
NOT the trajectory buffer true-reverse needs)." 

## True-reverse buffer size: tiny, because states change ONCE PER PHRASE (Rodney)
Cadence check (MonsoonChangeAlleyV2.hpp): user actions (pin edits, verb triggers) latchRow -> set a row
ARMED = pending (:158-184); pending transforms ALL commit at the PHRASE BOUNDARY (applyPendingTransforms,
:188,206 "one phrase-boundary commit = one undo"). So it's NOT two cadences (UI-rate edits + phrase-rate
mod) -- BOTH user edits AND modulation latch pending and commit TOGETHER at the phrase boundary. The UI-
rate part is only the ARMING (click -> pending light); the actual STATE CHANGE is at the phrase boundary.
So effectively ONE state-change cadence: the phrase boundary. (Rodney said "probably next step boundary" --
code says PHRASE boundary for the commit; confirm the mental model, but it's boundary-gated either way, a
musical boundary not continuous.)

### So the buffer is TINY and very practical
The state-trajectory buffer needs ONE ENTRY PER PHRASE BOUNDARY (the only time a new state exists). Depth =
number of PHRASES to reverse through, not samples/steps:
- A phrase is seconds (many steps). At ~2-8 s/phrase: 32 entries ~= 1-4 min of reversible history; 64
  ~= 2-8 min; 128 ~= 5-15 min. A LOT of musical reverse-reach.
- Memory/entry ~= a snapshot (~96-192 bytes: state pins + counters, or before/after). 64 entries ~= 6-12
  KB. Trivial (cf. the existing snapshot ring ~3 KB). Even 128 (~24 KB) is trivial.
So practical size = 32-64 phrase-states, ~6-12 KB. The phrase-boundary cadence is WHAT MAKES A LONG
REVERSE-REACH CHEAP: per-sample states would need a huge buffer; once-per-phrase, a few dozen entries = 
minutes of music.

### True-reverse is PHRASE-GRANULAR, reusing snapshot data (corrects "audio-rate replay")
It steps BACKWARD through the phrase-boundary states -- each reverse step jumps to the previous phrase's
committed pin state. Phrase-granular (retrace the committed phrase-states in reverse), NOT a smooth audio-
rate morph. Musically right: states are phrase-quantised forward, so reversing them is phrase-quantised
too. ("Audio-rate replay" was the wrong framing -- it's PHRASE-rate replay, one state per phrase played
backward, matching how they were laid down.)

### Nice efficiency: reuse the snapshot data you ALREADY capture
The phrase-boundary commits are ALREADY snapshotted (the 16-slot hand-off ring captures every commit's
before/after pins + counters). So the true-reverse buffer = a DEEPER version of the SAME snapshot data --
not new KINDS of data, just KEEPING MORE of it. Extend state-history depth from "16-slot thread hand-off"
to "~64-slot reverse trajectory". The snapshot CONTENT is already exactly what true-reverse needs; just
retain a deeper history for the performance-reverse.

### Net
Very practical: 32-64 phrase-states (~6-12 KB, 128 still trivial), phrase-granular, reusing the existing
snapshot content at greater depth. The once-per-phrase cadence makes minutes of reverse-reach nearly free.
Depth (32/64/128) = a taste choice (how far back to reverse), not a memory constraint.

Cross-ref: MonsoonChangeAlleyV2.hpp:158-184 (latchRow = arm pending), :188-206 (applyPendingTransforms =
phrase-boundary commit, one commit = one undo), :96-107 (snapshot ring content = what the deeper reverse
buffer reuses), the true-reverse proposal above (this sizes its buffer: phrase-granular, ~64 entries)." 

## True-reverse x undo x lock: yes, IF built to two rules (Rodney)
Three "backs" plus lock all touch the same pin state -- they must compose without corrupting each other.
Mechanisms: lock gates COMMITS via axisMask (locked axes' rows stay ARMED/pending, commit at next in-axis/
unlock, :191-220); undo = snapshot ring -> Rack history (edit history of commits); true-reverse = replay
the recorded phrase-state trajectory backward.

### Interaction 1: true-reverse x UNDO -> rule (a): true-reverse is PERFORMANCE, not an edit (no undo writes)
If true-reverse pushed each reverse-step onto Rack undo history -> mess (reverse 8 phrases = 8 undo
entries; undoing them reverses the reverse; circular). BAD. So true-reverse must NOT touch undo history --
it changes the live pin state for PLAYBACK but records no edits. Then undo still refers to the EDIT history
(pin edits + commits), independent of where true-reverse scrubbed. CLEAN -- and it's the SAME principle as
dice-reverse (generative, not an edit, doesn't push undo). Treat true-reverse like a TRANSPORT DIRECTION,
not an edit: undo-during-reverse undoes the last EDIT, not the reverse gesture (like "undo doesn't undo the
playhead moving").

### Interaction 2: true-reverse x LOCK -> rule (b): true-reverse RESPECTS the lock axisMask
Lock keeps some axes' rows pending (deferred commits). The trajectory is of COMMITTED states, so locked
(pending) changes aren't in it -- correct (locked things didn't happen, nothing to un-happen; they stay
pending through a reverse, commit on unlock as normal). BUT: true-reverse changes the live pin state to a
past state -- it must NOT overwrite a LOCKED axis. So true-reverse RESPECTS the lock axisMask: reverse only
UNLOCKED axes' states, leave LOCKED axes frozen. Same gating as commits (:218-220). Keeps lock's meaning
("this axis doesn't change") intact across the reverse gesture. Without this, true-reverse would VIOLATE
lock (reversing a locked axis) = a real bug.

### The two rules -> clean three-way + lock composition
- (a) true-reverse = performance op, NO undo-history writes (like dice-reverse).
- (b) true-reverse RESPECTS the lock axisMask -- reverses unlocked axes only, leaves locked frozen (like
  commits).
Then: UNDO = edit history (independent of playback); DICE-REVERSE = generative draw-scrub; TRUE-REVERSE =
performance playback of committed UNLOCKED-axis trajectory (respects lock, no undo writes); LOCK = freezes
an axis against ALL state change (commits AND true-reverse). Each has a clear domain; they compose.

### Both rules REUSE existing patterns (pattern de-risking)
(a) dice-reverse already doesn't write undo; (b) commits already respect axisMask. So true-reverse isn't
NEW interaction logic -- it applies the engine's existing "performance ops don't edit" + "lock gates state
change via axisMask" rules to the new gesture. Not-automatic-but-not-novel: a naive true-reverse that
pushed undo or ignored lock WOULD break both; built to the two existing patterns, it composes.

### Edge to VERIFY when building
What does the trajectory buffer record UNDER LOCK? A phrase committing with some axes locked records a
PARTIAL change (only unlocked axes changed). Reversing should be fine IF the buffer records POST-COMMIT
states (reflecting whatever lock allowed), so reverse retraces ACTUAL history including the lock-gating
that happened. The snapshot already captures post-commit before/after, so this should be inherent -- but
VERIFY: the trajectory must record actual committed states (lock-gated as they were), so reverse retraces
true history.

### Answer
YES, true-reverse works with undo and lock -- IF built to the two rules (performance-not-edit -> no undo
writes; respect lock axisMask). Not automatic (naive versions break both), but not novel either (both
rules are existing patterns). Verify the buffer records actual lock-gated committed states.

Cross-ref: MonsoonChangeAlleyV2.hpp:191-220 (axisMask lock gating -- rule b reuses this), the undo-vs-
reverse-dice section (dice-reverse doesn't write undo -- rule a reuses this), the true-reverse proposal +
buffer-size sections above (this checks its interactions), ROAD_TO_RELEASE (pattern de-risking: reuse
existing patterns)." 

## CORRECTION (Rodney): a state-reverse is JUST ANOTHER ACTION -- I overcomplicated it
Rodney: "isn't a state reverse just like any other action for undo?" YES -- and my prior "two special
rules" (keep true-reverse OUT of undo; separately make it respect lock) were OVERBUILT. Correcting:

### The "circular undo" worry was MANUFACTURED
I claimed pushing reverse-steps to undo history = circular ("reverse 8 = 8 entries, undoing reverses the
reverse"). WRONG. A true-reverse CHANGES the pin state; every pin-state change already snapshots -> one
undo action. So reverse 3 phrases = 3 state-jumps = 3 history entries; undo x3 steps FORWARD 3 = back to
start. That's not a mess -- it's undo working NORMALLY. Not circular; correct.

### A state-reverse IS a normal state-committing action
It produces a new state, snapshots before/after like collapse/rotate/reflect/scatter, pushes ONE history
action, and undo undoes it like any other. No "performance op that bypasses undo". I imported the DICE-
reverse model (legitimately outside undo because... it also produces a state that snapshots, so even that
was muddled) and wrongly special-cased true-reverse. The honest model: EVERY op that commits a pin-state
change (all verbs, fwd or reverse, AND true-reverse) produces a state, snapshots, is ONE undoable action.
Undo = step back through the committed-state history; everything that commits is IN that history.

### What true-reverse IS, distinct from undo (simpler than I said)
Both walk back through committed states. The distinction is the TRIGGER, not the mechanism:
- UNDO = step back through committed-state history at EDIT/UI pace, as an editing action (redo-able).
- TRUE-REVERSE = step back through the SAME committed states at MUSICAL/PHRASE pace as a PLAYED gesture.
= "undo, but CLOCKED/performed" vs "undo, but clicked". Possibly the SAME mechanism (walk the state
history backward), different trigger (phrase clock vs UI click). So true-reverse works with undo because
it essentially IS undo, clocked.

### Lock comes for FREE (also not a special rule)
If true-reverse is a normal state-committing action, it goes through the SAME applyPendingTransforms /
axisMask gating as everything else -> lock handling is automatic. Not a special "true-reverse respects
axisMask" rule -- it's "true-reverse is an action, actions respect axisMask, done."

### Corrected conclusion
A state-reverse is just another action: snapshotted, undoable, lock-respecting FOR FREE via the existing
commit machinery. Scrap the two special rules. The ONLY real design choice is cosmetic/UX: is true-reverse
TRIGGERED by the clock (performance) or the user (edit)? -- but mechanically it's the same undoable, lock-
respecting state action either way. Massively simpler + more robust than my "performance-op-bypasses-undo"
framing.

Supersedes the "true-reverse x undo x lock: two rules" section above (that overbuilt it; the circular-undo
worry was manufactured; a state-reverse is a normal undoable, lock-respecting action -- undo + lock handle
it for free because it uses the existing commit path).

Cross-ref: MonsoonChangeAlleyV2.hpp:188-220 (applyPendingTransforms + axisMask -- the commit path true-
reverse would use, giving undo+lock for free), :96-107 (snapshot ring -- a state-reverse snapshots like
any commit), the true-reverse proposal + buffer sections (unchanged: still a phrase-granular state-history
walk; just NOT special-cased for undo/lock)." 

## CORRECTION^2 (Rodney): undo is for USER actions only; modulation changes are NOT undoable
Rodney: "undo is only for user actions right? changes via modulation are not part of undo?" YES -- and this
corrects my previous "a state-reverse is just another undoable action" (which wrongly treated ALL state
commits as uniformly undoable).

### The principle (standard, and correct)
Undo is for USER EDITS, not for MODULATION. If an LFO/envelope modulates the correlation every phrase and
EACH modulated commit pushed an undo entry, the undo stack fills with automation churn you didn't do --
hit undo and you undo the LFO's last move, not YOUR last edit. Useless. So: automation/modulation changes
are NOT undoable events; only user edits are. (You don't undo a filter-cutoff LFO wobble.)

### This corrects my "everything's undoable" oversimplification
My last message treated all state commits as uniformly undoable. Wrong: modulation commits change state
but must NOT be undoable, and they go through the SAME commit path (applyPendingTransforms) as user commits.
So the real principle is finer: TWO CLASSES of state change --
- USER-triggered commits: undoable (push a TransformUndoAction to Rack history).
- MODULATION/CLOCK-triggered commits: change state but do NOT push undo history.
The commit PATH is shared; the HISTORY-PUSH must be GATED on the class (what ARMED the row: user via
latchRow-from-button/pin -> push; modulation -> don't).

### Reshapes true-reverse (my ORIGINAL instinct was right, for the WRONG reason)
True-reverse is CLOCKED (a performance gesture), NOT a user edit -> it's in the MODULATION/PERFORMANCE
class -> it should NOT push undo history, for the SAME reason modulation doesn't. Not because true-reverse
is "special" (my ad-hoc reason last time), but because it's MODULATION-CLASS, and that whole class is
excluded from undo. So "keep true-reverse out of undo" is correct -- via the general class rule, not a
special case.

### Corrected coherent model
- Two commit classes: USER-triggered (undoable, pushes history) + MODULATION/CLOCK-triggered (not undoable,
  no push).
- Shared commit PATH (applyPendingTransforms); history-push GATED on class (user vs modulation arming).
- True-reverse = modulation-class (clocked) -> no undo history, like all modulation.
- Lock gates both classes via axisMask (unchanged).

### VERIFY (possible latent bug): does modulation currently pollute undo?
Code (MonsoonChangeAlleyV2.hpp:94,956,989) pushes a TransformUndoAction "per committed transform" without
an OBVIOUS user-vs-modulation distinction at the push point; rows are armed by user (latchRow from button/
pin) OR by modulation, then applyPendingTransforms commits + pushes. So on the current code, MODULATION-
driven commits may ALSO be pushing undo history = WRONG by this principle (modulation polluting undo).
VERIFY: does applyPendingTransforms push TransformUndoAction for MODULATION-armed commits? If yes -> a
latent bug: gate the history-push on user-vs-modulation arming (only user-armed commits push). May be a
gating I can't see from greps; check in the code.

Supersedes "a state-reverse is just another undoable action" (that missed the user-vs-modulation class
split). Correct: undo is USER-only; modulation (and true-reverse, being clocked) changes state WITHOUT
undo; the split is by what ARMED the commit, gated at the history-push.

Cross-ref: MonsoonChangeAlleyV2.hpp:94/956/989 (TransformUndoAction push per commit -- VERIFY it's gated to
user-armed only), :158-184 (latchRow -- armed by user button/pin OR by modulation; the class distinction
lives at arming), :188-220 (applyPendingTransforms shared commit path), the prior two corrections (this is
the finer-grained truth: not 'all undoable', not 'true-reverse specially excluded' -- but 'user-class
undoable, modulation-class not, true-reverse is modulation-class')." 
