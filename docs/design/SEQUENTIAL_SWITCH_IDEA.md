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
