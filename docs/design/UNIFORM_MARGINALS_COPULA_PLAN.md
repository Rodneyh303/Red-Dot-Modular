# Preserving uniform marginals — copula correlation, A/B mix, slew (Rodney)

STATUS: PLAN, for a LATER BRANCH. Not to be done during the Sands/q-mix firefight; tests green first.
Rodney: **it was never the intention to change the data distribution.** Several stages silently do.

## The principle
Correlating voices, mixing A/B, and slewing should change the RELATIONSHIP between values, never each
value's own distribution. Today they change the distribution, which is a bug in intent — and the same
disease that killed AVERAGE_POLY (averaging N independent draws concentrates at 0.5 → mush; we removed
it for exactly this reason, without noticing the milder cases).

## Where uniformity is lost today
Linearity preserves the RANGE, not the SHAPE. Both of these are linear and both concentrate:
- **A/B mix** interpolates on PAST DRAWS, travelling back over the last 6 pairs as a CHAIN (Rodney) —
  at any mix position it blends the TWO ADJACENT draws in that chain. So each output combines exactly
  two independent uniforms: TRIANGULAR (at m=0.5, peaked at 0.5, half the variance, no mass at the
  extremes). Not affine-on-a-constant, so it does need fixing — but this is the BENIGN case: the
  6-pair chain gives reach back through history WITHOUT compounding concentration, because the
  distribution never sees more than two draws at once. (Had it blended all 6 pairs at once — ~12
  independent draws — CLT would give near-Gaussian with ~1/12 the variance, i.e. AVERAGE_POLY's failure
  in all but name, and A/B mix would have been the single largest distorter in the chain. It isn't.)
- **Slew** is a one-pole across steps = a weighted sum of many past draws → tends to a bell about 0.5 by
  CLT as the time constant grows.
- **Spread** itself: the convex mix `(1-a)d + a·t` has variance `(1-a)²+a²`, which DIPS TO 0.5 at
  mid-spread — mid-range voices visibly flatter (contrast loss). The variance-preserving alternative
  overshoots [0,1]: measured ~8.6% of steps clip at ρ≈0.7, piling mass at 0 and 1.

## The fix: do all distribution-shaping in NORMAL space, map back to uniform
One `Φ`/`Φ⁻¹` pair serves every stage.

**Correlation across voices (Gaussian copula)** — replaces the spread blend:
```
z = ρ·Φ⁻¹(leader) + √(1-ρ²)·Φ⁻¹(own);   r = Φ(z)
```
Marginal stays EXACTLY uniform; rank correlation is ρ to a fraction of a percent; **the knob IS ρ**
(no `k(a) = a/√((1-a)²+a²)` concavity — 0.5 currently gives 0.71); no clipping; no contrast loss; and
the `1-p` mirror special case DISAPPEARS because ρ = -1 gives the exact complement. Block correlation
entries become `ρ_u·ρ_v` exactly, so the matrix arithmetic in SPREAD_TARGET_MODES.md becomes exact.

**A/B mix, uniformity-preserving:**
```
z = (1-m)·Φ⁻¹(A) + m·Φ⁻¹(B);   r = Φ( z / √((1-m)² + m²) )
```
Exactly uniform for every m; m still reads as "how much B".

**Slew → AR(1) on the latent** (temporal correlation, not smoothing):
```
z_t = α·z_{t-1} + √(1-α²)·Φ⁻¹(u_t);   r_t = Φ(z_t)
```
Every `r_t` exactly uniform, consecutive steps correlated at α.
**Behaviour change to accept deliberately:** this is a CORRELATED GENERATOR, not a smoother. Values no
longer glide between steps — each step is a fresh draw that RESEMBLES its predecessor. If the musical
intent is "the probability drifts gradually", that survives; if it is "the value ramps smoothly", apply
the ramp as display/CV smoothing AFTER the uniform value is chosen, not inside the distribution.

## A/B mix and slew are the SAME job — collapse them (Rodney)
Both are TEMPORAL interpolation between draws: A/B mix is a two-tap crossfade between past draws, slew is
a one-pole across steps. In the rework they become ONE normal-space temporal stage rather than two
separate fixes — less work, and one fewer place for uniformity to leak. Keep both user controls (mix
position, slew time) as parameters OF that single stage; they need not become one knob.

## Pipeline order (the coherent architecture)
draws → copula correlation across voices → **one normal-space TEMPORAL stage (A/B mix + slew)** →
**uniform value out**
→ CA remap → any cosmetic smoothing.
Everything that shapes the DISTRIBUTION happens in normal space; everything after operates on a
genuinely uniform value.

**This also settles the stage question** raised against the spread rework: the copula needs raw uniform
draws, so correlation belongs at the DRAW stage, with the dice — not as a late value-level blend on
slewed, post-mix values (which are not uniform, so `Φ⁻¹` there is unfounded: you would get
approximately-right correlation AND a subtly altered distribution — the worst of both). It also sidesteps
the CA-remap ordering problem, since the remap happens after the draws.

## Practical bounds
- Clamp **|ρ| ≤ 0.98** (`Φ⁻¹` diverges at ±1); special-case ρ = 1 to a straight copy for true unison.
- Clamp `Φ⁻¹` inputs to **[1e-6, 1-1e-6]** so a draw of exactly 0 or 1 can't give ±∞.
- Precompute `√(1-ρ²)` (and the mix normaliser) once per voice/lane/phrase, not per step.
- Cost is negligible: this is PER STEP, not per sample — ~16 voices x 5 lanes x 16 steps ≈ 1.3k
  evaluations per phrase. Acklam/Moro rational approximation for `Φ⁻¹`, cheap `erf` for `Φ`.

## Do it on a branch
Changes the generated distribution, so it needs LISTENING, not just reasoning. Land the Sands kit
migration and the q-mix/CA work first. Regression: verify marginals are uniform (histogram the draws per
lane) and that measured pairwise correlation matches the ρ knob.
