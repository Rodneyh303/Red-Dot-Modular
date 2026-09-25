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
- **Slew is PHRASE MEMORY, not a per-step smoother (Rodney).** For each STEP POSITION it blends how much
  of LAST PHRASE's value vs THIS PHRASE's fresh draw. One phrase of it mixes two independent uniforms →
  triangular; because it is recursive ACROSS PHRASES, a high setting accumulates many past phrases at
  that step position and drifts toward 0.5 over time. Same concentration failure, on a PHRASE clock —
  easy to misread as the pattern "settling".
  Musically this is the RIGHT behaviour and must be preserved: a pattern MORPHS between phrases rather
  than being redrawn — recognisable evolution, not fresh randomness.
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

**Slew → AR(1) on the latent, PHRASE TO PHRASE at a fixed step index:**
```
z_p[j] = α·z_{p-1}[j] + √(1-α²)·Φ⁻¹(u_p[j]);   r_p[j] = Φ(z_p[j])
```
Every value exactly uniform; the SAME STEP in consecutive phrases correlated at α. Note the latent state
is per (STREAM, STEP) — 16 latents per stream — not a single running value. The phrase-memory morphing
behaviour is preserved exactly; only the marginal drift is removed.

## A/B mix and slew are the SAME job — collapse them (Rodney)
Both are TEMPORAL interpolation between draws: A/B mix is a two-tap crossfade along a 6-pair chain of
past draws; slew is phrase memory (last phrase vs this phrase, per step position). In the rework they become ONE normal-space temporal stage rather than two
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

## The payoff: spread modulation becomes literal TIME-VARYING CORRELATION (Rodney)
Not just correctness — a capability. TODAY a spread CV moves a knob whose relation to correlation is
concave (`k(a)`: 0.5 → 0.71) and whose mid-range also flattens the probabilities, so a sweep is audible
but you cannot say what correlation it passes through, and part of what you hear is CONTRAST LOSS rather
than relationship change. AFTER the rework **the CV *is* ρ**: a ramp 0→1 is a linear ramp of correlation
from independent to unison, with the MARGINALS UNTOUCHED — every voice keeps its own character and only
the RELATIONSHIPS move.

Consequences:
- The correlated-spread-modulator patch (SPREAD_TARGET_MODES.md) becomes a **correlation FIELD**: a CA
  pair's poly out into the spread CV sets each voice's ρ via a voice-permuted signal — the structure
  setting its own adherence, in honest units.
- The three timescales stay cleanly separated, which is what linear mixing was blurring: the pin map
  STEPS at phrase boundaries (who), ρ GLIDES continuously (how much), marginals NEVER move (what each
  voice is like). Changing "how much" no longer also changes "what each voice is like".
- ρ is notatable/recallable: "melody ρ 0.3 → 0.9 over eight bars" is a real instruction, not a knob
  gesture.

## The payoff: spread MODULATION becomes literal time-varying correlation (Rodney)
Correctness is not the only reason to do this. Today a CV sweep of spread moves a knob whose relation to
correlation is CONCAVE (`k(a)`: 0.5 → 0.71) and which also changes CONTRAST on the way (variance dips to
half at mid-spread) — so part of what you hear is the marginals flattening, not the relationship moving,
and you cannot say what correlation the sweep passed through. After the rework **the CV *is* ρ**: a ramp
0 → 1 is a linear ramp of correlation from independent to unison, marginals untouched throughout, so
every voice keeps its own character and ONLY the relationships move.

This sharpens the correlated-spread-modulator patch (SPREAD_TARGET_MODES.md): a CA pair's poly OUT into
the spread CV now sends a **correlation field** — each voice's ρ set by a voice-permuted signal, in
honest units.

And it keeps the three timescales cleanly separated, which is what makes the structure legible:
- pin map **steps** at phrase boundaries — WHO is related
- ρ **glides** continuously under CV — HOW MUCH
- marginals **never move** — WHAT EACH VOICE IS LIKE
Linear mixing blurred these: changing "how much" also changed "what each voice is like".

Bonus: with ρ as the parameter a sweep is recallable and notatable — "melody ρ 0.3 → 0.9 over eight bars"
is an instruction, not a knob gesture. Time-varying correlation is then simply whatever you patch into
the CV: e.g. a ramp across two phrases takes the ensemble from independent to unison over that span.

## Dependence parameters: rho per LANE, alpha per STREAM — and no term structure
- **Cross-voice correlation (rho) is PER LANE** — lanes are what you shape per voice (5 of them).
- **Temporal correlation (alpha) is PER STREAM — R / M / Q (Rodney)**, not per lane: slew is a
  per-stream control, and temporal dependence belongs with the DRAW SOURCE. Its time base is the
  PHRASE (per step position), not the step. Lanes inherit their
  stream's alpha (rhythm family lanes share one, melody's share another, q-mix its own). Three alphas.
- **ONE alpha is enough — deliberately NOT a correlation term structure.** A term structure (rho varying
  with LAG, e.g. AR(p)) would add one thing a single pole cannot: a RESURGENCE at a chosen lag —
  decorrelate fast, then re-cohere at lag 16. Not built, because (a) the musically useful axis is
  correlation varying with MUSICAL TIME, which we already get by modulating rho (and alpha) on a synced
  clock, and (b) phrase-scale recurrence is already expressible EXACTLY via the seed machinery
  (reset-as-loop, counter offset) rather than statistically — and "slightly more likely to resemble the
  step 16 ago" is far weaker perceptually than actually repeating the phrase.
  Recorded as a DECISION, not an omission, so nobody adds an AR(p) assuming it was never considered.

## Do it on a branch
Changes the generated distribution, so it needs LISTENING, not just reasoning. Land the Sands kit
migration and the q-mix/CA work first. Regression: verify marginals are uniform (histogram the draws per
lane) and that measured pairwise correlation matches the ρ knob.
