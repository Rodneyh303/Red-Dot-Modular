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


---

## Phi on the slew readout: use an INTERPOLATED LOOKUP TABLE (measured)

**Why.** After caching `PhiInv` (pure function of the draw, so 63 of 64 window entries survive a
one-position advance — cold build 34,816 calls, steady state 544), the readout cost moves to `Phi`.
`std::erfc` measures **~80 ns on MinGW64** (vs ~14 ns on glibc — this is a known MinGW libm
weakness), so 544 `Phi` per window is ~44 us — the dominant remaining term.

**What NOT to use:**
- **Winitzki** — measured max error **6.2e-5**, three orders worse than the ~1e-7 the float
  probability lanes need. Rejected on accuracy.
- **A&S 7.1.26** — accuracy is fine (measured **7.0e-8**) but it calls `exp`, and it measured SLOWER
  than `erfc` on the target toolchain. Rejected on speed. (Measure `exp` alone on MinGW to confirm
  the whole exp-based family is out.)

**Use instead: a half-range interpolated LUT over z in [0,6], with the symmetry
`Phi(-z) = 1 - Phi(z)`.** Measured (float table, linear interpolation):

| table | max abs err | monotone | ns/call (glibc) | size |
|---|---|---|---|---|
| 4096 | **9.3e-08** | yes | 3.0 | 16 KB |
| 8192 | 4.5e-08 | yes | 3.0 | 32 KB |

Take **4096** — it meets the 1e-7 target, is no slower than 8192, and is kinder to cache. No
transcendental call at all, so it sidesteps the MinGW libm problem entirely (expect a much bigger
win there than the ~5x seen on glibc).

**Required properties, all verified:**
- **Monotone** across [-7,7] — this is what preserves the uniform marginal. A non-monotone
  approximation would break the guarantee the whole rework exists to provide.
- **Deterministic** — bit-exact reversibility is unaffected; reverse still matches forward.
- Saturates to 0/1 beyond |z| = 6 (correct to ~1e-9), and the half-range symmetry means no accuracy
  loss on the negative side.

**The rule, stated neatly (Rodney): IF IT'S CACHED, IT CAN AFFORD TO BE EXACT.**
The accuracy split falls out of the caching structure — it is not a policy anyone has to remember:

| call site | cacheable? | frequency | precision needed | use |
|---|---|---|---|---|
| `Phi` inside `PhiInv`'s Halley step | YES — `PhiInv` is cached per draw value | 544 on a new position; 34,816 on a cold build | full (or the refinement converges only to LUT accuracy and the 1e-15 round-trip test fails) | **exact `Phi`** |
| `Phi` at the end of the slew readout | NO — advancing one position moves every weight onto a different draw, so all 544 `z` change | every position, incl. every frame while scrub-dragging | ~1e-7 (float probability lane) | **LUT** |
| `Phi` in spread's `mix2` | NO — `z` depends on the live rho | per voice per lane | ~1e-7 (same lanes) | **LUT** |
| `Phi` in the primitive unit tests | n/a | n/a | reference | **exact `Phi`** |

So: cached and accuracy-critical -> exact; uncacheable, hot and float-precision -> LUT.

**Rules:**
1. **Build the table at static-init from the EXACT `Phi`** — one source of truth, no transcribed
   constants.
2. **Use it for the whole PROBABILITY PIPELINE — the slew readout AND spread's `mix2`** (Rodney:
   "slew readout only?" — that split was arbitrary). Both produce probability values for the same
   float lanes, with the same ~1e-7 need, the same monotonicity requirement and the same uniform-
   marginal guarantee; running two different `Phi` implementations over one pipeline would give
   subtly different values from two functions doing the same job, for no benefit.
   **Keep the EXACT `Phi` for (a) `PhiInv`'s Halley refinement** — it needs full precision or the
   refinement converges only to LUT accuracy and the 1e-15 round-trip test fails — **and (b) the
   unit tests of the primitives themselves.**
   The line is PROBABILITY VALUES (LUT) vs INTERNAL PRECISION (exact), not slew vs spread.
3. **The distribution tests (KS / chi-square / marginal) MUST run through whichever `Phi` actually
   ships in the probability pipeline.** Otherwise the tests stop testing the product. A 9e-8 error will not
   move a KS result, but the test must exercise the real function.

**What this does NOT fix, and why:** advancing one position shifts every weight onto a different
draw, so all 544 `z` values change — `Phi` and the ~35k MACs cannot be cached across positions the
way `PhiInv` can. There IS an exact O(1) recursive update for geometric weights
(`S(n) = x_n + r·S(n-1) - r^K·x_{n-K}`), but reversing it requires dividing by `r`, which is not
bit-exact in floating point, so forward and backward would drift. That is exactly why the full
K-term recompute is mandatory. Expected steady state after the LUT: ~1.6 us of `Phi` + ~7 us of MACs
per position, i.e. ~9 us — so a scrub drag (2 windows x 3 streams) lands around 54 us/frame, well
under 1% of a 60 Hz budget, down from ~0.7 ms.

---

## IDEA (parked, Rodney): scrub RANGE — make adjacent positions modulatable

**Problem.** Scrub spans 6 positions on one knob (`s = mix*6`), so a CV sweep between two ADJACENT
draws (0->1 back, or 1->2 back) uses only a sixth of the input range. Fighting attenuator precision
to get a controlled morph between two neighbouring patterns. This is about MODULATION RESOLUTION,
not manual positioning.

**Fix: a window into the history**, so scrub's full travel (and full CV range) maps onto just the
region of interest.
- **Depth only** (`scrub spans 0..D`) — simple, but always anchored at 0, so you still cannot get
  full resolution between e.g. 3 and 4.
- **Span + offset** (`scrub spans offset .. offset+span`) — any two adjacent positions can fill the
  whole travel. Solves the stated case properly.
- **Minimal variant**: OFFSET only, span fixed at 1 — "offset picks the PAIR, scrub morphs within
  it". Arguably the cleanest for the exact problem described.

**Engine cost: nil.** Scrub already computes `s = mix*6` and reads `N-f` / `N-f-1`; this only changes
how the knob maps to `s`. No new draws, no new state, reversibility untouched (still a pure function
of position).

**Panel:** Rodney — "maybe two small knobs for range"; also floated a CONTEXT-MENU setup "like the
mode C/D melody choices". Monsoon is at 45HP after the Big-Five widening, so weigh two small knobs
vs menu + existing knob (e.g. expose OFFSET physically, keep SPAN in the menu).

**Decide when changing depth/offset:** preserve the ABSOLUTE position (zoom in around where you
are), not the knob fraction (which would make the position jump). Almost certainly what is wanted.

**Related, cheap, different feature — SNAP.** A "snap scrub to whole positions" menu toggle lands
exactly ON a past pattern with no interpolation (`f = 0` is the only way to hear a past draw
unmixed). Exact recall rather than a blend. Costs no panel space, and CV + an external quantiser
already approximates it.
