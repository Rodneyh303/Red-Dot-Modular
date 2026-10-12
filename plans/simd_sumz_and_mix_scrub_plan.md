# Plan — SIMD sumZ + mix/scrub fix (two briefs, sequential, new branch)

## Branch
New branch off `lane-expander-refactor` (e.g. `simd-sumz-mix-scrub`).

## Architecture overview

```mermaid
graph TD
    subgraph Phase A: simd_sumz_cc_brief.md
        A1[A1: weight-cache w[K] per r] --> A2[A2: horizontal SIMD dot product]
    end
    subgraph Phase B: mix_scrub_fix_cc_brief.md
        B1[B1: cache z0/z1 per position per phrase] --> B2[B2: swap blend to OU-bridge]
        B2 --> B3[B3: video-rate displayed-voice bars]
    end
    A2 --> B1
```

## Current call graph (what we're optimising)

```mermaid
graph TD
    RE[recomputeEffectiveRhythm/Melody/Qmix] -->|"per step i, per lane, per voice v"| blend[blend lambda]
    blend -->|"gather K=64 z values"| sumZ0[sumZ zw0 r -> z0]
    blend -->|"gather K=64 z values"| sumZ1[sumZ zw1 r -> z1]
    blend --> PhiFast[PhiFast 1-frac*z0 + frac*z1]
    sumZ0 --> weights[weights r w — 2x K-loop, SAME r every call]
    sumZ1 --> weights
    sumZ0 --> dotprod[dot product s = Σ w[j]*z[j], K=64 scalar loop]
    sumZ1 --> dotprod
```

**Problem**: `weights(r, w)` is recomputed every sumZ call with the same r (waste). The dot product is scalar. On mix modulation, the entire blend (two sumZ per step/lane/voice) reruns even though only frac changed.

---

## Phase A — simd_sumz_cc_brief.md

### A1: Weight-cache w[] per r (free, bit-identical)

**What**: Cache the K=64 L2-normalised geometric weights per r. Since slew r is phrase-fixed (the slew-phrase fix landed), `weights()` is called with the SAME r for all 16 steps × lanes × voices in one recompute. Currently it recomputes w[] every call.

**Files**:
- [`MovingAverageCopula.hpp`](Red-Dot-Modular/src/dsp/MovingAverageCopula.hpp:33) — add `WeightCache` struct + `sumZ` overload
- [`PatternEngine.cpp`](Red-Dot-Modular/src/dsp/engines/PatternEngine.cpp:244) — `recomputeEffective*` blend lambdas
- [`PatternEngine.hpp`](Red-Dot-Modular/src/dsp/engines/PatternEngine.hpp:906) — `patternRhythmAt/MelodyAt/QmixAt` (applyZ callers)

**Design**:
1. Add to `MovingAverageCopula`:
   ```cpp
   struct WeightCache {
       double w[K];
       double cachedR = 999.0;  // invalid sentinel
       bool matches(double r) const { return r == cachedR; }
       void update(double r) { weights(r, w); cachedR = r; }
   };
   ```
2. Add `sumZ` overload that takes pre-computed weights:
   ```cpp
   static double sumZ(const double* z, const double* w) {
       double s = 0.0;
       for (std::size_t j = 0; j < K; ++j) s += w[j] * z[j];
       return s;
   }
   ```
   (Keep existing `sumZ(z, r)` for backward compat — it calls `weights` then the new overload.)
3. PatternEngine holds `WeightCache` per stream (rhythm/melody/qmix):
   ```cpp
   redDot::MovingAverageCopula::WeightCache rhythmWeightCache_;
   redDot::MovingAverageCopula::WeightCache melodyWeightCache_;
   redDot::MovingAverageCopula::WeightCache qmixWeightCache_;
   ```
4. In `recomputeEffectiveRhythm` (and Melody/Qmix twins):
   - At the top: `if (!rhythmWeightCache_.matches(r)) rhythmWeightCache_.update(r);`
   - In `blend()` lambda: replace `sumZ(zw, r)` with `sumZ(zw, rhythmWeightCache_.w)`
