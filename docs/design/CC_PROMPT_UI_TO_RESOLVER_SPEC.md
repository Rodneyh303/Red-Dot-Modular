# ⟳ CURRENT-STATE UPDATE (verified at HEAD 52e6123 — read before the spec below)
Re-verified anchors + three updates since this spec was written:
1. LINE DRIFT: macroOwnLaneProbability is now SequencerEngine.hpp:759 (was :756);
   macroOwnProbability forwards to it (StraitsSandsMacroVisual.cpp:558-561); it's called in the
   Macro output sweep (StraitsSandsMacroVisual.cpp:681). Resolver helpers present:
   resolveSpread / resolveLorValue(:382) / resolveDirValue(:431), effective(:164).
2. THE /32 WORKAROUND IS WHAT THIS MIGRATION REPLACES: the CPU fix added a /32 control-rate throttle
   + a change-fingerprint gate around macroOwnLaneProbability (StraitsSandsMacroVisual.cpp:900-1117).
   The code's OWN comments say it's a STOPGAP: ":1109 ~7x(1+N)/32 => the /32 throttle is working but the
   resolve still runs every [control block]". The PROPER fix = this migration: resolve ONCE at STEP rate
   (cached), consumers read the cache. So this spec SUPERSEDES the /32 workaround — when the resolver
   produces the step-cached value, the /32 throttle + fingerprint gate around the recompute can be
   REMOVED (the recompute no longer exists; there's a cached value to read). Do that as part of step 3.
3. CARRY THE ATOM/TILT PARAMS: the tilt (p) + atom (ℓ, m) feel-test stores fingerprint "EXACTLY like
   warpW/warpF" (StraitsSandsMacroVisual.cpp:986) and compose inside the warp mix. The ONE resolver
   value must include them (warp mix already composes invCDF with tilt+atom — the resolver's warp term
   inherits it). No separate handling; just ensure the resolver's warp composition is the SAME one the
   feel-test uses (don't fork a second warp path).

────────────────────────────────────────────────────────────────────────────────
# SPEC — fold the UI into SandsModifierResolver (one effective-value authority, all consumers read it)

GOAL: end the bars-vs-output / East↔Macro-leak / lane-direction / cross-module-recompute bug CLASS by
making the resolver the SINGLE producer of each per-(voice,lane) effective value, read by the engine,
the bars, the prob-outs, and all modules — no second computation anywhere. This is the "resolver folds
in UI" structural fix (RATE_MANAGER_AND_RESOLVER_UI_DESIGN.md). Lane-direction fix FALLS OUT of this
(do not patch it separately). Prereq: the CPU section-profile + point-fix should land first so this is
built on a sane baseline; but this migration ALSO removes a CPU source (the per-sample triplicate).

## THE CURRENT MESS (verified in code — THREE computations of the same quantity)
"Effective probability / value for (voice, lane)" is computed in THREE places that can drift:
1. ENGINE DRAW PATH — writes random_ (the authored draw + spread, via SpreadInterp) at step time.
2. SandsModifierResolver<SpreadPolicy/LorPolicy/DirPolicy> (SandsModifierResolver.hpp:373-431) — the
   INTENDED authority; resolveSpread/resolveLorValue/resolveDirValue. Used by the engine for
   spread/LOR/direction delegation. CORRECT shape, but NOT the thing the bars/outputs read.
3. macroOwnLaneProbability (SequencerEngine.hpp:756) — ADDED by CC_PROMPT_macro_output_east_leak. Does
   SpreadInterp::polySlewedPub + applyAnchorV1Only + warpDisplayMix — i.e. RE-IMPLEMENTS spread+warp
   resolution a THIRD time. Called by BOTH the bars (StraitsSandsMacroVisual.cpp:534 forwards to it)
   AND the prob CV out — per channel, PER SAMPLE (the Macro 124% CPU suspect).
So: the resolver (2) exists but the display/output path built its OWN (3) instead of using it. That is
why bars-vs-output and the East-leak persisted AFTER the resolver, and why the fix added CPU.

## THE TARGET: ONE resolver call, one value, every consumer reads it
There must be exactly ONE function that computes the effective (voice,lane,step) value — the resolver —
and the engine draw, the bars, and every prob-out READ IT. No SpreadInterp-in-the-UI, no
macroOwnLaneProbability re-impl, no third path.

### Step 1 — make the resolver the sole producer of "effective (voice,lane) probability"
Extend SandsModifierResolver (or add a thin resolveProbability using the existing policies) so ONE call
produces the effective probability for (voice, lane, step) = base draw + resolved spread (via the
SpreadPolicy delegation algebra, NOT a separate SpreadInterp call) + warp mix. This SUBSUMES what
macroOwnLaneProbability does, but through the resolver's ONE delegation authority (so East/Macro
ownership is resolved in the ONE place, killing the leak + the lane-direction variant by construction).
- The spread term MUST come through SandsModifierResolver<SpreadPolicy>::effective (the delegation
  algebra), not a parallel SpreadInterp call. If SpreadInterp is the underlying math, the resolver's
  SpreadPolicy wraps it — but the DELEGATION decision (ownerEast/macroBase/tap) lives in the resolver,
  once.
- Warp mix: the resolver's output for melody/octave composes warpedValue (the existing warp mirror),
  per the per-lane gate (CC_PROMPT_05). ONE warp composition, not warpDisplayMix-vs-warpMixCell copies.

### Step 2 — route EVERY consumer through it; DELETE the duplicates
- ENGINE draw: reads the resolver (it mostly does for spread/LOR/dir; ensure the probability path does too).
- BARS: StraitsSandsMacroVisual macroOwnProbability → calls the resolver (continuous — see rate note),
  NOT a separate accessor. East bars likewise.
- PROB-OUTS: finalRandomByStrand / the CV-out path → read the resolver's value (latched — see rate note),
  NOT macroOwnLaneProbability, NOT raw random_[0].
- DELETE macroOwnLaneProbability (SequencerEngine.hpp:756) and macroOwnProbability's separate logic —
  they become (or forward to) the one resolver call. DELETE any remaining SpreadInterp-in-UI path.
- LANE DIRECTION: route the direction display + the direction consumer through resolveDirValue (the
  resolver already has DirPolicy). The VAR/LEG + follow-Macro-direction bug falls out — direction is
  resolved in the one place for all 7 lanes.

### Step 3 — the rate split (bars continuous, output latched) WITHOUT a second computation
The bars are CONTINUOUS (sight-leads-sound), the output is LATCHED per-step. These are the SAME resolver
value sampled at DIFFERENT rates — NOT two computations. Decompose the resolver so the HEAVY part
(delegation + spread + warp PARAMETERS) resolves at STEP/edit rate (cached), and only the THIN live
overlay (the continuous knob/CV position) is applied at frame rate for the bars. The output reads the
step-cached value; the bars read step-cached + thin-live-overlay. (This is the slow-heavy/fast-thin
rule — and it is what stops the per-sample CPU: the heavy resolver runs at step, not per sample.)
*** This step is where UI-to-resolver meets the CPU fix: the resolver MUST NOT run its heavy part
per-sample for the bars. Cache at step; overlay the thin live bit at frame. ***

## VOICE CONSISTENCY (kill the voice-0 residue while here)
The prob-out hardcodes warpRow(0) (voice 0) while bars draw the displayed voice — the voice mismatch
from CC_PROMPT_warp_display_scope. Resolving through ONE accessor that TAKES an explicit voice (no
hardcoded 0) makes bars and output read the SAME voice by construction. Decide (Rodney): the Macro
prob-out is the mono (V1) out → bars for the Macro panel show V1's resolver value; OR per-voice
poly-outs → the out takes a voice arg. Either way, ONE accessor, explicit voice, no hardcoded 0.

## CONTRACT TESTS (the invariants that make the class un-writable)
1. ONE-PRODUCER: assert the bars, the prob-out, and the engine draw all return the SAME resolver value
   for the same (voice, lane, step) [modulo the latch — see 3]. No path computes it independently.
2. OWNERSHIP (one-way): East-delegated lane → East edit changes East bars+output, Macro bars+output
   UNCHANGED. Macro-delegated → Macro drives, East inert. (Kills the leak.)
3. VOICE: bars and output read the SAME voice (no voice-0 hardcode divergence).
4. DIRECTION: all 7 lanes' direction resolves through resolveDirValue; VAR/LEG follow Macro like 0-4.
5. RATE: the heavy resolve does NOT run per-sample (assert call-count at step rate, not sample rate) —
   the CPU guard.

## DONE =
- ONE resolver accessor for effective (voice,lane,step) probability (spread via SpreadPolicy delegation
  + warp mix); macroOwnLaneProbability + macroOwnProbability-logic + SpreadInterp-in-UI DELETED.
- Bars, prob-outs, engine draw, direction — all read the resolver; explicit voice, no hardcoded 0.
- Rate split: heavy resolve at step (cached), thin live overlay at frame for bars; output latched.
- Contract tests 1-5 green; bash test/run_all.sh green.
- PR: "needs Rack re-test — bars==output source (not value, due latch); East-delegated East edits don't
  touch Macro; direction follows Macro on VAR/LEG; CPU: heavy resolve off the per-sample path."

## NOT THIS SPEC
- The rate MANAGER (the full per-mode-chain scheduler) — that comes AFTER, scheduling this clean graph.
  This spec does the MINIMAL rate split (step-cache the heavy resolve) needed to not run per-sample.
- The seam/strong-types work (parallel track).
