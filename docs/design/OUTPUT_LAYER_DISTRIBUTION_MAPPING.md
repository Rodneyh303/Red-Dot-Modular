# Output-layer probability transformation (marginal reshaping via F^-1)
A capability GIFTED by the end-to-end uniformity guarantee — the invariant defended through the whole
slew/mix/scrub correctness war (OU bridge, variance preservation, r=0/r<0). Not just "keep it correct":
preserving Uniform(0,1) end-to-end makes the OUTPUT a clean canvas for arbitrary marginal shaping.

## The principle
Uniform(0,1) is the copula substrate (Sklar: marginals -> CDF -> uniform, couple the uniforms, map back).
Our output is provably Uniform(0,1). So at the OUTPUT, any target marginal F is reachable by its inverse
CDF:  out = F^-1(U).  And it is:
- CORRELATION-PRESERVING: the copula couples the uniforms; a MONOTONE F^-1 per lane keeps the dependence
  intact (Sklar — copula invariant under monotone marginal transforms). Slew/spread/CA structure untouched.
- REVERSIBILITY-PRESERVING: the underlying uniform stays Philox-keyed/position-addressable; F^-1 is a
  STATELESS PURE FUNCTION on top. Scrub/reverse/offset still work (reverse the uniform, re-apply F^-1).
- CHEAP + SWAPPABLE: a transfer curve / LUT at output; change the output distribution live, nothing upstream
  disturbed.

## This RESOLVES the "user probability" question
- Don't let users replace the UPSTREAM copula model (it's a better, correlated, reversible probability model
  than hand-wired — substituting would downgrade it). HARD + wrong to accept as input.
- DO let users shape the DOWNSTREAM output MARGINAL via a transfer curve / F^-1. SAFE + principled: post-
  copula, monotone, stateless -> correlation + reversibility intact.
So: user brings entropy + craft + reaction (random gates Mode C, random pitch to quantiser) AND can shape
the output distribution; the instrument owns the correlational MODEL.

## Scope (Rodney's initial thinking — keep it tight)
- Distributions that are QUICK to compute and defined on FINITE [0,1] (so F^-1 is cheap, bounded, no tails
  to clip): e.g. Beta-ish shapes, triangular, raised-cosine, power (U^k), logistic-on-[0,1], piecewise/
  user curve. No heavy/unbounded ones.
- PER-LANE only. NO parameter modulation (keep it a set-and-forget shaping curve per lane, not a modulated
  surface — avoids the modulation-cost + reversibility questions).
- Delivery: an output-layer MAPPING, e.g. a SANDS EXTENSION (expander) that carries the per-lane F^-1 curves.

## UI thoughts (initial)
- A mapping VISUALISER (show the F^-1 curve per lane), and/or
- a SANDS option to display PRE- or POST-transform bars (toggle: see the raw uniform marginal vs the shaped
  output marginal). This is the truthful-display principle again — let the user SEE what the transform does,
  pre vs post.

## Why it's a nice (FUN, low-risk) direction
- Architecturally free (the uniformity guarantee already holds; F^-1 is a pure output function).
- Correlation + reversibility safe (monotone, stateless).
- Tight scope (per-lane, no mod, finite [0,1] distributions) = bounded, buildable.
- Connects to the forum "additive randomness / shape which values are chosen" conversation — done the
  principled way (output marginal via F^-1 on a correlated uniform), not by summing distributions.
- Visual + musical payoff (reshape register/value emphasis per lane while keeping the groove's correlation).

---

## Distribution palette — per-lane F^-1, cost & form (Rodney)
No-modulation + per-lane means: compute each lane's F^-1 ONCE at SET-TIME, then runtime = LUT lookup +
interp. So all per-distribution complexity lives at set-time; runtime is ONE uniform cheap path regardless.

### Closed-form F^-1 — use directly (no numerical inversion, LUT optional)
- **Triangular** — F^-1 piecewise, two sqrt branches around the mode. Exact, trivial. 1 param (mode). The
  reference case.
- **Power** — F^-1(u) = u^(1/k) (or 1-(1-u)^(1/k) mirror). One pow. 1 param. Skew toward 0/1. "Emphasis".
- **Logit-normal** — F^-1(u) = logistic(mu + sigma*PhiInv(u)). **Cheaper FOR US than anyone** because we
  already have PhiInv (copula cornerstone). 2 params (mu,sigma). Unimodal/skewable. Clamp u off {0,1}
  (open support). STANDOUT pick — reuses existing engine math.
- **Kumaraswamy** — F^-1(u) = (1 - (1-u)^(1/b))^(1/a). TWO pows, closed form. ~Beta-like shape flexibility
  (unimodal/skew/bathtub) with NO incomplete-beta, NO inversion, NO LUT needed. **Preferred Beta-family
  shaper for musical use** — gives the shapes you want without Beta's machinery.
- **User draw curve** — the monotone curve the user draws IS F^-1 directly (enforce monotonicity). Pure LUT,
  cheapest runtime, most expressive/fun. Work is only the UI (draw + monotonicity + interp).

### Needs numerical inversion — SET-TIME LUT only
- **True Beta(alpha,beta)** — F^-1 = inverse regularized incomplete beta: NO closed form, needs a root-find
  (Newton/Halley on I_x(a,b) - u). Viable ONLY because no-modulation -> invert ONCE per lane at set-time,
  build the LUT, lookup forever. Runtime does ZERO Beta math.
  - LOG-SPACE BETA: helps the SET-TIME LUT-BUILD ROBUSTNESS, not runtime. Evaluate the forward I_x(a,b)
    inside the inverter in log-space (lgamma-based: exp(lgamma(a)+lgamma(b)-lgamma(a+b)) etc.) so the LUT is
    accurate for EXTREME/skewed a,b (e.g. Beta(0.3,8)) that would otherwise over/underflow. It does NOT
    remove the inversion iteration or help the hot path (LUT lookup).
  - RECOMMENDATION: prefer KUMARASWAMY for the Beta-ish shapes (closed form, no LUT). Keep true Beta ONLY if
    its exact statistical properties are actually needed (for musical distribution-shaping: almost never).

### Unifying implementation
Every distribution -> a per-lane F^-1 LUT filled at SET-TIME (closed-form eval, or numerical inversion for
true Beta, or the drawn curve). Runtime = LUT lookup + interpolation, IDENTICAL cost for all. The
no-modulation constraint is what buys this: all per-distribution cost is at set-time, runtime is one cheap
uniform path. (Closed-form ones -- triangular/power/logit-normal/Kumaraswamy -- could even skip the LUT and
eval live if preferred; LUT is the uniform option.)
