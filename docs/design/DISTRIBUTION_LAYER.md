# Distribution layer — the probability abstraction (separate from the .dmcurve EDITOR)
Related to but SEPARATE from docs/design/DMCURVE_EDITOR.md: that doc is the UI for EDITING a drawn curve;
THIS doc is the probability-math CORE. Distribution is the general concern; a drawn/PCHIP curve is just ONE
kind of distribution it can take.

## Factoring (Rodney) — Distribution is the core, PCHIP is one of its types
- **Distribution** = the general probability object. Interface: cdf(x), pdf(x), invCDF(u) on [0,1].
  Everything downstream (lane F^-1 application, PDF/CDF display, the effective-distribution visualiser)
  talks to Distribution and NEVER cares which concrete type.
- Concrete types implementing Distribution:
  - Closed-form parametric: Triangular(mode), Power(k), Kumaraswamy(a,b), LogitNormal(mu,sigma).
  - **PchipDistribution** — wraps a MonotoneCurve (the DRAWN case): cdf=curve.evalCDF, invCDF=curve.evalInverse,
    pdf=curve.pdf. Just another concrete Distribution — the drawn curve is NOT special, it's one option.
- **MonotoneCurve** = a lower-level UTILITY (monotone PCHIP interpolation + control-point editing). Used BY
  PchipDistribution and manipulated BY the .dmcurve editor widget. It is the editable GEOMETRIC object; the
  Distribution is its PROBABILISTIC view. (Matches the earlier curve-vs-distribution distinction: curve =
  shape, distribution = the probabilistic object that MAY be a curve.)
- Parametric types have closed-form invCDF -> no reflect/re-fit needed; they expose the SAME invCDF(u)
  interface, so the lane's F^-1 application is identical regardless of type: dist.invCDF(u). Clean.

## Closed-form math (the pure functions to write + test; all on [0,1])
TRIANGULAR (mode c):
  cdf:  x<c ? x^2/c : 1-(1-x)^2/(1-c)
  pdf:  x<c ? 2x/c  : 2(1-x)/(1-c)
  inv:  u<c ? sqrt(u*c) : 1-sqrt((1-u)(1-c))
POWER (k>0):
  cdf: x^k   pdf: k*x^(k-1)   inv: u^(1/k)
KUMARASWAMY (a,b>0):
  cdf: 1-(1-x^a)^b   pdf: a*b*x^(a-1)*(1-x^a)^(b-1)   inv: (1-(1-u)^(1/b))^(1/a)
LOGIT-NORMAL (mu, sigma>0) — REUSE the engine's PhiInv (normal quantile) + Phi (normal CDF) + phi (normal PDF):
  logit(x)=ln(x/(1-x)); logistic(z)=1/(1+e^-z)
  cdf: Phi((logit(x)-mu)/sigma)
  pdf: (1/(sigma*x*(1-x))) * phi((logit(x)-mu)/sigma)
  inv: logistic(mu + sigma*PhiInv(u))
  Clamp x,u off exact {0,1} (open support).

## Contract-test battery — run against ALL distributions (the pattern that saved the session)
One battery, every Distribution (4 parametric + PchipDistribution) must pass:
- invCDF(cdf(x)) ~= x  AND  cdf(invCDF(u)) ~= u  (round-trip, tol ~1e-4).
- pdf(x) ~= numerical d/dx cdf(x) (central difference).
- cdf(0)~=0, cdf(1)~=1 (or open-support limits for logit-normal).
- cdf monotone non-decreasing over a fine x-sweep (no overshoot — critical for PchipDistribution:
  Fritsch-Carlson guarantees it, TEST it).
- DEGENERATE-TO-KNOWN (catches formula typos cheaply): Kumaraswamy(1,1)=uniform (cdf(x)=x); Power(1)=uniform;
  triangular at known mode/points; logit-normal invCDF(0.5)=logistic(mu) (median).
Plus PchipDistribution runs the MonotoneCurve edit tests (see DMCURVE_EDITOR.md Phase 1).

