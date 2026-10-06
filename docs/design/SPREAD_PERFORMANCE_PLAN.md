# Spread / copula performance — authoritative plan (single source of truth)

STATUS: active. This supersedes any scattered instructions. The spread/slew/mix copula path is too
heavy on all cores (confirmed end-to-end; worst on MinGW — see below). This doc is the ordered plan;
do the stages in order, measure after each.

## Root cause (measured + code-confirmed)
- The per-cell spread apply `SpreadInterp::applyPoly` → `interpolate` → `copula::mix2` → `combine`
  (GaussianCopula.hpp:111-114) does **2× exact PhiInv + 1× exact Phi(erfc) in DOUBLE, per cell.**
- `combine`/`mix2` do NOT use the fast `PhiLUT` — only the slew readout (`applyZ`, PhiFast) does.
- The full spread field (≈7 lanes × 15 voices × 16 steps ≈ 1680 cells) is recomputed at CONTROL rate
  (~1500Hz) in `expanderManager.sync()` → ≈2.5M mix2/sec, each with 3 exact transcendentals.
- **MinGW's `erfc` is pathologically slow** (hence PhiLUT was built for the slew readout). So on the
  MinGW build each mix2 pays 3× slow-erfc-class calls — a big part of the severity.
- Measured: `process` ≈6500ns idle / ≈10500ns running (≈30-50% of the 20800ns/sample budget); cost
  scales with nonzero spread × voices (more live cells).

## Key facts that shape the fix
- **Every consumer reads only a SLICE, never the full field:**
  - Engine (probability read / Lantern / prob-outs): **current step only, all voices, all lanes**, at
    **STEP rate** (read at the step edge).
  - UI (Sands/Macro probability bars): **displayed voice only × all lanes × 16 steps**, at **VIDEO
    rate** (so mix/slew/spread knob turns show live).
  - Nothing reads all-voices × 16-steps at control rate — that combination serves no consumer.
- **Reversibility does NOT require exact Phi — only the SAME Phi forward and back** (Rodney). The LUT
  is deterministic; round-trip is bit-exact by construction as long as forward spread and reverse
  spread both use the identical LUT. Absolute accuracy vs true erf is irrelevant to reversibility.
- Spread/mix2 is downstream of the draw (not part of the reversible DRAW bit-identity). The slew
  readout already uses PhiLUT safely (validated by distribution tests, GaussianCopula.hpp:39). Spread
  is the same class → LUT-safe. (The line-26 "1e-15 contract" refers to PhiInv in the CACHED DRAW
  path, which stays exact; it does not force mix2 to be exact.)

## THE PLAN — ordered by value/effort. Measure after each stage.

### Stage 1 — Fast PhiLUT + new PhiInvLUT for mix2 (biggest win on MinGW, smallest change, shared)
This removes the slow MinGW erfc/PhiInv from the per-cell path. Reversibility preserved (same LUT fwd
and back).
1. **Build a `PhiInvLUT`** (new) — mirror the existing forward `PhiLUT` (4096-entry, linear interp,
   built once from exact PhiInv). Care at the TAILS (PhiInv → ±inf near 0/1); clamp to the existing
   U_EPS bounds (GaussianCopula.hpp:79-80) and tabulate within. Expose `PhiInvFast(p)`.
2. **Route `combine()` through the LUTs:** `Phi(z)` → `PhiFast` (exists); `PhiInv(u[j])` →
   `PhiInvFast` (new). Keep the double mix arithmetic; only the transcendentals become lookups.
3. **SHARED WIN:** the new `PhiInvFast` also serves slew/mix wherever exact PhiInv is used in a hot
   path — route those through it too (check PatternEngine.cpp draw/slew paths). One LUT, two
   beneficiaries (spread mix2 AND slew/mix).
4. **Reversibility rule:** forward spread and reverse spread MUST use the identical LUT path. Verify
   the reverse spread goes through the same PhiFast/PhiInvFast, not exact.
5. **Validate:** distribution tests (design says they MUST run through the LUT); round-trip /
   reversibility tests must stay bit-exact (same-LUT-both-ways guarantees this).
   Measure `process` avgNs — expect a large drop (kills MinGW erfc×3/cell).

### Stage 2 — Restructure: compute only the slices each consumer reads
Only if Stage 1 isn't sufficient (it reduces per-call cost; this reduces call COUNT ~250×). Do in
sub-stages with bit-compare after each (this is the hard-won spread/correlation path — don't
monolith it).
- **2a Engine slice:** compute mix2 for **current step × all voices × all lanes at STEP rate** (step
  advance), cache `polyRandom[*][*][currentStep]`. Remove the full-field rebuild from the control-rate
  `sync()` path. Bit-compare engine probability vs current (same seed/pattern) — MUST be identical.
- **2b UI slice:** compute **displayed voice × all lanes × 16 steps at VIDEO rate** (light divider
  ~60Hz) for the bars. Also fixes mix/slew/spread display responsiveness (video-rate not step-rate).
- **2c** delete the dead control-rate full-field path once 2a+2b serve both consumers.

### Stage 3 — SIMD (only if Stages 1+2 still short; likely unnecessary)
Rack's `rack::simd::float_4` vectorises the per-voice apply 4-at-a-time IF single precision is
acceptable AND the LUT path is used (float LUT output). The restructure's current-step slice produces
a contiguous per-voice (SoA) array that float_4 can load. Decide by measurement after Stage 1+2, NOT
before.

## Do NOT
- Do NOT make the copula float to "save precision cost" — double mix arithmetic is cheap; the cost was
  the transcendentals (fixed by LUT) and the call count (fixed by restructure).
- Do NOT change forward spread without changing reverse identically (breaks reversibility).
- Do NOT restructure (Stage 2) monolithically — bit-compare after 2a and 2b.
- Do NOT reach for SIMD before Stage 1+2 measurement.

## Verify targets
- `process` avgNs → a few hundred (from 6500-10500).
- mix2/applyPoly transcendental cost → ~0 (LUT lookups, Stage 1); call count → ~thousands/sec (Stage 2).
- Reversibility round-trip bit-exact; distribution tests pass.
- UI bars update live at video rate on mix/slew/spread knob turns.
