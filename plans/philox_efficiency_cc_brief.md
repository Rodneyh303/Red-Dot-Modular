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
DO B NOW (Rodney), right after A, in the SAME pass — A and B touch the SAME code (the Philox block
generation / rawDraw*PatternAt), and A's consecutive-counter fillBlock is exactly what B's SIMD consumes.
Restructure the draw hot path ONCE: batch (A) + vectorize (B) together. (The offset-scrub/reverse-mode
load is the WHY it's worth it, but we do it now while in this code, not deferred.)

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
A + B NOW, same pass (both touch the Philox block gen; A sets up the consecutive-counter layout B's SIMD
consumes -> restructure the draw hot path once). Then C (LUT SIMD) LATER, only if a fast CV offset-scrub
at 32 steps still stutters after A+B — measure the LUT gather + z-index clustering first. Keep the flat
consecutive-counter layout throughout (good for both batching and SIMD).

### Sequencing within A+B
1. A first: batch via fillBlock, build + BIT-COMPARE PASS + commit (lock in the bit-identical 4x before
   adding SIMD complexity).
2. Then B: vectorize the block gen (float_4 / AVX2) feeding from A's batched access, build + BIT-COMPARE
   PASS (still bit-identical — same algorithm in parallel lanes) + measure the speedup + commit.
Keep A and B as SEPARATE commits so if B's SIMD has a platform issue (it's new intrinsic code, 3 OSes via
CI), A's bit-identical win stands alone and B can be isolated. CI now builds this branch on Win/Mac/Linux
— watch that B compiles on all three (SIMD intrinsics are the most platform-sensitive code; prefer
rack::simd::float_4 which abstracts the platform over raw _mm_ intrinsics).

---

## STEP B SIMD — intrinsics decision (Rodney)
CC found: rack::simd int32_4/float_4 lack integer multiply/shift/shuffle/blend (int32_4 multiply "NOT
AVAILABLE IN SSE3"). Philox's core 32x32->64 unsigned multiply (needs hi AND lo 32 bits) requires raw
_mm_mullo_epi32 / _mm_mul_epu32 / _mm_shuffle_epi32 / _mm_blend_epi16.

DECISION:
- **Option C is RULED OUT** — float-based 16-bit decomposition risks float rounding -> NOT bit-identical ->
  breaks the bit-compare verification AND reversibility (the chain must reproduce exactly). Philox is
  integer arithmetic; it MUST stay integer arithmetic. Non-negotiable.
- **Use Option B** — raw __m128i throughout the SSE kernel (consistent; the natural idiom for a vectorized
  integer PRNG). The int32_4 abstraction buys little here since multiply/shuffle/blend are raw anyway, and
  mixing int32_4 + .v adds wrap/unwrap friction. Include the SDK's platform-detection header.

### CRITICAL — PORTABILITY: SSE intrinsics DON'T compile on arm64 (Apple Silicon)
The macOS CI build is a UNIVERSAL binary (x86_64 + arm64). `_mm_*` SSE intrinsics are x86-only — they DO
NOT EXIST on the arm64 slice -> SSE-only Philox will FAIL to compile on Mac. MUST gate it:
    #if defined(__SSE4_1__)   // x86: Windows, Linux, Intel-Mac slice
        ... SSE Philox kernel ...
    #else                     // arm64-Mac slice (and any other arch)
        ... existing SCALAR Philox ...
    #endif
So x86 gets the SIMD speedup; arm64-Mac gets the scalar path (CORRECT, BIT-IDENTICAL, just unaccelerated).
All paths produce the SAME Philox outputs -> bit-compare passes on every platform.
(Optional LATER: add a NEON path for arm64-Mac speedup — but scalar-fallback FIRST; Apple Silicon scalar
is fast, may not need NEON.)

### Verify B
- Bit-compare passes on ALL 3 CI platforms (SSE path on Win/Lin, scalar path on arm64-Mac both bit-identical).
- B compiles on all 3 (watch Mac especially — the #ifdef gate is what makes it compile on arm64).
- Measure the x86 speedup (expect ~4-8x on block gen on top of step A's 4x).
- Keep A and B separate commits.
