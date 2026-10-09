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

## Revisit — a STEP-16-LIMIT AUDIT, scheduled AFTER mono fix + addressing system (Rodney)
The "stay at 16" above holds for the LENGTH argument (Intertropical horizontal polyphony + nested
beating already give long evolving patterns). But there is a SEPARATE, open question: **LEGIBILITY** —
whether the sophisticated per-voice features (voice LOR ranges/offsets, per-voice reset points,
absolute-mode regions, correlated reset-phase = canon/hocket) need MORE BASE ROOM to be heard as
DISTINCT rather than compressed into mush. Those relationships live WITHIN the 16-step base frame, so 16
may be too cramped to resolve them clearly. This is a different (and stronger) argument than "need longer
patterns".

**Plan: a step-16-limit AUDIT + 32-step A/B experiment, scheduled AFTER (a) the mono fix and (b) the
addressing system are in.** Order matters: the experiment tests whether the FEATURES need room, so the
features must EXIST first (testing 32 before the addressing system just tests a longer flat loop — the
wrong thing).
- Make MAX_STEPS a constant (not hard-coded 16); branch.
- A/B the SAME sophisticated patches at 16 vs 32 with the features engaged. Question: do the voice
  relationships (canon/hocket, ranges, offsets) read MORE CLEARLY at 32, or is 16 enough?
- If 32 makes them legible where 16 muddied them -> the (invasive) resize is justified (touches
  random_[16][·][16], CA, copula, reversibility, hot path — real surgery, but earned).
- If equally clear at 16 -> 16 confirmed, surgery saved.
- Middle option to consider: a CONFIGURABLE base length (16 or 32) if the architecture can take a
  variable MAX_STEPS without doubling everything unconditionally — room when wanted, no forced cost.

So: 16 confirmed for LENGTH; the LEGIBILITY question is open and deliberately deferred to a post-mono,
post-addressing-system audit/experiment. Decision made by LISTENING (A/B), not in advance.

### LONG NOTES — the concrete, partly-OBJECTIVE case (Rodney)
The clearest instance where 16 is genuinely too small is GENERATING LONGER NOTES. A note's DURATION eats
steps: whole note = 16 (the entire pattern is one note), half note = 8 (half the frame). So the longer
the notes, the FEWER fit — you cannot write a multi-note phrase of long notes in 16 (e.g. four half
notes = 32 steps). This is not aesthetics; it is a HARD EXPRESSIVE CAP: the arithmetic doesn't fit.
It compounds with the features: legato/tie machinery (extends notes -> needs steps to extend into),
triplets/odd durations (consume steps + leave gaps), per-voice ranges with long notes (a small range may
fit only ONE long note -> no phrase).
This sharpens the audit's success criterion: not only "does it sound better at 32" (subjective) but
"is there music — specifically long-note phrases — IMPOSSIBLE to express at 16 but natural at 32?" For
long notes the answer is pretty clearly YES (unarguable arithmetic: N long notes need N x length > 16).
So for long-note music, more steps is ENABLING, not just legibility — which tilts the audit toward
"yes, we need room" for that use case, and makes the CONFIGURABLE base length (16 for short-note
patterns, 32 when long-note phrases are wanted) the most attractive option: room when needed, no forced
cost on every patch.
Only post-release, as a deliberate large project, and only if — after LIVING with the addressing system +
horizontal polyphony — 16 genuinely proves limiting. Bet: it won't, because nested ratios + sequential
arrangement already stretch 16 across many bars of structured, evolving material.



### PRECEDENT + what actually resizes (Rodney)
- **Variable length ALREADY EXISTS:** `PATTERN_LENGTH_PARAM` (Monsoon.hpp:93) — the pattern already
  plays a variable 1..16 steps. So extending to 32 is RAISING A CEILING, not building variable-length.
  The length CONTROL and play-N-steps logic are already there; what's hardcoded 16 is the STORAGE
  dimension `random_[16][NUM_STRANDS][16]` (PatternEngine.hpp:103, the 2nd [16] = MAX_STEPS) and
  everything indexed by it (CA, copula, reversibility, hot path). The resize is concentrated in the
  array dimension + indexing, NOT new control logic.
- **16 is a RECOGNISED FLAW of the closest ancestor (Rodney):** the 16-step cap is seen as a limitation
  of the original Melodicer (the nearest probabilistic step-sequencer in spirit). So this is EXTERNAL
  evidence, not a hypothetical — the category has already found 16 limiting. Shipping with it = shipping
  a known category weakness.

**Net:** three converging arguments now tilt toward RAISING the ceiling — (1) long notes = hard
arithmetic cap, (2) Melodicer precedent = recognised category flaw, (3) the length control already
exists so it's a ceiling-raise not a new feature. The audit is still the gate (build it, A/B, confirm),
but the expected conclusion has shifted toward "raise it" (to 32, or configurable 16/32). Cost stays
the array-dimension resize (invasive but bounded: MAX_STEPS + indexing across CA/copula/reversibility/
hot path), to be done carefully with the test suite (bit-compare, reversibility) as the safety net.

## CEILING = 32, bounded by UI practicality (Rodney)
**32 is the MAX setting — 64 would make Sands too fiddly at current HP.** 64 cells across the lane grid
(and 64 per-step clickable cells for the absolute-2/3 play/mute MASK) would be too small to read/edit/
paint precisely at the panel's current width → bad UX. 32 keeps the per-step cells big enough to click
and read without widening the panel. So the ceiling is min(musically wanted, editable at current HP) =
32: enough to fix the long-note/phrase flaw + the Melodicer limitation, while staying usable.
- Pattern length stays VARIABLE via the existing PATTERN_LENGTH_PARAM — now 1..32 instead of 1..16.
- Cell editability matters DOUBLY because the absolute-2/3 mask is painted per-cell; 32 keeps both
  probability-bar editing AND mask-painting usable; 64 would make both fiddly.
- No HP expansion — 32 keeps the current panel width.
So the target is definitively 32 (not "32 or more") — UI practicality caps it there, and 32 solves the
flaw without the fiddliness or a wider panel.