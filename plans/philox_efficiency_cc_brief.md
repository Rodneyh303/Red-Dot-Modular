# CC BRIEF — Philox efficiency: 4x output batching, then SIMD Philox

Nonce: DROPPED (not a C++26 Philox API feature — it's just counter-partitioning; buys no compute, breaks
draw-compat, and would complicate the SIMD consecutive-counter batching). Keep the flat
`pos*DRAW_CHUNK + cursor` layout.

## STEP A (DO NOW) — 4x Philox output batching. FREE 4x, BIT-IDENTICAL.
### The waste (confirmed)
`rawDrawRhythmPatternAt` (PatternEngine.hpp:734) calls `philoxRhythmAt(pos, c++)` ~1024 times/step (c runs
0..~1024 consecutive). Each `atUniform(x)` = `block(x>>2)[x&3]` RECOMPUTES the Philox block. 4 consecutive
c values recompute the same block 4x, using 1 of 4 outputs each -> 75% of Philox work wasted.
Same in rawDrawMelodyPatternAt (melody+octave+polyMelody) and rawDrawQmixPatternAt.
### The fix
`c` is perfectly CONSECUTIVE (c++), so batch via the existing `fillBlock(base, out, n)`
(PhiloxRng.hpp:174) — compute each block ONCE, consume all 4 outputs. Replace the per-cursor
`philoxXAt(pos, c++)` loop with a single `fillBlock((uint64_t)pos*DRAW_CHUNK, buf, N)` into a scratch
buffer, then scatter buf[] into the draw struct fields in the same order. N = the cursor count per step's
stream (rhythm: 4 + 15*4 = 64 per step-i, x16 steps; melody: 2+15; qmix). BIT-IDENTICAL (same Philox
outputs, just not recomputed) -> bit-compare MUST pass. ~4x fewer Philox block computations on the draw
hot path.
### Verify
Bit-identical output vs before (same seed). Measure the draw-gen cost drop. Build + test + bit-compare +
PUSH.

## STEP B (NEXT, after A lands) — SIMD Philox. 4-8x more, on top of A.
Philox is the IDEAL SIMD target: branchless, fixed 10-round loop, pure 32x32->64 mul + XOR + add, no
divergence, DESIGNED for parallel counters (Salmon et al). Natural axis: the consecutive counters from A's
fillBlock -> compute 4 (SSE/float_4) or 8 (AVX2) Philox BLOCKS at once.
- Use rack::simd (float_4) or raw intrinsics; no existing SIMD in the codebase yet (new code).
- Vectorize philox4x32_10 across 4/8 consecutive block positions; feed from the batched fillBlock.
- Still BIT-IDENTICAL (same algorithm, same outputs, computed in parallel lanes) -> bit-compare holds.
- Gain: ~4-8x on Philox block gen; combined with A's 4x -> ~16-32x on raw-draw generation.
Gate this on NEED: do it when the offset-scrub / reverse-mode load (sweeping uncached positions, 2x at
32 steps) justifies it. The concrete test: "fast CV offset sweep at 32 steps stays smooth?"

## STEP C (DEFERRED, conditional) — LUT SIMD only if still needed after offset-scrub.
PhiInv (cached, erfc+Halley) is COLD + branchy -> don't SIMD. PhiLUT/PhiInvLUT (hot readout) are table
lookups: the interp math vectorizes but the LOOKUP is a GATHER (data-dependent addresses) — SIMD's weak
spot (gather serializes / high latency). So LUT-SIMD is HARD and sub-linear (~2-3x, gather-bound), and the
LUT already captured its win in SCALAR form (it traded arithmetic FOR a memory load — the opposite of what
SIMD wants). LUT vs SIMD are ALTERNATIVE optimizations, not complementary.
Only revisit if, after A+B, a fast CV offset-scrub at 32 steps STILL stutters. Then MEASURE whether the
LUT gather is actually the bottleneck and whether the z-indices CLUSTER (adjacent z -> same cache line ->
contiguous load not scatter -> SIMD-able). If scattered, LUT-SIMD may not pay; consider other options.

## Order
A (now, free 4x, bit-identical) -> B (SIMD Philox, when offset-scrub/reverse-mode needs it) -> C (LUT
SIMD, only if B still insufficient, measure first). Keep flat consecutive-counter layout throughout (good
for both batching and SIMD).