5. In `patternRhythmAt/MelodyAt/QmixAt` (the `applyZ` path): same pattern — cache weights, pass to `applyZ` overload (or inline `PhiFast(sumZ(zw, wc.w))`).

**Verify**: Bit-identical (same computation, just cached). Run `test_slew_copula` + `test_GaussianCopula` + full `run_all.sh`. Commit + push.

### A2: Horizontal SIMD the dot product (re-baseline reversibility)

**What**: Vectorize `s = Σ w[j]*z[j]` (K=64 doubles) using SIMDe. Fixed strided reduction order, same every call + every platform.

**Files**:
- [`MovingAverageCopula.hpp`](Red-Dot-Modular/src/dsp/MovingAverageCopula.hpp:67) — the `sumZ(z, w)` overload from A1
- [`test_slew_copula.cpp`](Red-Dot-Modular/test/test_slew_copula.cpp:80) — re-baseline reversibility test
- [`test/run_all.sh`](Red-Dot-Modular/test/run_all.sh:35) — ensure `-msse4.1` or `-msse2` flag (already present)

**Design**:
1. In `MovingAverageCopula.hpp`, add `#include "../third_party/simd/simd_compat.hpp"` (guarded).
2. Rewrite `sumZ(const double* z, const double* w)` with AVX2 4-wide double:
   ```cpp
   static double sumZ(const double* z, const double* w) {
       // 4-wide lane-independent accumulation, SAME order on all platforms.
       // K=64 / 4 = 16 iterations. No FMA (separate mul+add = 2 roundings, matches scalar).
       // Horizontal sum: (lane0 + lane1) + (lane2 + lane3) — fixed everywhere.
       #if defined(__AVX2__)
       __m256d acc = _mm256_setzero_pd();
       for (std::size_t j = 0; j < K; j += 4) {
           __m256d zj = _mm256_loadu_pd(&z[j]);
           __m256d wj = _mm256_loadu_pd(&w[j]);
           acc = _mm256_add_pd(acc, _mm256_mul_pd(zj, wj));  // NO FMA — 2 roundings
       }
       alignas(32) double tmp[4];
       _mm256_store_pd(tmp, acc);
       return (tmp[0] + tmp[1]) + (tmp[2] + tmp[3]);
       #else
       // Scalar 4-wide: IDENTICAL grouping + order as AVX2 (lane-independent + same h-sum).
       // On arm64 with SIMDe __m256d emulation: same result via 2x NEON __m128d.
       // On x86 without AVX2: scalar fallback, same result (just not vectorised).
       double lane0 = 0.0, lane1 = 0.0, lane2 = 0.0, lane3 = 0.0;
       for (std::size_t j = 0; j < K; j += 4) {
           lane0 += w[j]   * z[j];
           lane1 += w[j+1] * z[j+1];
           lane2 += w[j+2] * z[j+2];
           lane3 += w[j+3] * z[j+3];
       }
       return (lane0 + lane1) + (lane2 + lane3);
       #endif
   }
   ```
   - **Cross-platform bit-exactness**: the 4-wide lane-independent structure is identical on AVX2 (native __m256d), arm64 (SIMDe 2× NEON), and scalar. Each lane does `accN += w[j+N] * z[j+N]` (one mul, one add, two roundings — no FMA). The horizontal sum `(lane0+lane1) + (lane2+lane3)` is fixed everywhere.
   - **No FMA**: `_mm256_mul_pd` + `_mm256_add_pd` (two roundings) matches the scalar `lane0 += w*z` (also two roundings). `_mm256_fmadd_pd` would do one rounding → different result → breaks cross-platform bit-exactness.
   - **AVX2 enable**: add `-mavx2` to x86 build flags in Makefile + run_all.sh. The `#if defined(__AVX2__)` guards native use. Non-AVX2 builds (older x86, test without flag) fall through to the scalar 4-wide path — same RESULT, just not vectorised.
   - **simd_compat.hpp**: add `#include <simde/x86/avx2.h>` alongside the existing sse4.2.h, so arm64 gets __m256d emulation. Define `SIMD_HAVE_AVX2` if `__AVX2__` is defined.
   - Keep `r == 0` early-out in `sumZ(z, r)` (not in the `sumZ(z, w)` overload — the caller short-circuits r==0).
