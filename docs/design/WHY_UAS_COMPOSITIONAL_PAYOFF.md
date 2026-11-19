# Why UAS / Sands LOR modulation — the compositional payoff

The design rationale, in Rodney's own framing (2026-11-20). The "why" of the
instrument — worth keeping when the engine detail blurs.

## The old worry

Pre-UAS: Sands LOR modulation felt like it might be overkill — a solution looking
for a problem.

## What answers it: ORTHOGONALITY

The three LOR axes are genuinely INDEPENDENT, and that independence IS the
compositional space. UAS is what made them cleanly SEPARABLE — pre-UAS they were
entangled, so modulating one perturbed the others and the result was mush.

- **LENGTH** modulates the PERIOD / phase → lanes of different lengths drift in
  and out of phase. Polyrhythm / polymeter, and because content is re-timed,
  variation in rhythm AND melody at once.
- **OFFSET** preserves phase, changes CONTENT → moves the window into the pattern
  without disturbing the rhythmic relationship. The axis that changes WHAT without
  changing WHEN.
- **ROTATION** changes CONTENT PHASE independent of length (with or without length
  modulation) → which content lands where. Canon / phasing.

Sands sets these relationships both AMONG LANES and BETWEEN VOICES — so the same
three axes give within-voice variation and across-voice counterpoint from one
mechanism.

## The worked example (Rodney)

Two length-8 lanes, one stepping rotation +1 each lap → a DETERMINISTIC PHASING
CANON: the pair realigns every 8 laps, so a 64-step macro-form emerges from two
8-step lanes. Reich's phasing, but quantized and exact, set by a single rotation
modulation. (And the macro period emerging from LOR relationships is why the
64-step idea matters — see SIXTYFOUR_STEP_IDEA.md.)

## Why the engineering arc was the precondition, not a detour

The payoff exists ONLY if the phases are trustworthy. A sequencer that is one step
off on when rotation lands, or commits modulation at the wrong edge, lies about
its own phase — and you cannot compose structure on a liar. Every bug in the
engine arc — the rotation/window edge (CC_FIX_CYCLE_COMMIT_WINDOW_EDGE), the
commit boundary, the reverse mirror (CC_PROMPT_REVERSE_PROBLANE_ROTATION_EDGE),
the position-law consolidation — was making these relationships EXACT so they can
be built on. The engineering wasn't separate from the music; it was the condition
for it. And because LOR is now MODULATABLE (CV-driven) with correct commit timing,
the evolution can be automated: structured generative variation, not randomness.
