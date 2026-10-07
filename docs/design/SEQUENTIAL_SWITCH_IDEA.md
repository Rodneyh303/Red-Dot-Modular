# Sequential switch — VERIFIED, it IS one (deterministic AND stochastic)

## CORRECTION: the claim HOLDS — I checked the wrong module first
Earlier I checked INTERTROPICAL's inputs (none) and wrongly concluded "no CV input". The CV input is on
CHANGE ALLEY, upstream, chained to Intertropical. Verified: MonsoonChangeAlleyV2.hpp:233 — Change Alley
has **8 poly-CV IN/OUT pairs** (CA_EXPRESSION_CV_CORRELATION.md). So the signal path is real:
**patchable CV IN (Change Alley) -> CA REMAPPING -> Intertropical mapping (normalled from CA) -> outputs.**

## It's a DETERMINISTIC AND STOCHASTIC sequential switch
- **CV into Change Alley** -> remapped by the CA permutation, which is **deterministic** (fixed mapping)
  OR **stochastic** (dice/random verbs reshuffle the mapping). So the switching is a FIXED pattern or a
  generatively RANDOM one.
- **Through Intertropical** (normalled from CA) -> the arrangement mapping applied.
So: patch CV in, it's routed to outputs by a mapping that spans DETERMINISTIC (fixed sequence =
ordinary sequential switch) to STOCHASTIC (reshuffling = random switch), on the same order<->chaos
continuum as everything else — correlation-aware, reversible.

## Claim (holds today)
A **deterministic AND stochastic sequential switch** — distinctive, NOT a commodity round-robin: its
routing pattern is navigable from fixed to generative, correlated, reversible. This is a FIFTH verified
functional identity: patch CV through Change Alley -> switched between outputs by a det-or-random
arrangement.

(Verify-before-claim still honoured — the claim is now grounded in the 8 poly-CV pairs on Change Alley,
not asserted. My first pass checked Intertropical alone and missed the CA->Intertropical chain.)


## The random switching is REVERSIBLE TWO WAYS (verified — code names both)
The stochastic routing isn't lose-it-once-gone randomness; it's reversible by TWO DISTINCT mechanisms,
which the code explicitly distinguishes (MonsoonChangeAlleyV2.hpp:115 "Distinct from Philox dice-reverse"):
1. **Dice / Philox reverse (temporal, re-draw backward):** the scatter draw counters are signed-int64
   addressable POSITIONS in domain-separated Philox streams (lines 125-127): forward jack = counter++,
   back jack = counter--. Step the counter back and the random permutation RECONSTRUCTS bit-exact — the
   random sequence of mappings is scrubbable.
2. **True-reverse (state-history, walk the trajectory backward):** CATrajectoryBuffer (line 185) holds
   the per-stream committed pin-state trajectory; stepBack pops the newest committed state and restores
   the PREVIOUS src[] (lines 324, 115-116). So the committed MAPPING STATE is restorable backward
   through its actual history.
The remap itself is a permutation (out[row] = in[src[row]], line 488/500) via the src[] voice table.

So the stochastic sequential switch is reversible both as a RE-DRAW (dice backward) and as a
STATE-HISTORY WALK (true-reverse) — two independent reverse paths over the random mapping. No ordinary
sequential switch (deterministic or random) is reversible at all, let alone two ways. This is the part
that makes it categorically unlike anything else.


## Deterministic mode: SOME transforms are invertible (precise — CA_DICE_COUNTER_MODEL.md)
"Some of the transforms are also reversible" — the "SOME" is load-bearing and the doc already nailed why:
- **Non-fan-in transforms (rotate, reflect/invert, pure permutations)** — genuinely INVERTIBLE as
  transforms: apply the inverse, recover the prior config exactly.
- **Fan-in transforms (scatter, collapse — many-to-one)** — NOT invertible as transforms (information
  lost; CA_DICE_COUNTER_MODEL.md:31 "scatter has fan-in => NO inverse transform"). Their "reverse" is
  NOT an inverse: either REVERSE-DICE (scatterCounter--, re-derives the previous DRAW and applies it to
  CURRENT pins — a generative scrub gesture, NOT an edit-undo; lines 42-48,57) or TRUE-REVERSE
  (state-replay from the stored trajectory — an undo, but via history, not inversion).

So the honest scoped claim: **SOME deterministic transforms (the permutation/non-fan-in class) are
truly invertible; all are navigable via dice-reverse (generative) and restorable via true-reverse
(state-replay).** Do NOT claim scatter/collapse are "reversible transforms" — they're lossy; their
reversibility is state-replay, not inversion. The word "some" is the accurate one.