3. **Re-baseline**: The SIMD order differs from the old scalar `for (j<K) s += w[j]*z[j]` order. The reversibility test (forward-then-back) MUST still hold (same order both ways). The r==0 bit-identity test is unaffected (early-out). The variance test is statistical. If any golden-value bit-compare exists, re-baseline it. Document the order change in the commit message.
4. The `apply` and `combine` paths in `GaussianCopula.hpp` are NOT changed (they use `PhiInvFast` per element, not a cached z-window — different path, not in scope).

**Verify**: `test_slew_copula` (reversibility + bit-identity + variance), `test_GaussianCopula`, full `run_all.sh`. Push + watch CI (3 platforms — SIMD is most platform-sensitive; SIMDe + `#if SIMD_HAVE_SSE` handle x86+arm64).

---

## Phase B — mix_scrub_fix_cc_brief.md

### B1: Cache z0/z1 per position per phrase (keep current linear blend)

**What**: Cache the two window sumZ results (z0, z1) per position per phrase. On frac-only changes (scrub within the same integer position pair), skip the expensive K=64 sumZ and reuse cached z0/z1 — only redo the cheap blend (PhiFast).

**Files**:
- [`PatternEngine.cpp`](Red-Dot-Modular/src/dsp/engines/PatternEngine.cpp:244) — `recomputeEffectiveRhythm/Melody/Qmix`
- [`PatternEngine.hpp`](Red-Dot-Modular/src/dsp/engines/PatternEngine.hpp:92) — add cache storage

**Design**:
1. Add cache storage to PatternEngine (per stream):
   ```cpp
   // Cached window sumZ results (z0 = window@N-f, z1 = window@N-f-1).
   // Valid within a phrase (r fixed) + same integer position f.
   // Key: (rhythmDrawCtr, f, r). Invalidate on phrase boundary or f-cross.
   struct ZWindowCache {
       static constexpr int LANES_R = 4 + 4*15;  // rhythm: 4 mono + 4×15 poly = 64
       // Actually: 4 mono lanes (rhythm, variation, legato, accent) + 4 poly lanes × 15 voices
       // Melody: 2 mono + 2×15 poly = 32
       // Qmix: 1 mono + 1×15 poly = 16
       // Simpler: cache as flat arrays sized to the max (64 per step × 16 steps)
       double z0[16 * 64];  // [step][lane-voice] — flat
       double z1[16 * 64];
       int64_t cachedCtr = -999;
       int cachedF = -999;
       double cachedR = 999.0;
       bool valid = false;
   };
   ZWindowCache rhythmZCache_, melodyZCache_, qmixZCache_;
   ```
2. In `recomputeEffectiveRhythm`, split into two paths:
   - **Full recompute** (cache miss): compute z0/z1 for all steps/lanes/voices (current code), store in cache, then blend.
   - **Blend-only** (cache hit: same ctr, same f, same r): skip sumZ, read z0/z1 from cache, only redo `PhiFast((1-frac)*z0 + frac*z1)`.
3. Cache validity check at the top of recomputeEffective*:
   ```cpp
   const int f = (int)s;
   const bool cacheHit = rhythmZCache_.valid
       && rhythmZCache_.cachedCtr == N
       && rhythmZCache_.cachedF == f
       && rhythmZCache_.cachedR == r;
   ```
4. The cache is invalidated implicitly: when the draw counter advances (roll/phrase boundary → new N), or when f changes (position-cross → different windows), or when r changes (phrase boundary → new slew). All three are captured by the (ctr, f, r) key.
5. `forceRecomputeSlewed()` (CA pin remap) sets `rhythmZCache_.valid = false` to force full recompute.
6. Keep the current linear blend `(1-frac)*z0 + frac*z1` — the OU-bridge swap is B2.