## JSON (shared with .dmcurve + patch state; see DMCURVE_EDITOR.md for schema)
type field selects the concrete Distribution; params{} holds parametric params; points[] for the drawn
(Pchip) case. toJson/fromJson dispatch on type. One schema, used by .dmcurve file AND module dataToJson.

## Build = the TRUE Phase 1 (pure, do-first, de-risks everything)
Distribution module (interface + 4 parametric + PchipDistribution) + MonotoneCurve utility + the contract-
test battery. All pure (no VCV/UI). The .dmcurve EDITOR (DMCURVE_EDITOR.md Phase 2/3: widget, popup, live
preview) builds ON TOP of this already-complete distribution layer.

---

## Palette additions + selection principle (Rodney)
### SELECTION PRINCIPLE — add a parametric distribution only if BOTH:
(a) it gives a DISTINCT, nameable MUSICAL SHAPE the others don't, AND
(b) it has a CHEAP inverse-CDF (closed-form, or PhiInv-based) — so the NO-LUT / direct-eval architecture holds.
The drawn PchipDistribution is the ESCAPE VALVE for anything exotic, so the parametric list stays small +
curated, not exhaustive.

### ADD — Arcsine (bathtub / extremes; cheap)
- Shape: U-shaped — mass at BOTH extremes (0 and 1), sparse middle. The clean "favour the extremes" shape
  the others don't give cleanly (triangular=single-peak, power=monotone, logit-normal=single-peak;
  Kumaraswamy can bathtub but needs a<1,b<1 fiddly).
- Closed-form: cdf = (2/pi)*asin(sqrt(x));  pdf = 1/(pi*sqrt(x*(1-x)));  inv = sin^2(pi*u/2). One sin/sqrt.
- Musical: favour bottom AND top of range / rest-density either sparse or dense, rarely medium.

### ADD — Truncated-normal (bounded bell; reuses PhiInv)
- Shape: normal clipped to [0,1] (params mean mu, sd sigma). Familiar bell, controllable spread; UNLIKE
  logit-normal it has NON-ZERO density AT the edges (clipped, not squashed) -> can pile mass at an edge.
  Worth having alongside logit-normal (closest pair; different edge behaviour).
- Closed-form via Phi/PhiInv (already in engine): with standardized bounds a=(0-mu)/sigma, b=(1-mu)/sigma,
  inv(u) = clamp( mu + sigma*PhiInv( Phi(a) + u*(Phi(b)-Phi(a)) ), 0, 1 );
  cdf(x) = (Phi((x-mu)/sigma)-Phi(a))/(Phi(b)-Phi(a));  pdf(x) = phi((x-mu)/sigma)/(sigma*(Phi(b)-Phi(a))).
  Same cost class as logit-normal.

### SKIP — Bates (considered, rejected)
- Bates = mean of n uniforms (n=1 uniform, n->inf normal). Shape = center-concentrated bump, tunable by n.
- FAILS (b): NO closed-form inverse-CDF (Irwin-Hall piecewise-poly CDF -> needs root-find or LUT) ->
  would reintroduce the set-time-LUT path we deliberately CUT (we do direct eval, no LUT).
- FAILS (a) too: redundant with Truncated-normal for the "concentrate in the middle" effect, which
  truncated-normal gives cheaply + more controllably (via sigma). So skip.

### v1 palette (shape-space coverage, ALL cheap inverses, NO LUT)
- Monotone skew: Power.  Single-peak interior: Triangular (linear), Truncated-normal (bell, edge-mass),
  Logit-normal (squashed bell, zero edges).  Bathtub/extremes: Arcsine (clean), Kumaraswamy (flexible).
  Catch-all flexible: Kumaraswamy.  Anything else: drawn PchipDistribution.

### Future-exotic reference (NOT v1)
Kotz & van Dorp, "Beyond Beta: Other Continuous Families of Distributions with Bounded Support" — the
authoritative catalog of [0,1] distributions. If a user ever requests an exotic shape: van Dorp's
Two-Sided Power / generalized-trapezoidal families (flat-top / trapezoidal-with-skew) are the place to look.
Meanwhile the drawn curve covers it. Book = reference, not a v1 need.
