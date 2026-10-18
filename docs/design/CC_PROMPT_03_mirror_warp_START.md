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

## THE WARP-F BANK IS MERGED (CC_PROMPT_02 / PR #288) — use these names, they EXIST
Do NOT use the old "editor.warp"/"invCDF(warpF,...)" names from any older draft — 02 shipped as:
- `src/dsp/WarpF.hpp`: `using WarpF = std::variant<IdentityWarp, 8 palette types, DrawnWarp>`;
  `IdentityWarp` is the default alternative (index 0). Forwarders: `invCDF(const WarpF&, double u,
  const MonotoneCurve* pool)`, and cdf/pdf likewise. (DrawnWarp needs the pool ptr; others ignore it.)
- `WarpBank` (in Monsoon::editor.warp): `float warpW[112]`, `WarpF warpF[112]` (index v*7+lane),
  and `double warpedValue(int v,int lane,double u) const` = `(1-w)*u + w*invCDF(warpF[i],u,pool)`
  — the BRANCHLESS mix, already built + tested (259 asserts, strictly-increasing in float32).
- Monsoon accessors EXIST: getWarpF/setWarpF(slot,lane,...), getWarpW/setWarpW(slot,lane,...)
  (Monsoon.hpp ~930-933).
So STEP 1 (the w param) and the mix are DONE. This prompt adds the MIRROR + the ENGINE WIRING.

