# CC PROMPT 03 — Phase 3 step 2: the warp mirror, w-depth, and warp composition

PREREQUISITE: CC_PROMPT_02 (LaneDist bank + lower() + drawn pool + tests) merged and green.
This is where warp first affects AUDIO. It is the keystone, so it is the most careful prompt.

## READ FIRST
1. `CLAUDE.md`, `docs/design/CHAT_HANDOFF.md` — context + working model.
2. `docs/design/SESSION_LOG_2026-10-18b.md` §1, §2 — ownership + the TIMING model (mirror holds
   the EFFECTIVE w-mixture; w latched at draw time / step; Sklar-safe per step; display
   immediate; sight-leads-sound-by-one-step). READ §2 carefully — it is the contract.
3. `docs/design/SESSION_LOG_2026-10-18.md` (session a) — the Distribution.hpp/mirror storage
   decision and the cacheability rule (GaussianCopula.hpp:37-46).

Branch: off `lane-expander-refactor`, `feat/warp-mirror`. PR in. Same rules.

## THE VERIFIED SEAMS (grounded this session — do NOT re-guess these)
- UNIFORM STORE (where warp reads FROM): `polyRandom(v, PL_MELODY)[step]` → `random_[v][lane][step]`
  in PatternEngine; `finalRandomByStrand(strand,step)` → `random_[0][strand][step]` is the
  display/CV-out accessor (PatternEngine.hpp:419). The mirror sits at the SAME post-everything
  stage (comment at :421 "pin remap lives upstream in slewed").
- MELODY warp COMPOSES at `genPitchLive` (PatternEngine.cpp:145): the uniform r_semi feeds
  `pickSemitone(in.semiWeights, N, r_semi)` — a discrete fader inverse-CDF (widths = mass).
  Warp = pickSemitone(weights, N, invCDF(r_semi)) = faderMap ∘ F^-1. VERIFIED.
- OCTAVE warp COMPOSES in the same fn: `roll = clamp(r_oct)`, `oct = oL + floor(roll·range)`.
  A second independent warp site (warp r_oct before the range map). VERIFIED.
- melodySource[16][16] is a DISPLAY/PERSISTENCE MIRROR, NOT a consumer. Warp does NOT go there.
  VERIFIED (it is read nowhere outside persistence; the degree map uses melodyRandom/polyRandom).

## STEPS

