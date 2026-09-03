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
