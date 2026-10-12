# CC BRIEF — slew recompute: align compute with CONSUMPTION (per-step audio + video-rate visual)

Supersedes the blind rate-limit divider (plans/slew_modulation_rate_limit_cc_brief.md). That was the right
instinct (recompute less) but the wrong GRANULARITY. This is the principled fix.

## The problem (measured)
Modulating 3 slews: ~40% CPU at 8 voices, ~80%+ at 16. The rate-limit divider (RECOMPUTE_DIV=32, ~1.4kHz)
helped but modulation is STILL very expensive, because each recompute is enormous:
recomputeEffective* does ALL 16 steps x K=64 window x N voices x lanes, per stream. Even at 1.4kHz x 3
streams x 16 voices that's ~100B ops/sec (each sumZ is ~64 taps + weights + a PhiFast LUT gather).
SIMD alone (4-8x) can't fix 100B ops/sec; and the gather in sumZ caps SIMD anyway.

## The fix — compute EXACTLY what's consumed, when, at what rate (three orthogonal reductions)
The current code recomputes the FULL pattern at control rate "just in case". But nothing consumes that:
- **AUDIO output consumes ONE step's value per step.** In clock mode the output reads the current step
  on each step boundary; between steps nothing reads the recomputed pattern. Each step's slewed value is
  INDEPENDENT (pure fn of the cached z-window at that position — confirmed: recomputeEffective* gathers K
  cached draws around the position and sumZ's them; steps don't depend on each other).
  => Recompute ONLY the step the playhead is about to read, ONLY active voices (voice-opt), at STEP RATE
     (tempo-driven, ~10-50Hz — not 16 steps, not per-sample, not 1.4kHz). ~16x (one step not 16) x the
     step-vs-control frequency drop (~30-100x) = ~500-1600x fewer ops on the audio path.
- **VISUAL (Sands bars) consumes only the DISPLAYED PAGE x DISPLAYED voice/lane, at what the eye sees.**
  => Recompute visuals at VIDEO RATE (~60Hz), ONLY the displayed page's steps x the displayed voice/lane
     (not all steps x all 15 voices x all lanes). A handful of visible bars at 60Hz.
- **VOICE-OPT (already done):** only active voices everywhere.

These are orthogonal and MULTIPLY. Audio path: 1 step x N voices x lanes x K at step-rate. Visual path:
displayed-page x 1 voice x displayed-lanes x K at 60Hz. Neither does the old all-steps x all-voices x
all-lanes at control-rate (the 80%). Expected: 80% -> low single digits.

Principle: compute aligns with CONSUMPTION (which steps, which voices, which lanes, what rate, audio vs
visual). No quality loss — you compute exactly what's heard/seen. No SIMD needed, no determinism risk.

## Verify
- BIT-COMPARE: the per-step audio values must MATCH the old full-recompute values AT the step points
  (same seed, same counter -> identical output). The decomposition changes WHEN/WHAT-SUBSET is computed,
  not the VALUE of any step actually read. Must be bit-identical on all 3 platforms.
- Reversibility / 1e-15 round-trip still holds (per-step compute is still the stateless position-indexed
  window — just computed on demand for the read step).
- Fixed slew still bit-identical.
- MEASURE: modulate 3 slews at 16 voices, VCV CPU meter -> confirm 80% -> low single digits.