**Verify**: Bit-identical for full-recompute path (same computation). Blend-only path produces identical results (same z0/z1, same blend formula). `test_slew_copula`, `test_GaussianCopula`, full `run_all.sh`. **Measure**: modulate MIX at 16 voices → CPU should be flat (blend-only path is O(steps × lanes × voices) PhiFast calls, no K=64 sumZ). Commit + push.

### B2: Swap linear blend to OU-bridge (correctness)

**What**: Replace the linear blend `(1-frac)*z0 + frac*z1` with the OU/AR(1) bridge that preserves the uniform marginal at every frac (including r==0, where the current linear blend distorts).

**Files**:
- [`PatternEngine.cpp`](Red-Dot-Modular/src/dsp/engines/PatternEngine.cpp:272) — the `PhiFast` call in blend()
- [`docs/design/AB_MIX_COPULA_FIX.md`](Red-Dot-Modular/docs/design/AB_MIX_COPULA_FIX.md) — the a/b/v forms

**Design**:
1. The OU-bridge: `X = a(r,frac)*z0 + b(r,frac)*z1 + sqrt(v(r,frac))*xi`, then `PhiFast(X)`.
2. `a`, `b`, `v` are closed-form in (r, frac) — see AB_MIX_COPULA_FIX.md for the exact forms.
3. `xi` = ONE Philox draw from a SEPARATE key domain (not the base draw stream), position+frac keyed → reversible.
4. r < 0 (hocket): keep current behaviour for now (TBD per brief).
5. This fixes the r==0 marginal distortion: at r==0, the current linear blend of two independent N(0,1) values gives variance 0.5 (not 1.0). The OU-bridge at r==0 gives a fresh N(0,1) draw (xi term dominates).
6. Re-baseline: mid-scrub values WILL change (old blend was wrong). Integer positions (frac=0) stay exact (= z0). Document as a correctness fix, not a regression.

**Verify**: Monte-Carlo: variance=1 + endpoint corrs r^f/r^(1-f) at several r,frac. `test_slew_copula` (re-baseline mid-scrub values). Full `run_all.sh`. Commit + push.

### B3: Wire video-rate displayed-voice bar update on Sands

**What**: Show the mix probability-bar modulation on Sands at video rate (~60Hz), displayed voice only. Unlike slew (phrase-locked — bars don't move mid-phrase), MIX is consumed continuously (scrub changes output live) → live bars MATCH output → truthful display.

**Files**:
- [`StraitsEastSandsVisual.cpp`](Red-Dot-Modular/src/StraitsEastSandsVisual.cpp:951) — the `step()` function (UI thread, ~60Hz)
- [`StraitsSandsMacroVisual.cpp`](Red-Dot-Modular/src/StraitsSandsMacroVisual.cpp:454) — the `step()` function (Macro visual)
- [`PatternEngine.hpp`](Red-Dot-Modular/src/dsp/engines/PatternEngine.hpp:92) — expose a cheap per-step blend query

**Design**:
1. Add a `slewedAt(int step, int lane, int voice, float frac)` query to PatternEngine that does ONLY the cheap blend (PhiFast on cached z0/z1) — no sumZ, no full recompute.
2. Sands `step()` (already ~60Hz) calls this for the DISPLAYED voice only (1 voice × displayed page steps × displayed lanes).
3. This is separate from the audio path (which applies mix at step boundary via the cached z0/z1 + blend in recomputeEffective*).
4. PRINCIPLE: display updates at the rate the parameter is consumed — phrase for slew (bars phrase-locked), video/continuous for mix (bars live).

**Verify**: Sands bars track the scrub live (video rate, displayed voice) and match the audio output. Commit + push + watch CI.

---

## Method (all steps)

1. Each step is a separate commit.
2. Before each commit: `bash test/run_all.sh` — all green.
3. After each commit: `git push` — watch CI (3 platforms: Linux x86, Mac x64, Mac arm64).
4. Bit-compare: fixed-slew output must be bit-identical (A1). SIMD order (A2) + OU-bridge (B2) re-baseline documented.
5. Reversibility: `test_slew_copula` forward-then-back must hold after every step.
6. Measure: after B1, modulate MIX at 16 voices → CPU flat. After A2, roll/position-cross → sumZ ~2-4x faster.
