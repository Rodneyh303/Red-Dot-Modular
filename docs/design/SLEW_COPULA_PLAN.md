# Slew via moving-average Gaussian copula — plan (master)

**CORRECTION (checked against the code): the draws ARE directly addressable — there is no carried
state to work around, and no window class is needed.** `philoxRhythmAt(pos, cursor)` is
`rhythmPhilox.atUniform(pos*DRAW_CHUNK + cursor)`, and `rawDraw*PatternAt(pos)` walks the cursor
across that chunk, so ANY pos (including negative) is computable directly. This is exactly what
`DICE_SCRUB_SLEW_B2.md` specified ("no chain, no stored walk state"). An earlier revision of this
doc wrongly proposed a carried head/tail window stepped by the bijection — that would ADD state to
a design whose whole point is having none. Do not build it. (The confusion: the LIVE draw path
`rhythmCursor++` is stateful, but the ADDRESSED path is not — two different entry points.) See also
`UNIFORM_MARGINALS_COPULA_PLAN.md` (why), `DICE_SCRUB_SLEW_B2.md` (the B2 readout this replaces),
`SPREAD_TARGET_MODES.md` (the other consumer of the same primitive).

## Intent (Rodney)
Slew controls **how far the next draw can move from the last one**.
Requirements: **stateless-at-readout / reversible**, **preserve the lane's distribution**,
**controlled motion**.

## ALREADY BUILT AND GREEN on master — do not re-implement
- `src/dsp/GaussianCopula.hpp` — `Phi` (erfc form), `PhiInv` (Acklam + one Halley step; round trip
  ~1e-15 — supersedes the AS241 note in the old draft), `combine(u, w, n)`, and
  `mix2(own, leader, rho)` for SPREAD (rho = +1 -> leader, -1 -> exactly `1 - leader`, 0 -> own, so
  the legacy `1-p` mirror special case disappears).
- `src/dsp/MovingAverageCopula.hpp` — `K = 64`, `R_MAX = 0.97`, geometric weights `w_j ∝ r^j`
  L2-normalised (`SUM w^2 == 1`); `apply(u, r)`, `weights(r, w)`, `lagCorr(r, m)` (analytic).
- `test/test_GaussianCopula.cpp` — registered in `test/run_all.sh`, suite green. Covers: Phi/PhiInv
  round trip; `SUM w^2 == 1`; **r == 0 bit-identity**; uniform marginal at r = 0/0.3/0.6/0.9/0.97
  (mean, variance ~1/12, KS **after thinning by K**); empirical lag-m vs analytic; `mix2` endpoints;
  determinism.
  NOTE on the KS test: KS assumes independent samples, but consecutive outputs are autocorrelated BY
  DESIGN, so the raw series over-rejects at high r. Thin by K (samples ≥ K apart share no source
  draws) — this is why the old draft's "thin by K" instruction is mandatory, not optional.

## Model
```
r == 0 -> return the legacy draw u_n BIT-IDENTICALLY        (no migration, no behaviour change)
r  > 0 -> z_n = SUM_{j<K} w_j(r)·PhiInv(u_{n-j}),  w_j ∝ r^j,  SUM w^2 = 1
          out = Phi(z_n)  -> EXACTLY uniform -> existing lane quantile unchanged
```
`K = 64`, `r` clamped to `[0, 0.97]`, non-finite r treated as 0. Lag-m correlation is the analytic
dot product `SUM_j w_j·w_{j+m}`; lag ≥ K is exactly 0. As K grows lag-1 -> r, so the knob reads as
**"correlation with the previous draw"**.

**KEY PROPERTY: `r` enters ONLY at readout, never the chain.** The chain therefore reverses exactly
under ANY slew modulation — no constant-slew requirement (unlike recursive slews). Output replay on
reverse is exact **iff `r(n)` is reproducible at step n** (lane-driven, not live CV) — state that in
the UI/docs.

REJECTED, do not reintroduce: clamping a variance-preserving linear mix (piles mass at the edges);
truncating the window (under-weights edges, breaks pure-function-of-position — and
`DICE_SCRUB_SLEW_B2.md` forbids origin truncation: always read the full K, into NEGATIVE counters,
since Philox is a bijection over the signed space); AR(1)/recursive (carries state, unstable
backward, degenerate at rho = 0); linear averaging of the window (today's bug — an equal K-tap
average has ~1/K the variance: AVERAGE_POLY's failure mode).

## The work

### 1. Readout only — no window class
Feed `MovingAverageCopula::apply` from a loop over `rawDraw*PatternAt(pos - j)`, j = 0..K-1 —
exactly as the existing 7-tap loop does with `SCRUB_K`. No carried state, no bijection stepping,
no head/tail. Reversibility comes free from direct addressing, as it does today.
- **Recompute the full K-term sum at every position — never a running sum.** (Still the rule; with
  direct addressing there is no temptation to carry one.)
- **COST — the real open question at K = 64.** Each `rawDraw*PatternAt` fills a whole draw
  (16 steps x several lanes x 15 voices ~ 500 Philox calls). 64 of those is ~32k Philox evaluations
  per position, against ~3.5k for today's 7 taps. Measure it. Mitigations, in order: cache drawn
  patterns by position (they are pure functions of pos, so caching is reversal-neutral); cache
  `PhiInv` of each cached value; or reconsider K. Report before wiring.

### 2. Readout
Call `MovingAverageCopula::apply(window, r)`. Keep `r == 0` on the exact legacy path (the class
already returns `u[0]` bitwise, but assert it through the ENGINE path too).

### 3. Scrub and K are different things
B2 used `SCRUB_K = 6` for BOTH the scrub span and the smoothing window. Scrub still spans 6 positions
(`s = mix*6`); the copula window is `K = 64`. Do not conflate them. Scrub interpolates two adjacent
positions — do that interpolation in NORMAL space (blend the two `z` values, then `Phi` once), not on
the two uniform outputs.

### 4. All three streams
Slew is per STREAM (R / M / Q), not per lane; lanes inherit their stream's r. Precompute
`weights(r)` once per (stream, position) — not per lane, not per voice.

## Tests to add with the wiring
1. **Legacy bit-identity at r = 0 through the real engine path** (not just the copula unit).
2. **Reversibility:** random ± walks return **bitwise** to the start, including with `r` varying per
   step; forward/backward/shuffled evaluation agree; large and negative counters work.
3. **Distribution preserved through the engine:** thin by K, then KS (α = 0.01) and a 100-bin
   chi-square; also map through a lumpy 7-note weighted quantile and chi-square the note histogram.
4. **Motion:** empirical lag-1 of `PhiInv(output)` matches `lagCorr(r,1)` within 4 standard errors;
   lag-K ≈ 0; mean |Δnote| non-increasing in r.
5. **Periodicity:** with a loop length set, `out(n) == out(n+L)` bitwise and no discontinuity in the
   lag-1 statistic at the seam.

## Open decisions (flag in the report, do not silently choose)
- Whether `K = 64` and `R_MAX = 0.97` are right. Truncation makes rho1 < r near the top.
- Knob mapping: linear in r, or inverted so the knob sets target rho1 directly.
- Whether the lane's pitch ordering in the quantile gives the perceived motion we want.
- Cost: 64 PhiInv per position per stream. Measure ns/step; if it bites, cache `PhiInv(u)` alongside
  the carried window (it is a pure function of the draw, so caching is safe and reversal-neutral).
