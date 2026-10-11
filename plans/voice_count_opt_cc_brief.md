# CC BRIEF — voice-count optimization (next perf priority)

Full spec + classified candidates: docs/design/VOICE_COUNT_OPTIMIZATION.md (read it — the "PhiInv 91%"
correction + the strategic-gate banner). This is the working brief.

## Goal
Skip per-voice work for INACTIVE voices -> cost scales ~16/N x for N active voices. This is the GATE for
multiple-Monsoon polyrhythm / crab-canon. Mechanical, low-risk (loop bounds + addressable delta-fill).

## Core finding (why it's the big win)
~91% of the draw cost is PhiInv (NOT Philox — PatternEngine.hpp:774). PhiInv is cached per (key,pos),
computed ONCE on cache-MISS for ALL 15 poly voices incl inactive. So the big saving = gate the per-voice
PhiInv-at-miss + the per-voice blend/processing to the active count.

## Rule
- KEEP raw PHILOX all-voices (multivariate, cheap — don't condition the draw structure).
- GATE the per-voice PhiInv (at cache-miss) AND the per-voice blend/processing loops to the active count
  (< numPolyVoices, not < 15).
- Per-lane loops (< 7 / NUM_STRANDS) -> LEAVE ALONE (lane axis, not voice).

## Implementation pattern — "guard everything, UNGUARD on poly-count INCREASE"
- Normally compute per-voice work only for active voices.
- On INCREASE: compute the delta voices' PhiInv/z at the current cached positions — EASY (PhiInv is a
  pure fn of (key,pos)). Phase-B infra EXISTS (prevNumPolyVoices_ + resetActivatedPolyVoices_,
  SequencerEngine.hpp:745) — extend it to fill the newly-active voices' z.
- On DECREASE: stop computing dropped voices (nothing to regen).

## FIRST — verify the index mapping (critical)
Phase A shifted indices (voices[0] = V1; old < 15 loops are the poly range). CONFIRM exactly how
numPolyVoices maps to the loop index / voices[] indexing post-Phase-A BEFORE changing any bound. Wrong
mapping = skip/process the wrong voices.

## Candidate loops (audited — classify each as you touch it)
- GATE (real per-voice downstream compute): SequencerEngine.cpp:248,342 (per-voice LOR length recompute);
  MonsoonExpanderManager.cpp:84 (ownership), 99,143 (per-voice state). The slewedPoly* blend loops in
  PatternEngine.cpp (279,297,356,413,...) -> gate the per-step blend.
- The PhiInv-at-cache-miss computation (the 91% — in CachedRhythmDraw/MelodyDraw/QmixDraw fill) -> gate
  to active. BIG WIN.
- KEEP all-voices (draw/source fills): PatternEngine.cpp 28,73,236,237,311,318,320,321,378,432,456,481;
  hpp 246,276.
- ALREADY gated (reference, no change): Monsoon.cpp:1278; MonsoonExpanderManager.cpp:239,700.

## Verification (both required)
1. BIT-COMPARE: with ALL voices active (numPolyVoices=max), output BIT-IDENTICAL before/after (gating only
   skips inactive). The bit-compare test + CI gate catch regressions. MUST pass on all 3 platforms.
2. MEASURE: toggle voice count 1 vs 16, watch the VCV per-module CPU meter -> confirm ~16/N scaling.
3. Voice-ADD clean: increase count mid-play -> new voices in at next step boundary, existing unchanged
   (no glitch), via delta-PhiInv fill. Confirm the mutual lock (poly-change at step boundary).

## Method
One area at a time; build + BIT-COMPARE + commit + PUSH each. Watch CI (runs tests on 3 platforms now).
No logging. Report the bit-compare result + the measured CPU saving (1 vs 16 voices).
