# Deterministic-FP flags — cost + a possible trim (note, don't forget)

## What's applied, where, and the cost
`DETERMINISTIC_FP := -fno-unsafe-math-optimizations -fno-associative-math -ffp-contract=off`
applied (Makefile) to only TWO plugin TUs: `PatternEngine.cpp.o` and `Monsoon.cpp.o`.
(Also added to the TEST build in run_all.sh so bit-exact float tests — EXPECT_BITEQ in test_TuningTable
etc. — match the plugin + hold across x86/arm64.)

MEASURED cost (per the Makefile comment): ~35% on the window readout (11.2us -> 15.2us), i.e.
~67us -> ~91us per frame while scrub-dragging — UNDER 1% of a 60Hz (16,667us) frame budget. Scoped to
2 TUs, so negligible plugin-wide. Test-build cost is irrelevant (tests aren't perf-critical; the flags
just make bit-exact tests valid).

## The two groups differ in cost vs value
- **-ffp-contract=off** — prevents FMA contraction. CHEAP, HIGH value: this is the CROSS-PLATFORM
  determinism (x86 and ARM contract (a*b)*c into FMA differently -> last-bit diffs). It's what makes
  "same seed = same music on any machine" true, and what makes bit-exact tests pass on arm64 (that was
  the test_TuningTable Mac failure). KEEP everywhere.
- **-fno-associative-math -fno-unsafe-math-optimizations** — prevents reassociation. This is MOST of the
  ~35% cost, and its MEASURED value is ~NIL: reassociation flipping a gate was measured at ~1 in 1e15
  (2M comparisons, 0 flips). Kept as "belt and braces" insurance, not a bug fix.

## The possible TRIM (if perf headroom is ever needed — e.g. 32 steps / offset-scrub)
If the ~1% ever matters, SPLIT the flags: keep **-ffp-contract=off** (cheap cross-platform guarantee) but
DROP **-fno-associative-math / -fno-unsafe-math-optimizations** (expensive, ~nil measured value) -> recover
most of the 35% while keeping FMA determinism. Only do this if profiling shows those TUs are a bottleneck;
the Makefile comment deliberately decided to keep all three for now (1% is negligible, the claim matters).

## Interaction with SIMD (important)
-fno-associative-math BLOCKS float auto-vectorization (compiler can't reorder a sum). No conflict TODAY:
the SIMD Philox is INTEGER + HAND-vectorized (unaffected), and the copula readout is deliberately not
auto-vectorized (determinism). But if LUT-SIMD (step C) is ever done, it must be HAND-vectorized AND
bit-deterministic — you can't rely on the compiler to auto-SIMD float math in these TUs. (This is part of
why LUT-SIMD is the "hard" one.)

## TL;DR
Costs ~1% of frame budget (scoped, measured, deliberate). If headroom needed later: drop the
reassociation guard (nil value), keep -ffp-contract=off (the real cross-platform guarantee). Don't forget
-fno-associative-math and float-auto-vectorization are mutually exclusive on those TUs.
