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

### WHERE TO HOOK THE REBUILDS (resolved — correct-by-construction, do NOT re-derive block order)
The mirror must reflect random_ AS OF NOW whenever random_ changes, so freshness never depends on
per-block call order. Hook the rebuild at the random_ WRITERS, not at a global per-block pass:
  - recomputeEffectiveMelody (PatternEngine.cpp:393, writes random_[0] mel/oct at :462) and
    recomputeEffectiveMelodyForVoice (:601, :621) -> at the END, rebuild that voice's MELODY+OCTAVE
    mirror rows (<=32 cells). Then :693's genPitchLive read of the mirror is fresh regardless of
    whether the manager's spread pass has run yet this block -- the mirror always holds
    warp(current random_), exactly what the audio path reads at that instant (self-consistent).
    Cheap (32 cells); this is the belt-and-braces freshness guarantee.
  - refreshPolyRandomCell (SequencerEngine.cpp:1043, step-rate sole writer of a final cell) ->
    rebuild that one mirror cell right after it writes random_ (all lanes it wrote; harmless +
    future-proof).
  - the manager spread passes (MonsoonSandsManager.cpp mono/East/Macro writes to random_) ->
    rebuild the touched cells at the end of the pass, or just rebuild-all at end of processDNA
    (0.064ms, cheap per the cost note).
RULE: every random_ writer either rebuilds the cells it wrote or marks them dirty for an
end-of-block rebuild. "rebuild-all-if-any-dirty at end of processDNA" is acceptable given the
cost -- but the 32-cell recompute-hook rebuild is what makes :693 fresh WITHIN the block.

### WIRING THE BANK POINTER + DELEGATION MIRROR (the precedent exists)
- Bank pointer: engine.pe gets a WarpBank* (set to &editor.warp), wired the same place+way as the
  spreadTargetMode mirroring -- Monsoon.cpp:1356 `engine.pe.spreadTargetMode[l]=getSpreadTargetMode(l)`
  (per-block, control path). NULL bank => no warp => rebuilds no-op => consumers fall back to raw
  random_ => byte-identical legacy. THIS is how MeloDicer (no bank) stays unchanged; it is also the
  w=0 path's guarantee.
- Delegation mirror (which voices are Macro-delegated vs own-curve): build it where topology is
  resolved -- MonsoonSandsManager::processDNA right after SandsTopology is built (~:137): loop
  v x lane, delegated[v][l] = (topo.owner(v,l)==Role::MACRO). No Sands/Macro => owner()==NONE =>
  delegated=false everywhere => all per-voice slots (correct: no Macro, no delegation).
  (b) DISPLAY (PatternEngine.cpp:790-793) + finalRandomByStrand (PatternEngine.hpp:419).
      BOTH must key the warp to the correct voice (sounding: v; display: displayedVoice_) using the
      SAME warpF/warpW the other reads — mismatch = bars don't match what's heard.

  *** THE PER-LANE GATE (resolved — this is the rule, do NOT return the mirror wholesale):
      A lane's DISPLAY reads the mirror IFF its ENGINE CONSUMER does. VERIFIED: the engine warps
      ONLY melody/octave (via genPitchLive); rest/legato/accent/qmix/variation decisions read RAW
      uniforms (SequencerEngine.cpp:750-754: polyRandom(0, PL_REST/LEGATO/ACCENT/QMIX/VARIATION),
      no warp). Boolean/qmix warp is DEFERRED (-18b §3). So:
        finalRandomByStrand(strand, step) returns the MIRROR for strand ∈ {MELODY, OCTAVE},
        and RAW random_ for every other strand (byte-identical to today).
      If it returned the mirror for ALL strands and a future UI set warp on REST, the REST BARS
      would warp while the REST AUDIO stayed raw → bars LIE vs audio — the exact single-source
      failure the mirror prevents. The gate keeps bars == audio by construction for EVERY lane.
      When qmix/boolean warp lands later, flip that lane's CONSUMER and its finalRandomByStrand
      case TOGETHER — never one without the other. (My earlier "returns the effective value"
      unqualified was ambiguous; THIS per-lane rule is the resolution.)
      Note the bank still HOLDS all 7 lanes' warped values in the mirror (inert for non-mel/oct
      until their consumer is wired) — that's fine, it's just unused storage, not a lie. ***

  *** CONTRACT TEST: no consumer reads raw random_ for MELODY/OCTAVE output once this lands.
      Assert: finalRandomByStrand(STRAND_MELODY, step) == mirror (≠ raw) when w>0 on that
      (voice,MELODY) slot; finalRandomByStrand(STRAND_REST, step) == raw (UNCHANGED) even with
      w>0 set on the REST slot (proves the gate); and the sounding path's effective uniform ==
      mirror for melody/octave. Model on test_final_random_by_strand.cpp. ***

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
