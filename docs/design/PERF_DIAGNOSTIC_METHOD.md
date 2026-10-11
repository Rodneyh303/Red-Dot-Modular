# Finding the next bottleneck — differential observation, NOT profiling (Rodney)

## How the copula bottleneck was actually found
NOT by profiling — profiling wasn't helping / couldn't be figured out. The copula cost was found by
DIFFERENTIAL OBSERVATION: noticing that NONZERO CORRELATION spiked the cost big time. That pointed
straight at the mechanism (the correlation/mix2 path — erfc Phi/PhiInv — that only runs when correlation
!= 0).

## Why profiling fails here (and differential works)
- VCV plugin work is spread across control-rate + audio-rate callbacks, interleaved with the whole Rack
  engine + other modules -> sample profilers get swamped.
- The copula cost was CONDITIONAL (expensive only at nonzero correlation/spread). A flat profile of a
  zero-correlation patch shows NOTHING. A profiler doesn't tell you "cheap at spread=0, 50x worse at
  spread=1" — it hides the conditionality.
- DIFFERENTIAL observation directly associates "this param/feature = this cost", which is far more
  actionable (it points at the MECHANISM, not just a hot line).

## The method for the NEXT bottleneck (use this, not profiling)
Toggle each expensive-looking feature to its extreme and watch the VCV PER-MODULE CPU METER (crude but
live + per-module). Whatever STILL spikes post-16x is the real next target:
- **Max correlation/spread** -> if it STILL spikes post-16x, the remaining cost is the spread APPLY across
  voices (combineSpread/applyPoly), not the draw. (SIMD-across-voices candidate IF arithmetic-dominated —
  but check: mix2 touches the LUT, so it may be gather, not pure arithmetic.)
- **Max voices (16 vs 1)** -> per-voice processing. Addressed by VOICE-OPT (active-voices), which also
  serves the MULTI-INSTANCE goal -> probably do voice-opt (helps both) rather than SIMD.
- **Deep slew vs zero** -> the MA readout / reduction (sumZ). A dot-product = SIMD-friendly, BUT a SIMD
  reduction reorders float adds -> breaks bit-identity unless hand-written with a deterministic order
  (+ -ffp-contract=off). Harder than integer Philox.
- **Fast scrub (once offset exists)** -> draw regen (should be handled by 16x + bidirectional buffer).

## Discipline
Do NOT speculatively SIMD. The copula was fixed because it was THE thing that spiked (correlation on).
Find the next target the same way — toggle, watch the meter — then SIMD only the arithmetic-dominated one
that actually spikes. For the multi-instance goal, shared-draw + voice-opt likely beat more
single-instance SIMD (sharing work across N instances > making 1 instance 2x faster).
Note the determinism tension: float SIMD (spread apply, MA reduction) needs controlled reduction order +
-ffp-contract=off to stay bit-exact — don't rely on compiler auto-vectorization (which the dropped
reassociation guard now permits but which would reorder non-deterministically).
