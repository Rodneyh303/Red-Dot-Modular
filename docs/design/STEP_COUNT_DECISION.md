# Step count — stay at 16 (decision + reasoning)

## Decision: STAY AT 16 steps. Do NOT expand to 32.
Considered expanding the base pattern to ~32 steps (tempting given the rich addressing machinery). Decided
against.

## Why 32 is expensive
16 is deeply baked in — NOT just the light ring (which is cosmetic). It's the 2nd dimension of the core
data structure `random_[16][NUM_STRANDS][16]` (banks x strands x STEPS), plus dozens of `[16]` arrays
(slewed buffers, preRemap buffers, poly `[15][16]`, CA source maps `caRhythmSrc[16]={0..15}`), the
`&row[16]` accessors, and the copula/CA/reversibility machinery that indexes `[step]` assuming 16.
Resizing to 32 ripples through the copula, CA, slew, poly, reversibility, and the output HOT PATH (just
optimised) — invasive, risky (reintroduces bugs in just-stabilised systems), roughly doubles
per-recompute work (re-opens performance), and is poorly timed pre-release.

## Why 32 is UNNECESSARY — the architecture already delivers length + evolution from 16
1. **LONGER patterns = HORIZONTAL POLYPHONY via Intertropical (Rodney).** Reading the voices SEQUENTIALLY
   (theme-and-variations / horizontal) rather than simultaneously CONCATENATES them: 16 voices x 16 steps
   read in sequence = up to a 256-step pattern, with no change to the 16-step base. Length is an
   ARRANGEMENT concern (compose 16-step units in sequence), not a base-structure one — and it's BETTER
   length: long patterns of CORRELATED, varied units (theme + variations over the long form), arranged,
   vs a bigger flat loop.
2. **EVOLUTION = nested-length beating** (the unified addressing system): nested voice/lane lengths beat
   at LCMs reaching into the hundreds/thousands of steps, plus metric modulation + absolute-mode regions.
   16 base steps already generate multi-bar, non-repeating-feeling evolution (the felt 6/8 is the proof:
   16 steps, effective cycle 24+, feels alive).

So the machinery that tempted us toward 32 is exactly what makes 32 unnecessary: length is MULTIPLICATIVE
(ratios) and COMPOSED (sequential voices), not additive (more raw steps). A compact 16-step base + the
addressing/arrangement machinery beats a bigger flat base.

## Revisit?
Only post-release, as a deliberate large project, and only if — after LIVING with the addressing system +
horizontal polyphony — 16 genuinely proves limiting. Bet: it won't, because nested ratios + sequential
arrangement already stretch 16 across many bars of structured, evolving material.
