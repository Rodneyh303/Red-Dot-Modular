# CC BRIEF — SIMD the sumZ reduction (horizontal over K) + cache the weights

## >>> DO THIS FIRST (before the mix cache). Rodney's order: sumZ -> mix. <<<
Self-contained in MovingAverageCopula::sumZ, bit-verifiable independently, makes sumZ faster EVERYWHERE
it's called -> the mix cache is then built on an already-fast sumZ. Within this brief: (1) weight-cache
(free, bit-identical) -> commit; (2) horizontal SIMD, fixed reduction order, RE-BASELINE the reversibility/
1e-15 test to the SIMD order -> commit. THEN do plans/mix_scrub_fix_cc_brief.md.

Do WITH / AFTER the mix cache (plans/mix_scrub_fix_cc_brief.md) — same code (MovingAverageCopula::sumZ +
the blend() callers). The mix cache reduces HOW OFTEN sumZ runs (scrub path cached out); this makes each
remaining sumZ (at ROLL / position-cross / phrase boundary, where z0/z1 are freshly computed over all
steps x lanes x voices) FASTER.

## Current sumZ (MovingAverageCopula.hpp)
    static double sumZ(const double* z, double r) {
        if (r == 0.0) return z[0];
        double w[K]; weights(r, w);            // <-- recomputed EVERY call, same r
        double s = 0.0;
        for (j<K) s += w[j]*z[j];              // <-- the K=64 dot product
        return s;
    }
K = 64 (fixed). weights(r,w) = geometric r^j, L2-normalised (two K-loops: fill+sumsq, then normalise).

## Three layered wins (all compose, all bit-exact-able)
### (1) CACHE the weights w[] per r (free, do first)
weights(r,w) is r-dependent, z-INDEPENDENT. Slew r is now PHRASE-FIXED, so w[] is phrase-stable. On a roll
(recompute z0/z1 for all 16 steps x lanes x voices at one r) weights() is currently redone every call with
the SAME r -> pure waste. Cache w[] (per stream, per r); recompute only when r changes (phrase boundary).
Keep a separate cached w[] for the two scrub windows share the same r. Bit-identical.

### (2) HORIZONTAL SIMD the dot product Sum w[j]*z[j] (the decided axis)
Vectorise the 64 taps of ONE sumZ (horizontal), NOT across voices (vertical). Decided earlier
(ROADMAP/DRAW_BUFFER docs): K=64 is fixed/uniform/large -> trivial always-full packing (64/4 or 64/8),
w[] and z[] already CONTIGUOUS -> direct load, NO gather, NO layout rework, robust to variable-N and
ragged lanes. Vertical's packing (variable voices, ragged 1/2/4 lanes) is the open-ended problem; here it's
avoided.
- Use the SIMDe path: #include "../third_party/simd/simd_compat.hpp" (x86 native + arm64 NEON). Vectorise
  the multiply-accumulate; keep z/w as double (the chain is double for the 1e-15 reversible round-trip) ->
  use the 2-wide (SSE2 __m128d) or 4-wide (AVX __m256d) DOUBLE path. (K=64 -> 32 or 16 vector iters.)
- DETERMINISTIC REDUCTION ORDER: a SIMD reduction sums taps in a different order than the scalar loop ->
  different rounding. Use a FIXED strided/tree order (the AAD reproducible-adjoint technique), SAME every
  call + every platform. The result will differ from the current scalar order -> RE-BASELINE the
  reversibility / 1e-15 round-trip test + bit-compare to the new SIMD order (pre-release, fine). Do NOT
  rely on compiler auto-vectorisation (it reorders non-deterministically).

### (3) (separate) the mix cache reduces sumZ FREQUENCY
Covered in the mix brief. Together: fewer sumZ calls (cache) x faster each (weights-cache + SIMD).

## Verify
- Bit-compare: with a FIXED SIMD reduction order, output is identical to ITSELF across all 3 platforms
  (the key property). It will DIFFER from the old scalar-order output in the last bits -> re-baseline the
  reversibility test + any golden values to the SIMD order; document (not a regression — same algorithm,
  deterministic new summation order). Reversibility/1e-15 round-trip MUST still hold under the new order.
- Measure: roll / fast position-cross / reverse-mode with many voices -> sumZ cost down ~2-4x (double SIMD)
  on top of the weights-cache win. Watch CI (3 platforms — SIMD is the most platform-sensitive; SIMDe +
  the #if SIMD_HAVE_SSE gate handle x86+arm64).
- Keep the r==0 early-out (return z[0]) outside the SIMD path.

## Order
weights-cache (1) first (free, bit-identical, measure) -> then horizontal SIMD (2) (re-baseline the
reversibility test to the fixed order). Separate commits. Compose with the mix cache.
