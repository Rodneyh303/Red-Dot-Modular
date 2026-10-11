# Voice-count optimization — NEXT PERF PRIORITY (mechanical, low-risk)

## Why this is next (biggest + simplest + safest)
- END-TO-END: skips inactive voices through the whole PER-VOICE DOWNSTREAM pipeline (spread apply,
  readout, slew, quantise, output, display) — a MULTIPLIER (~16/N x on per-voice cost), not a one-stage
  speedup.
- The MULTI-INSTANCE lever: few voices per instance -> multiple Monsoons realistic (polyrhythm + CA
  sharing). See MULTI_INSTANCE_EFFICIENCY.md.
- Mechanical + low-risk: NO SIMD, NO float-determinism tension, NO draw conditioning. Just cap
  DOWNSTREAM per-voice loops at the active count.
- EASIER post V1->voice-0: voices are uniform (V1 = voices[0], no separate mono path), so "active count"
  means the same everywhere — no V1-exception in any bound.

## The rule
- **DRAWS: always all voices.** The draw is MULTIVARIATE (all voices at once) and cheap post-16x. Do NOT
  condition/skip the draw. Draw-generation loops STAY `< 15` (or `< 16`). Keeping them all-voices means
  adding a voice = INSTANT (its draw is already there; just start processing it — no regen, no history).
- **DOWNSTREAM PROCESSING: cap at the active count.** Spread apply, readout, output, display, per-voice
  state that COSTS per step -> gate to numPolyVoices.
- **PER-LANE loops (`< 7` / NUM_STRANDS): leave alone** — lane axis, not voice.

## Semantics (SequencerEngine.hpp:131+)
- `PolyVoice voices[16]` (Phase A: voices[0] = V1; V1..V16). `numPolyVoices` from the context menu
  (0 = mono only). The legacy `< 15` loops are the OLD V2..V16 poly range (now shifted; "poly voices" =
  indices used by these loops). The active count is `numPolyVoices`. CONFIRM the exact mapping
  (numPolyVoices vs the 16-wide voices[] indexing post-Phase-A) before gating — Phase A shifted indices.

## CANDIDATE LOOPS (audited) — classify each, then gate the DOWNSTREAM ones
### KEEP all-voices (DRAW generation / source copy — multivariate, cheap) — do NOT gate
PatternEngine.cpp: 28, 73, 236, 237, 279, 297, 311, 318, 320, 321, 356, 369, 378, 413, 424, 432, 456, 481
  — these fill/copy slewedPoly* / polyRandom / polyXSource (the per-voice DRAW field). All-voices by design.
PatternEngine.hpp: 246, 276 — draw-field init/copy. Keep.

### REVIEW — cheap per-step bookkeeping vs real downstream cost (gate only if it COSTS per step)
SequencerEngine.cpp:106  — laneTickV_[v][s]=0 reset. Cheap bookkeeping; gating optional (low saving).
SequencerEngine.cpp:189, 306 — laneDirV_[v][l] = pending (direction latch). Cheap; optional.
SequencerEngine.cpp:248, 342 — per-voice LOR length recompute (polyStrandLen per voice) -> this DOES work
  per voice (recomp); GATE to active count (real saving in the per-voice direction/length compute).
MonsoonExpanderManager.cpp:84 — eastPolyOwner[pv] ownership resolve per voice -> gate (ownership calc).
MonsoonExpanderManager.cpp:99, 143 — setVarlegLocalEast / delegation clear per voice -> gate (per-voice
  state writes).

### ALREADY active-count-bound (reference — no change)
Monsoon.cpp:1278 (numPolyVoices); MonsoonExpanderManager.cpp:239, 700 (effPolyVoices); Phase-B regen
SequencerEngine.hpp:745 (prevNumPolyVoices_ tracking -> the voice-change infra already exists).

### LEAVE (per-lane, not voice)
MonsoonExpanderManager.cpp:567, 579, 598, 607 (`< 7` = lanes); any `< NUM_STRANDS`.