### STEP 1 — w (warp depth), the universal param
w ∈ [0,1], PER-VOICE, PER-LANE, latched at DRAW TIME (step). Lives as a lane param beside the
LaneDist bank (NOT in LaneDist — it's a scalar, modulatable; the curve is the authored object).
    float warpW[112] = {};   // 16×7, index v*7+lane, like spread[112]. getWarpW/setWarpW.
The effective warp of a uniform u for (v,lane):
    double warped(u) = (1-w)*u + w * laneInvCDF(laneDist[v*7+lane], u, pool);
Monotone for all w ∈ [0,1] (convex combo of monotone fns) → Sklar-safe → rank-preserving.
DK_UNIFORM makes laneInvCDF identity → warped==u for any w (free no-op). Default w=0.

### STEP 2 — the mirror (effective w-mixture)
    float warped_[16][7][16] = {};   // mirrors random_'s [voice][lane][step] shape, FLOAT store
It holds the EFFECTIVE w-mixture (NOT full-warp; there is no separate full-warp array — §2).
Compute: DOUBLE math (laneInvCDF is double), store FLOAT. Exact invCDF, NO new LUT, NO
PhiInvFast (see §-storage in session a: the warp is a CACHED caller, the mirror IS that cache).

### STEP 3 — the SCOPED DIRTY MASK (not a bool)
Rebuild is per-scope, never the whole array on a UI touch (§1 arithmetic):
  - Macro curve/w edit on a lane → rebuild that lane across DELEGATING voices (≤256 cells; SKIP
    voices unlocked to their own curve — nothing about them changed).
  - Sands (East) edit on lane+voice → rebuild 16 cells (one lane, one voice).
  - Pattern regeneration / new draw / scrub → the whole 1792 (that's the only time 1792 is right).
Represent the dirty set so these three cases are distinct. Rebuild reads random_ (the w=0
basis) + laneDist + warpW, writes warped_.

### STEP 4 — route consumers to the mirror
The engine and display must read the EFFECTIVE value, not raw random_, or they silently
disagree (the single-source rule — §2). Two consumers:
  (a) genPitchLive melody: pickSemitone(..., warpedSemiUniform) where warpedSemiUniform comes
      from the mirror for (voice, MELODY, step). Octave: warp r_oct via the mirror for
      (voice, OCTAVE, step) before the roll/range map.
  (b) finalRandomByStrand (bars + prob CV outs): return the mirror's effective value so bars
      and CV outs carry the w-mix automatically (§7 of -18b). This is the display keystone:
      drag curve → dirty → rebuild → bars redistribute.
  *** CONTRACT: no consumer reads raw random_ for melody/octave output once this lands. A
      contract test must assert every output consumer routes through the mirror accessor —
      this is the at-read-vs-mirror safety the session flagged. Model it on the existing
      test_final_random_by_strand.cpp. ***

### STEP 5 — tests (what CAN be tested without Rack)
`test/test_warp_mirror.cpp` (header-only where possible; may need PatternEngine.cpp companion
like test_slew_copula — check run_all.sh:59 pattern):
  - w=0 ⟹ warped_[v][l][s] == random_[v][l][s] EXACTLY, for every kind (the no-op guarantee).
  - w=1, DK_UNIFORM ⟹ still identity.
  - w=1, a parametric ⟹ warped == laneInvCDF(d, u) exactly.
  - MONOTONE: for fixed (v,l), u1<=u2 ⟹ warped(u1)<=warped(u2) across a dense sweep, in FLOAT
    (the Sklar-safety property; float rounding preserves non-decreasing — but with the STRICT-
    INCREASE editor constraint from -18b §8, assert STRICTLY increasing minus ties-at-eps).
  - DIRTY SCOPE: a Macro-lane edit rebuilds only that lane's delegating voices (assert untouched
    cells unchanged); a Sands edit rebuilds 16 cells.
  - CONSUMER ROUTING: finalRandomByStrand returns the mirror value (not raw) when w>0.
Register in run_all.sh. GREEN.

### STEP 6 — latch timing (the ONE thing you can't fully test; state it)
w is sampled/latched at DRAW TIME (step boundary), NOT per sample and NOT at phrase. The mirror
rebuilds on the dirty mask. Display rebuilds IMMEDIATELY on edit (so sight leads sound by one
step — §2, a PROTECTED property: do NOT quantise the display rebuild to the step). You can unit-
test that a w change produces the new mirror value; you CANNOT test the Rack feel — say so.

## DONE =
- warpW[112] bank; warped_ mirror (double compute, float store, no new LUT).
- Scoped dirty mask (3 cases distinct); rebuild from random_+laneDist+warpW.
- genPitchLive melody (pickSemitone) + octave (roll) read the mirror; finalRandomByStrand
  returns the effective value → bars + CV outs carry the w-mix.
- test_warp_mirror.cpp green: no-op@w0, identity, parametric match, monotone, dirty-scope,
  consumer-routing. `bash test/run_all.sh` ALL GREEN.
- PR body: paste test output; confirm w=0 is byte-identical to pre-warp output (the no-op proof).

## NOT THIS PROMPT
- The UI columns / popup editor / visualiser module (prompt 04).
- Parameter modulation of distribution params (theta) + the send/tap routing of w (later;
  depends on the per-voice send-tap fix from prompt 01 being merged).
- Boolean-lane warp (deferred by proof — -18b §3; booleans get a response curve later, not this).
