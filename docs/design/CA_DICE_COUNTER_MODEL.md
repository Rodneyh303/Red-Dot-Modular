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