## Method
1. For each candidate: read it — is it DRAW (keep), real per-step DOWNSTREAM cost (gate), or cheap
   bookkeeping (gate only if trivial/consistent)? Priority = the ones doing real per-voice COMPUTE
   (spread apply, LOR length recompute, ownership, readout, output).
2. Gate the downstream ones to the active count CONSISTENTLY (one un-gated downstream stage = inactive
   voices still cost). Use the same active-count variable throughout.
3. The voice-CHANGE handling exists (Phase-B regen + prevNumPolyVoices_); voice-add is instant
   (loop widens, draws already present). Confirm the mutual lock (poly-change at step boundary).
4. BIT-COMPARE: with all voices active, output IDENTICAL before/after (gating doesn't change the active
   voices' result, only skips inactive ones) -> the bit-compare + test gate verify no regression.
5. MEASURE: toggle voice count (1 vs 16), watch the VCV CPU meter -> confirm the saving scales ~16/N.

## Risk
Very low: no SIMD, no determinism change, no draw conditioning; gating = loop bounds. The only care is
(a) consistency (gate ALL downstream stages, not some), and (b) confirm the numPolyVoices<->voices[]
index mapping post-Phase-A so the bound is correct. Bit-compare catches mistakes.


## CORRECTION — the saving is BIGGER than first thought: PhiInv is 91% and PER-VOICE (Rodney)
Earlier "draws are cheap post-16x, don't skip them" was WRONG IN EMPHASIS. PatternEngine.hpp:774 states:
"~91% of patternXAt's cost is PhiInv (Acklam+Halley), NOT Philox." The 16x was on PHILOX (the cheap 9%).
PhiInv is CACHED per (key,pos) — computed ONCE on cache-MISS, then reused. The multivariate cache entry
holds PhiInv for ALL voices (zPolyRhythm[15][16] etc.), so a cache-miss computes PhiInv for ALL 15 poly
voices at that position — INCLUDING INACTIVE ONES.

So the real per-voice cost split:
- **PhiInv (91%): paid once per position on cache-MISS, for ALL voices.** Gating it to ACTIVE voices
  saves the dominant cost (12 inactive voices x PhiInv) on every new position drawn. THIS is the big win
  the "don't skip draws" conclusion missed.
- **Steady-state blend/applyZ (the slewedPoly* `<15` loops, per step): reads cached z + blends.** Gating
  these saves the per-step per-voice blend (smaller, but real).

So GATE ALL per-voice work to active voices — both the PhiInv-at-cache-miss AND the blend loops — not
just the "downstream". The PhiInv gating is the LARGE saving (~16/N x on the 91% cost).

### Implementation (Rodney) — "guard everything, unguard on poly-count INCREASE" (easy)
- Normally compute per-voice work (PhiInv-at-miss + blend) only for ACTIVE voices.
- On poly count INCREASE, the newly-active voices were skipped -> compute the DELTA voices' PhiInv/z at
  the current cached positions. EASY because PhiInv is a PURE function of (key,pos) -> just evaluate it
  for the new voices' slots. The Phase-B infra ALREADY exists (prevNumPolyVoices_ tracking +
  resetActivatedPolyVoices_, SequencerEngine.hpp:745) — extend it to also fill the delta voices' z.
- On DECREASE: just stop computing the dropped voices (nothing to regen).
This supersedes "keep draws all-voices": keep raw PHILOX all-voices (cheap, multivariate) but GATE the
PhiInv/z (the 91%) to active voices, filling the delta on increase. The raw Philox stays all-16 (so the
multivariate structure + addressability hold); only the expensive PhiInv transform is active-gated.

### Net
~16/N x on the DOMINANT (91% PhiInv) per-voice cost + the per-step blend. Much bigger than gating only
the "downstream". Still low-risk: PhiInv is addressable (compute delta on increase), bit-compare verifies
(all-voices-active = identical output). Measure: toggle 1 vs 16 voices, watch CPU meter.