## THE VERIFIED SEAMS (grounded against the merged code — do NOT re-guess these)
- UNIFORM STORE / display accessor: `finalRandomByStrand(strand,step)` → `random_[0][strand][step]`
  (PatternEngine.hpp:419). The mirror sits at the SAME post-everything stage (comment at :421 "pin
  remap lives upstream in slewed").
- MELODY/OCTAVE warp COMPOSES at `genPitchLive(outSem, in, r_semi, r_oct)` (PatternEngine.cpp:145):
  r_semi feeds `pickSemitone(in.semiWeights, N, r_semi)` (PatternEngine.cpp:124) — a discrete fader
  inverse-CDF; r_oct feeds `roll=clamp(r_oct); oct=oL+floor(roll·range)`. Warp = feed the WARPED
  uniform in place of the raw r_semi/r_oct. VERIFIED.
- *** genPitchLive has THREE call sites with DIFFERENT uniform sources + TWO consumers — this is
  the load-bearing detail: ***
    · SOUNDING (audio path), PatternEngine.cpp:693: genPitchLive(sem, in, slewedMelody[0][i],
      slewedOctave[0][i]) → melodyPitchV[i]. PER-VOICE, the real pitch output.
    · DISPLAY, PatternEngine.cpp:790-793: mVal = (displayedVoice_==0 ? melodyRandom[i] :
      polyRandom(displayedVoice_,PL_MELODY)[i]); genPitchLive(sem,in,mVal,oVal) → melodyPitchV
      for the Sands bars, keyed to displayedVoice_.
    · base, PatternEngine.cpp:141 (unitMelody() — a fixed probe, not a draw; leave as-is).
  BOTH the sounding site and the display site must warp using the SAME voice's warpF/warpW
  (the sounding site: voice v; the display site: displayedVoice_). If they use different warp,
  the bars won't match what's heard — the single-source failure the whole mirror exists to prevent.
- melodySource[16][16] is a DISPLAY/PERSISTENCE MIRROR, NOT a consumer. Warp does NOT go there.
  VERIFIED (read nowhere outside persistence; the degree map uses melodyRandom/polyRandom directly).

## STEPS

### STEP 1 — DONE IN 02. w/warpW/warpF banks + warpedValue mix already exist (see top).
Nothing to build here; this prompt uses them. (If you need the mix: editor.warp.warpedValue.)

### STEP 2 — the mirror (effective w-mixture)
    float warped_[16][7][16] = {};   // mirrors random_'s [voice][lane][step], FLOAT store
Holds the EFFECTIVE w-mixture (there is NO separate full-warp array). Each cell =
editor.warp.warpedValue(v, lane, random_[v][lane][step]) — DOUBLE compute (warpedValue returns
double), stored FLOAT. Exact invCDF, NO new LUT (the warp is a CACHED caller; the mirror IS the
cache). The float32 monotonicity of this store is ALREADY proven by 02's test_warp_f_bank S7.

### STEP 3 — the dirty mask (scoped, but COST IS NOT THE REASON — keep it simple)
*** VERIFIED COST (measured, worst case all-TruncNormal invCDF): a FULL 1792-cell rebuild is
0.064 ms; a Macro-lane rebuild (256 cells) 0.009 ms; a Sands cell-edit (16 cells) 0.0006 ms.
So even a full rebuild every frame is <4% of one core at 60fps. The scoped mask is for
CLEANLINESS (don't recompute untouched voices), NOT performance — do NOT over-engineer it.
A simple per-(lane) or per-(lane,voice) dirty flag is plenty; a full rebuild on any draw is
fine. ***
Scopes:
  - Macro curve/w edit on a lane → rebuild that lane across DELEGATING voices (skip voices
    unlocked to their own curve).
  - Sands (East) edit on lane+voice → rebuild 16 cells.
  - Pattern regen / new draw / scrub → rebuild all 1792 (cheap, per the cost above).
Rebuild reads random_ (the w=0 basis) + editor.warp (warpF+warpW), writes warped_.

### STEP 4 — route consumers to the mirror (the single-source keystone)
The engine AND display must read the EFFECTIVE value, not raw random_ — else they silently
disagree (what the mirror exists to prevent). The two consumers, at the VERIFIED seams:
  (a) SOUNDING (PatternEngine.cpp:693): feed genPitchLive the MIRROR value for (v, MELODY/OCTAVE,
      step) instead of raw slewedMelody/slewedOctave.
      *** VERIFIED ORDERING (don't re-derive): the slew pass writes its result BACK into random_ —
      PatternEngine.cpp:462 `melodyRandom[i]=slewedMelody[0][i]` (melodyRandom is a view onto
      random_[0][MELODY]). So AFTER the slew pass, random_ HOLDS the post-slew uniform, and
      slewedMelody == random_ at that point. => a SINGLE mirror over random_ is the correct w=0
      basis for BOTH consumers (warp composes AFTER slew, on the settled uniform). Build the mirror
      from random_ AFTER the slew write-back; the sounding path then reads the mirror for voice v,
      the display reads it for displayedVoice_, and they agree by construction. Do NOT mirror
      slewedMelody separately — that would double the source of truth. ***
  (b) DISPLAY (PatternEngine.cpp:790-793) + finalRandomByStrand (PatternEngine.hpp:419): return
      the mirror's effective value → bars + prob CV outs carry the w-mix automatically. Keystone:
      drag curve → dirty → rebuild → bars redistribute.
  BOTH must key the warp to the correct voice (sounding: v; display: displayedVoice_) using the
  SAME warpF/warpW the other reads — mismatch = bars don't match what's heard.
  *** CONTRACT TEST: no consumer reads raw random_ for melody/octave OUTPUT once this lands.
      Assert finalRandomByStrand returns the mirror value (not raw) when w>0, and the same for
      the sounding path's effective uniform. Model on test_final_random_by_strand.cpp. ***

### STEP 5 — tests (what CAN be tested without Rack)
`test/test_warp_mirror.cpp` (header-only where possible; may need PatternEngine.cpp companion
like test_slew_copula — check run_all.sh:59 pattern):
  - w=0 ⟹ warped_[v][l][s] == random_[v][l][s] EXACTLY, for every kind (the no-op guarantee).
  - w=1, IdentityWarp ⟹ still identity (warpedValue==u even at w=1).
  - w=1, a parametric ⟹ warped == invCDF(warpF,...)(d, u) exactly.
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
- warped_ mirror (double compute, float store, no new LUT), built from random_ AFTER the slew
  write-back + editor.warp (warpF+warpW, both from merged 02).
- Dirty mask (scopes distinct; cost-trivial — simple flag is fine); rebuild reads random_+editor.warp.
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