## SIMD — VERTICAL across INDEPENDENT STREAMS (not horizontal within one sum) — a clean ~8x
IMPORTANT (Rodney, from AAD/tape experience): do NOT vectorize WITHIN one sumZ (horizontal — the 64 taps
of a single reduction, which needs a cross-lane horizontal sum + a deterministic reduction ORDER + fights
the final gather). Instead vectorize ACROSS INDEPENDENT reductions (vertical): pack 8 independent VOICES
(or steps, or lanes) into the 8 SIMD lanes, each lane running its OWN sumZ over its own window — the
IDENTICAL scalar logic, 8 in parallel. The engine is full of independent reductions (per voice, per step,
per lane — a voice's window sum doesn't depend on another's), so this fits naturally.
Why vertical wins here:
- BIT-IDENTICAL BY CONSTRUCTION — each lane is the exact scalar computation, same order; nothing is
  reordered. The determinism/reassociation concern VANISHES (no reduction-order problem). This is the AAD
  "independent summing of multiple streams" technique.
- NO horizontal-reduction tail (method-1's Amdahl bottleneck) — each lane's result is independent.
- The final PhiFast GATHER also parallelizes (8 voices' lookups = one gather).
- Clean ~8x (AVX2) / ~4x (SSE/NEON) on the WHOLE per-step reduction incl. the transform.
So SIMD here is NOT a caveated "cherry" — it's a clean, bit-exact ~8x with no determinism issue, and it's
arguably EASIER than horizontal (no horizontal sum, no order care). Likely a MUST, not optional. Use the
SIMDe path (src/third_party/simd/simd_compat.hpp) for x86 native + arm64 NEON; bit-compare on 3 platforms.
ORDER: still land the consumption-aligned DECOMPOSITION first (the ~1000x structural win / the actual
fix), then apply vertical SIMD across voices to the remaining per-step reductions (slew sumZ AND the
cross-voice spread/mix2 reduction — same vertical technique, independent streams).

## Method
Decomposition FIRST (audio per-step-when-read + visual video-rate-displayed-page), one piece at a time,
build + bit-compare + reversibility test + commit + PUSH, watch CI (3 platforms), measure CPU. THEN,
optionally, SIMD the sumZ as the cherry.

---

## MIX is bad too — same problem, same rate fix (Rodney)
"Mix" is TWO things; both are slow CHARACTER controls that don't need audio/control-rate modulation ->
same rate principle as slew:
1. **Scrub/blend MIX (rhythmMix/melodyMix/qmixMix):** shares the SAME recompute path as slew — the gate
   checks `MixLatched != MixApplied || SlewLatched != SlewApplied` (PatternEngine.cpp:501+). So modulating
   this mix triggers the same recomputeEffective* -> the DECOMPOSITION ABOVE ALREADY FIXES IT (per-step
   audio + video-rate visual). No extra work.
2. **SPREAD / correlation (mix2 / combineSpread / polySpreadEffective):** a SEPARATE path — runs
   per-voice x per-lane in the MANAGER sync (MonsoonExpanderManager combineSpread loop, lines 426-521,
   one mix2 per voice per lane REST/MELODY/OCTAVE/ACCENT/QMIX). If modulating the SPREAD knob is the
   expensive one, it's THIS path, and it needs the SAME treatment:
   - Recompute combineSpread / polySpreadEffective at CONTROL/VIDEO rate (not per-sample), and ONLY for
     ACTIVE voices (voice-opt helps the cross-voice cost directly — fewer voices = fewer mix2 calls).
   - Spread is a character control; modulating it doesn't need per-sample response.

### DIAGNOSE which mix is the culprit (differential — toggle separately)
Modulate the SCRUB/BLEND mix alone vs the SPREAD/correlation knob alone, watch the VCV CPU meter:
- scrub/blend mix spikes -> it's path (1), fixed by the decomposition.
- spread knob spikes -> it's path (2), the combineSpread manager loop — apply the same rate + active-voices
  treatment there.
Likely both; both get the consumption-aligned rate fix. Rate IS the answer for mix, same as slew.

---

## SIMD AXIS — RECONSIDERED: horizontal (over K) likely best after all (Rodney)
Worked through BOTH. Decision: SIMD over the K SUMMING DIMENSION (horizontal — vectorize the 64 taps of
each sumZ), NOT vertical across voices/lanes. Reasoning (the packing reality flips it):

VERTICAL (across voices/lanes) — bit-exact by construction, BUT the packing is the hard part and it's
open-ended + weakest where we need it:
- Voices are VARIABLE (1-16, gated) -> few-voice patches UNDERFILL the vector (N=4 fills 4/8) — i.e. worst
  exactly in the lean multi-instance sweet spot we care about.
- Lanes are RAGGED (qmix=1, melody=2, rhythm=4) -> don't tile 8-wide.
- One-step-at-a-time removes steps as a packing axis.
- To pack efficiently you must FLATTEN voice x lane AND co-design the z-cache LAYOUT so the packed
  reductions' inputs are contiguous (else gather). Open-ended layout rework.

HORIZONTAL (over K) — the one real cost is a solved discipline; everything else is trivial:
- K = 64 is FIXED, UNIFORM, LARGE -> tiles perfectly (64/8 = 8 full AVX2 vectors), ZERO underfill, no
  raggedness, no variable-count. Trivial, always-full packing.
- NO data-layout co-design — each sumZ's window zw[0..63] is ALREADY contiguous -> load direct, no gather,
  no flatten.
- ROBUST to variable-N and ragged lanes — every sumZ is the same 64-tap reduction regardless of voice
  count or which lane.
- COST = deterministic reduction ORDER (SIMD sums taps in a different order than scalar). SOLVED: use a
  FIXED strided/tree order (same every call, same every platform) — the AAD reproducible-adjoint
  technique. Re-baseline the reversibility/1e-15 test to the SIMD order (pre-release, fine). Bit-compare
  confirms + stays identical across platforms.
- Horizontal-tail (one cross-lane add per reduction) is amortized over 64 taps (~10% tax), not dominant.

Net: horizontal's hard part (fixed-order determinism) is BOUNDED + KNOWN; vertical's hard part (packing a
ragged/variable space + layout co-design, underfilling at low N) is OPEN-ENDED + worst in our target case.
So HORIZONTAL over K wins on practicality. Use the SIMDe path (simd_compat.hpp) x86+NEON; fixed reduction
order; bit-compare on 3 platforms; re-baseline the reversibility test to the SIMD order.
(Still land the DECOMPOSITION first — the ~1000x structural fix — then horizontal-SIMD the per-step sumZ.)
