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

## Scope (FINAL — Rodney)
- Distributions QUICK to compute, FINITE [0,1] (F^-1 cheap, bounded, no tails). See palette below.
- NO parameter modulation (set-and-forget curve — avoids modulation-cost + reversibility questions).
- SCOPE = the instrument's existing two-tier MACRO <-> SANDS delegate/override pattern (same as every other
  per-voice param): the F^-1 curve is **per-lane, GLOBAL across voices by default (set via MACRO)**, with
  **optional PER-VOICE UNLOCK to a different curve via SANDS**. Default = ONE curve per lane; per-voice is
  OPT-IN only when a voice wants distinct character. NOT per-slot, NOT per-collection, NOT fixed per-voice.
- Collection-character EMERGES: set global-per-lane on Macro, selectively unlock voices in Sands; whatever
  collections the arrangement (voice->slot->fan-out->arrange) forms inherit their voices' curves. No
  collection-level control needed — it composes up from Macro-global + selective-Sands-unlock.
- Reuses existing machinery + mental model (delegate-global-or-override-local); zero new conceptual surface.
- Delivery: the curve lives on MACRO (global per lane) and SANDS (per-voice when unlocked), where those
  controls already are.

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

---

## USER-DRAWN CURVE — how to surface it (Rodney: think on this)
The user-draw F^-1 is the most expressive + fun shaper, but it needs a curve-DRAWING UI. The curve IS F^-1
(monotone [0,1]->[0,1]); the only runtime cost is a LUT lookup, so ALL the work is the editor.

### Reference / prior art
- MindMeld ShapeMaster is the gold standard for curve drawing in VCV (node-based, draggable handles,
  tension/curve-type per segment, snap). BUT closed-source — can't lift code, only study the UX.
- So: either (a) build a minimal bespoke monotone-curve editor, or (b) pull in a 3rd-party OPEN-SOURCE curve
  lib.

### Requirements (narrower than ShapeMaster — this is NOT a full envelope editor)
- Domain/range both [0,1]; MONOTONE NON-DECREASING enforced (it's an inverse-CDF — must be monotone or the
  "distribution" is ill-defined). This constraint SIMPLIFIES the editor vs ShapeMaster (no arbitrary shapes).
- A handful of draggable control points + interpolation (monotone cubic / PCHIP keeps it monotone +
  smooth; or monotone Catmull-Rom; linear is the trivial floor).
- Endpoints pinned (0,0) and (1,1) OR free (a non-(0,0)/(1,1) curve = truncated/shifted support — decide:
  pin for a true CDF, or allow free for creative "dead zones").
- Output: a LUT (e.g. 256/1024 samples) sampled from the curve at set-time. Runtime = lookup+interp.
- Fits on a Sands/Macro panel region (small), with the pre/post-transform bar display alongside.

### Open-source options to evaluate (3rd-party)
- MONOTONE INTERPOLATION (the math, not the UI): PCHIP / Fritsch-Carlson monotone cubic — tiny, public-
  domain algorithms, trivial to vendor (no lib needed, ~30 lines). This is likely all the "lib" required —
  the interpolation is the only non-trivial math, and monotone-cubic is a known closed-form.
- CURVE-EDITOR UI: VCV itself has no shared curve widget; most plugins roll their own. Check permissively-
  licensed VCV plugins with curve editors (MIT/BSD/GPL-compat with our license) to study/adapt — but the
  WIDGET is mostly nanovg drawing + hit-testing on control points, which is bespoke-cheap to write.
- Decision lean: VENDOR the monotone-cubic interpolation (~30 lines, public domain) + write a MINIMAL
  bespoke nanovg curve widget (draggable monotone-constrained points). A full 3rd-party curve-editor lib is
  probably heavier than needed for a monotone [0,1] inverse-CDF. Study ShapeMaster's UX, don't need its code.

### Build order (when this lands)
1. Closed-form distributions FIRST (triangular/power/logit-normal/Kumaraswamy) — no UI beyond param knobs +
   the visualiser. Gets the whole F^-1-LUT pipeline + pre/post display working.
2. THEN the user-draw curve editor on top (the pipeline already exists; only the editor is new).

---

## .dmcurve — savable distribution-curve asset (precedent: Bitwig BWCURVE)
WHY: Bitwig ships curves as first-class, savable, reusable, cross-device assets (BWCURVE). Their Keytrack+
("curve maps incoming note pitch to a value") and Transfer ("curve-based waveshaping on pitch") are EXACTLY
our use case — a transfer function / F^-1 on a value. If a respected DAW makes this a core savable asset,
.dmcurve is justified outright.

## Format decisions
- STORE THE DEFINITION, not samples: parametric -> {type, params} (kumaraswamy a,b / logit-normal mu,sigma /
  power k / triangular mode); drawn -> {control points (<=~32), interp=pchip}; + endpoint convention
  (pinned (0,0)-(1,1) | free). Re-editable on load, exact, compact. (Same principle as .dmtune: store the
  definition, not the rendered output.)
- ONE unified format, a `type:` field distinguishes parametric vs drawn (user shouldn't care which — it's
  just "a distribution shape I saved").
- VERSIONED (v1 header) — format changes never break old files (learned the hard way: silent convention
  drift is the enemy).
- TEXT-BASED (Scala .scl style) — human-readable, diffable, shareable, future-proof.
- VALIDATE ON LOAD: enforce monotonicity (reject/clamp non-monotone drawn files — a non-monotone F^-1 is
  ill-defined), clamp params to valid ranges.
- SCOPE: one .dmcurve = one lane's F^-1. Loads to Macro (global per lane) or Sands (per-voice when unlocked).
  (Later: a "curve set" = all lanes, like a bank.)

## THE DIFFERENTIATOR — open + documented (where BWCURVE is closed)
BWCURVE is "at present undocumented" and the SOS reviewer explicitly wished for "third-party curve
libraries, or tools to generate new sets of curves algorithmically." So: make .dmcurve an OPEN, DOCUMENTED,
text format — enabling exactly what the Bitwig community wants but can't have: shareable third-party curve
libraries + algorithmic curve generators. The Scala/.scl playbook (open text -> ecosystem) applied to
distribution curves. Open format = our edge over the closed precedent.

## Authorable-asset FAMILY (the positioning)
.dmtune (tunings — Sikit/Colonnade/Duo author) + .dmcurve (distribution shapes — the output mapping
authors): both text, versioned, save/load/share, "define the thing not the rendered output," both feeding
the copula engine. dot.modular isn't just a synth — it's an AUTHORABLE correlational-randomness ecosystem
with an open file-format family for the SHAPING layers (pitch mapping + distribution mapping). Eventually: a
named library + browser for both (as Bitwig 6 unifies curves + automation clips in one browser).

---

## OPTION (if not too distracting) — also ship as a STANDALONE probability-reshaper module
The reshaper core is a PURE, context-free function: out = F^-1(in) + the curve/distribution UI + .dmcurve.
It doesn't care if `in` is Monsoon's internal uniform or an external CV. So the same core can be exposed
TWO ways — build the core ONCE, wrap twice:
1. Monsoon/Sands EXPANDER — internal per-lane uniform, Macro-global/Sands-unlock scope (the primary purpose).
2. STANDALONE VCV module — CV in -> F^-1 -> CV out. A general "probability/distribution reshaper" for VCV.

### Why it's worth considering (but OPTIONAL — don't let it distract from Monsoon)
- Almost free: shared pure core + a thin wrapper (just a CV in/out jack). The F^-1 engine, curve editor,
  .dmcurve are built regardless for the expander.
- Fills a real VCV gap: the forum thread (Morris, "additive randomness") shows the community wants
  distribution-shaping beyond subtractive filtering; a drop-after-any-random-source reshaper isn't
  well-served. Ships what they're asking for.
- GATEWAY to the ecosystem: a free/cheap standalone reshaper -> users meet .dmcurve -> discover tuning
  ecosystem / Monsoon / the copula engine. Low-commitment entry point to the deep instrument.
- DE-RISKS the integrated version: a standalone in->F^-1->out module has NONE of the voice-index/convention
  minefield (the thing that just cost a brutal day). Trivial, conventionally-safe, easy to test -> proves
  the F^-1 + curve + .dmcurve machinery in isolation BEFORE wiring into Monsoon. A palette-cleanser build.

### The one design choice for the standalone (input isn't guaranteed uniform)
F^-1 assumes Uniform(0,1) in (guaranteed inside Monsoon; NOT for arbitrary external CV):
- (a) ASSUME-UNIFORM: "feed me uniform randomness, I reshape its distribution." Honest, simple, = exactly
  what Monsoon does internally. The quick version.
- (b) EMPIRICAL-CDF NORMALIZE then F^-1: measure the input's running distribution (online histogram/CDF),
  map to uniform, then F^-1 -> "transform ANY signal's distribution into ANY target distribution"
  (distribution transfer). More powerful + distinctive as a standalone product, but needs the online CDF
  estimator (more work). The standout general tool if pursued.

### Verdict
Note as an OPTION. Primary focus = the Monsoon expander (serves the instrument). IF the standalone falls out
cheaply (it mostly does — shared core), it's high-leverage (gateway + fills a gap + de-risks). Do (a) first
if at all; (b) only if the "any->any distribution transfer" tool is worth the online-CDF work. Do NOT let it
pull focus from the instrument.

---

## REFINED file/state model — .dmcurve is a FILE; the distribution is PATCH STATE (no .dmdist)
Separated the two concepts (Rodney), then realised only ONE needs a file:
- A **CURVE** = a monotone shape (points + interp) — a CRAFTED, reusable, shareable ASSET. Worth a file.
- A **DISTRIBUTION** = which dist a lane uses (type + params, OR "drawn") — just a SETTING. Not an asset.

### Decision: ONE format (.dmcurve); distribution rides in the patch
- **.dmcurve = the ONE file format** — a general-purpose monotone CURVE (points + interp, endpoint
  convention). Reusable ANYWHERE a curve is wanted (F^-1/transfer/mapping), not locked to "distribution".
  The BWCURVE analogue; OPEN + documented + text + versioned (see .dmcurve section above). Save/load/library/
  share.
- **NO .dmdist file.** The distribution SELECTION (parametric {type, params}, or "drawn") is LANE/PATCH
  STATE — it serializes with the patch like slew/K/any parameter. Nothing to save separately.
- A DRAWN lane EMBEDS its curve points in the patch (self-contained — patch never breaks), and can
  OPTIONALLY save/load a .dmcurve for library/reuse. The .dmcurve is the SHARING/LIBRARY copy; the patch
  always carries its own inline copy. No fragile cross-references.

### Why (the principle)
Files are for CRAFTED, REUSABLE ASSETS (tunings .dmtune, curves .dmcurve). Patch state is for SETTINGS
(which distribution + params). A parametric dist choice is a setting; a hand-drawn curve is an asset — only
the asset earns a format. Result: ONE format not two, no .dmdist library/browser/reference-resolution, no
cross-reference fragility, and .dmcurve stays general-purpose. Authorable-asset family = .dmtune + .dmcurve
(two files); distributions live in patches.

---

## UI — EDIT THE CDF DIRECTLY (not F^-1); show PDF/CDF/F^-1 (Rodney)
Edit surface = the CDF F(x), because it's the object the user MEANS and the constraints are cleanest.
- EDIT: CDF F(x) on [0,1], pinned F(0)=0, F(1)=1, STRICTLY INCREASING with minimum slope epsilon ~1e-3.
  The user draws "what fraction of outputs fall below x" — a comprehensible statement.
  *** 2026-10-18: TIGHTENED from "monotone non-decreasing". A FLAT segment is an ATOM, and atoms break
  Sklar UNIQUENESS (the copula is then pinned only on Ran F, which has gaps), destroy rank information,
  drag Spearman/Kendall AGAINST the Spread knob's calibration, and produce stuck unisons (the atom returns
  the IDENTICAL value every time). Strict increase is what rank preservation actually needs; everything
  else a user draws (steep/shallow/S/asymmetric/lumpy) is unaffected — this excludes one degenerate corner,
  not the drawn-curve feature. ENFORCE at control-point level: y_{k+1} - y_k >= epsilon*(x_{k+1} - x_k),
  with the drag handler CLAMPING so a flat drag meets RESISTANCE (same feel as the existing monotonicity
  clamp). PCHIP cooperates: strictly increasing control points + Fritsch-Carlson limiting => strictly
  increasing interpolant. epsilon ~1e-3 also guarantees float storage of warped_ cannot collapse a segment
  to ties. FOLLOW-ONS: (a) assert STRICTLY increasing in the float-store property test, not non-decreasing;
  (b) audit the palette for parameter values giving near-flat regions (logit-sigmoid heavy tails is the
  likely candidate) or the invariant leaks through the palette; (c) MonotoneCurve::fromDistribution must
  LAND on strictly increasing control points — assert, don't assume. ***
- PDF = the CDF's SLOPE (DIRECT derivative f=F', not reciprocal). Shown LIVE as they draw: STEEP = high
  density ("lots of values here"), FLAT = low density. Best drawing feedback — you shape density directly.
- F^-1 (the RUNTIME object, applied to the uniform stream) = the drawn CDF INVERTED. Derived at edit-commit:
  reflect the CDF's control points across the diagonal (swap x<->y — still monotone, still valid points) +
  re-fit PCHIP on the swapped points. -> F^-1 stays a DIRECT-EVAL PCHIP (no LUT, no runtime root-find). The
  inversion is "swap coords, re-fit", done once per edit.
- Three views, all from the one object: CDF (edit), PDF (slope, live density feedback), F^-1 (derived, the
  runtime transform). Clamp the PDF plot for near-vertical CDF segments (slope->inf = near-delta spike) —
  display clamp only, correct behaviour.

### Closed-form distributions in the SAME CDF-centric view
Named dists (triangular/power/logit-normal/Kumaraswamy) have closed-form CDF + PDF + F^-1 — show all three
in the same view, so DRAWN and NAMED are comparable/interchangeable (draw a CDF -> see its PDF + F^-1; pick
Kumaraswamy -> see its CDF curve). Overlay to match a drawn curve to a named family, or start named + hand-
tweak.

### Composes with the pre/post output histogram (truthful display)
The realized output MARGINAL histogram = the empirical PDF; show it AGAINST the target PDF — "distribution
you asked for" vs "what the output produced". The truthful-display principle (show actual vs intended).

## WHY THIS ALL WORKS — the uniformity-preserving modulation was the point (Rodney)
This entire feature — editing a CDF, deriving F^-1, mapping the output to any target distribution, with
correlation + reversibility intact — RESTS on the output being guaranteed Uniform(0,1). That guarantee is
EXACTLY what the whole slew/mix/scrub correctness war defended: uniformity-PRESERVING modulation (OU bridge,
variance preservation, r=0/r<0, the AB-mix copula blend). We insisted on uniform-preserving modulation for
correctness; the PAYOFF is that the output is a clean uniform substrate onto which ANY CDF maps cleanly.
Had modulation distorted the marginal, F^-1(output) would be garbage. The hard invariant we protected is the
foundation this feature stands on. (Nice: the thing that made the correctness work non-negotiable is the
same thing that makes this capability possible.)

---

## PITCH-LANE semantics — weights vs F^-1 degree-tilt (Rodney; code-checked)
Generative pitch (genPitchLive): the melody draw r_semi -> pickSemitone(semiWeights, N, r_semi), a
correct cumulative-weight roulette: note i picked with probability weights[i]/sum, ORDER-INDEPENDENT for
probability (pickSemitone is sensible as-is — picking depends on WEIGHTS only). Notes are laid out in
DEGREE (pitch) order along the [0,1] uniform axis.

### The two orthogonal controls (confirmed)
- **UNIFORM distribution (flat r_semi, today's behaviour):** the pick depends on WEIGHTS ONLY. Degree order
  is INERT — invisible, because each note's slice is hit proportional to its weight regardless of position.
- **F^-1 NOT uniform (the output-mapping feature):** brings in DEGREE on top of weights. Because notes are
  pitch-ordered along [0,1], a skewed F^-1 tilts WHICH REGION of the axis is favoured -> a register/degree
  tilt layered on the weights. Skew low -> favour low-DEGREE (low-pitch-in-octave) notes; skew high ->
  high; mid-heavy -> middle; bimodal -> extremes. Each note's share = its weight x the local density of the
  F^-1 tilt.
So: WEIGHTS = which notes + base probability (uniform=flat, skewed=per-note emphasis; arbitrary per-note).
F^-1 = DEGREE/register tilt along the pitch-ordered axis. Independent axes.

### Four combinations
- uniform weights + flat F^-1      -> flat (every note equally likely)
- uniform weights + skewed F^-1    -> all notes available, mass register-tilted (PURE register tilt — F^-1
                                      does all the shaping)
- skewed weights + flat F^-1       -> specific notes emphasised, no register tilt (today)
- skewed weights + skewed F^-1     -> emphasised notes AND a register tilt over them

### Notes for building
- pickSemitone is correct — do NOT "fix" it; position-order is inert under uniform, becomes the register
  HANDLE only under F^-1. (If a user wants arbitrary per-note emphasis regardless of pitch -> that's the
  WEIGHTS, not F^-1. F^-1 is inherently a pitch-ordered/register tilt.)
- OCTAVE lane is currently UNIFORM (oct = oL + floor(r_oct*range)) -> F^-1 on r_oct is a PURE NEW register
  (octave) tilt, net-new control (octave was flat).
- Combined r_semi + r_oct F^-1 = independent within-octave-degree tilt + octave tilt -> full register
  sculpting, within the scale weights, copula-correlation preserved (monotone per-lane).
- Pre/post display should show the REALIZED note distribution (semiWeights composed with F^-1), not just
  F^-1 — so the user sees the combined effect.

---

## F^-1 vs TRUE TAIL DEPENDENCE — the honest line (Rodney)
What per-voice F^-1 gives: voices sharing a correlation CLUSTER or DISPERSE in OUTPUT depending on whether
their MARGINAL response curves are similar or different — a MARGINAL-level effect. The copula (dependence
structure) is UNCHANGED (Sklar: monotone marginal transforms leave the copula invariant); F^-1 reshapes
WHERE in value-space the fixed Gaussian dependence lands per voice.
What it does NOT give: TRUE TAIL DEPENDENCE — a COPULA property (genuine extreme co-movement: t/Gumbel/
Clayton). Gaussian copula has ZERO tail dependence; no marginal transform can manufacture it (tail dep is
invariant under marginal transforms). THE LINE = MARGINALS (what F^-1 shapes) vs COPULA (what it can't
touch).
Design position: keep the GAUSSIAN copula (gifts: PhiInv, reversibility, uniformity, OU-bridge, tractability)
+ output F^-1 shaping -> the musical FLAVOUR of range-varying correlation-expression (clustering/dispersion),
WITHOUT real tail dependence. True tail dep would need a different copula family (costs: loses the gifts).
Honest: F^-1 MIMICS, via marginals, some of what copula-family-choice gives; it is not the same thing.
*** The above is exactly right for a FIXED warp. It UNDERSTATES what a MODULATED warp does — see the next
section. (2026-10-18) ***

## MODULATED WARP IS A REAL COPULA CHANGE (2026-10-18 — amends the section above)
If theta (or the depth w) is MODULATED BY A SOURCE SHARED ACROSS VOICES, the statement "the copula is
unchanged" stops holding UNCONDITIONALLY. Conditional on theta the copula is still C; unconditionally you
have a FRAILTY MIXTURE over a common factor, and the copula of a mixture is not the mixture of copulas:
    H(x_1..x_n) = INTEGRAL C( F_theta(x_1), ..., F_theta(x_n) ) dP(theta)
This is a ONE-FACTOR COPULA (Krupskii & Joe 2013) with conditionally GAUSSIAN coordinates. P = the law of
the LATCHED theta at draw times (a sine LFO gives arcsine on [-A,A]; a triangle gives uniform; a clock-synced
LFO is discrete and non-iid). Consequences:
- At r = 0 with PowerDistribution (F_theta = x^theta, multiplicative in cumulative hazard) it is literally
  ARCHIMEDEAN, generator phi = M_P^-1 (inverse MGF of the modulation source's amplitude law). So the SPREAD
  KNOB MOVES BETWEEN DEPENDENCE CLASSES, not just correlation values. Palette members are NOT interchangeable
  here — only Power has the hazard-multiplicative form giving a clean generator.
- Law of total covariance: Cov = E[Cov|theta] + Cov(mu_i(theta), mu_j(theta)). With HOMOGENEOUS warps the
  second term is Var(mu(theta)) >= 0, so shared modulation can only ADD positive dependence (PQD).
  NEGATIVE common-factor dependence needs HETEROGENEOUS/OPPOSED warps (Sands unlock) — unreachable by any rho.
- w gates theta MULTIPLICATIVELY (at w=0 the factor vanishes), so shared w is a dependence-STRENGTH control:
  a non-stationary copula with rho untouched. SYMMETRIC warps (m(theta)=1/2) give a DISPERSION factor with
  NO mean co-movement — bounded stochastic volatility; Spearman sees ~nothing, quadrant measures see it.
- At rho = 1 the copula is M (Frechet bound) for ANY w and F — per-voice warp gives PARALLEL VOICING, and
  shared modulation with heterogeneous warps pushes STRICTLY BELOW M (min is concave): modulation DECORRELATES.
- lambda (tail dependence) is STILL ZERO: with Cbar(s,s) ~ s^kappa, kappa = 2/(1+r) > 1, we get
  lambda_U <= G_max^(kappa-1), and G_max -> 0 for any CONTINUOUS F_theta under any BOUNDED P. No modulation,
  truncation or affine rescaling rescues it. Only an ATOM does — and atoms are excluded by the strict-increase
  constraint (see EDIT section) because they break Sklar UNIQUENESS and give stuck unisons.
PITCH LANGUAGE: "common-factor dependence", "co-spiking at audible quantiles", "a copula with user-chosen
marginals". NOT "tail dependence" — a reader who knows the frailty literature will check it.
Full derivations: SESSION_LOG_2026-10-18.md sections 5-7.

### THE TAIL-DEPENDENCE DOOR HAS A KNOWN KEY — the common-shock / DMO construction (Rodney)
We kept finding lambda>0 unreachable: bounded factor + continuous marginals + Gaussian conditional coupling
all force lambda=0, and the only exits (atoms, unbounded mixing) each cost a gift. There IS a principled
construction that reaches genuine, CALIBRATABLE tail dependence, and it's Rodney's own published work:
the DYNAMIC MARSHALL-OLKIN (DMO) / COMMON-SHOCK copula — Albanese, Crepey, Hoskinson, Saadeddine, "XVA
Analysis From the Balance Sheet", Quantitative Finance 21(1), 2021 (arXiv 2009.00368); the common-shock
default model is Crepey-Bielecki-Brigo (2014, ch.8-10), Crepey-Song (2016), Elouerkhaoui (2007,2017).

How it works (credit → music translation): shocks are indexed by SUBSETS Y of names (voices); each shock
fires at an independent exponential time with intensity gamma_Y, and when it fires it hits EVERY name in Y
SIMULTANEOUSLY. A shock on {3,4} defaults names 3 and 4 at the same instant. That SIMULTANEITY puts an
ATOM on the diagonal of the joint survival function for subset Y — and THAT atom IS the tail dependence
(lambda>0), calibratable via gamma_Y. Conditional on the shock structure the names are INDEPENDENT, which
is exactly the conditional-independence property our warp construction LACKS (ours keeps Gaussian-MA
coupling under conditioning) — so DMO sits squarely in the Marshall-Olkin / Archimedean family, where ours
does not.

The relationship to our warp, stated exactly:
- Our shared-theta warp = common FACTOR on the MARGINALS, continuous, bounded → frailty mixture, lambda=0,
  enriched finite-quantile dependence. The lambda=0 CORNER of the common-shock family.
- DMO = common SHOCKS on the values DIRECTLY, with simultaneous (atom) outcomes → genuine joint events,
  lambda>0, calibratable joint rate. The lambda>0 INTERIOR of the same family.
Same family; opposite choices about the atom. Ours declines it (to keep uniform margins + an identifiable
Gaussian-MA copula); DMO embraces it (to get joint extremity).

IF Monsoon ever wants real cross-voice JOINT-EXTREMITY — voices slamming to a shared value at the same step
as a CALIBRATABLE RATE, not an inaudible knife-edge and not an accidental clamp artefact — DMO is the
principled route: a per-step "shock" indexed by a voice-subset, firing at a controllable intensity, forcing
a shared value on that subset. This is the rigorous version of the "tutti unison as an explicit feature with
its own probability knob" idea (deferred). It gives lambda>0 HONESTLY with a calibratable joint rate.
NOT scheduled — later, if ever; the warp foundation comes first. Recorded so the closed door's key isn't
lost: it's a published, owned construction.
SCOPE RULING (Rodney 2026-10-22): MO stays AT CLAMPING — no DMO implementation planned. Cross-LANE
common shocks DECLINED — hardly worth it: lanes are not commensurate objects (a "shared rail value"
means different things per lane), smooth cross-lane dependence already lives in the copula/spread
structure, and discrete cross-lane atoms would risk the lane-routing invariants for little payoff.
Within-LANE across VOICES: the worthwhile scope if the door ever reopens — voices on a lane share the
value universe (unison is meaningful there), and once the clamp lands most of the texture arrives free
(the co-hit corridor product -> min(lambda) via r; hit clustering by r; emergent co-clipping where
clamp geometry matches), with a scale quantizer collapsing near-unison at the comonotone end to ACTUAL
unison. What would still require the shock: a CALIBRATED unison rate (the corridor is emergent and
caps at min(lambda)) and unison at NON-rail values (shared-value shock, a different atom than the rail
event). Key stays on file.

### CLASSICAL MO REFERENCE — the KwMO tilt family (filed 2026-10-22)
The CLASSICAL, absolutely-continuous (lambda=0) branch of the same MO family tree — the formal home of the
"lambda=0 CORNER" above — is the KwMO tilt family: Alizadeh, Tahir, Cordeiro, Mansoor, Zubair, Hamedani,
"The Kumaraswamy Marshal-Olkin Family of Distributions", J. Egyptian Math. Soc. 23 (2015) 546-557,
doi:10.1016/j.joems.2014.12.002. Table 1 nests Kw-G (p=0), the MO family (a=b=1) and the PH/RH models; the
MO tilt is their eq (1), the composed family eq (4)-(5), and eq (14) is the CLOSED-FORM quantile:

    Q(u) = Q_G( (1-p)·H(u) / (1 - p·H(u)) ),   H(u) = (1-(1-u)^(1/b))^(1/a) = our KumaraswamyWarp quantile

With uniform baseline this is a candidate palette member ("Kumaraswamy-tilt"): the shipped Kw quantile
composed with ONE rational map; p=0 is BIT-IDENTICAL to the shipped warp (house neutral-parameter rule);
needs an authoring clamp p < 1 (p→1⁻ collapses R_p→0 for all u<1 — the atom-at-zero, rank-destroying case
the editor forbids). The tilt's defining property (hazard ratio h_KwMO/h_G monotone on the support:
increasing for p<0, decreasing for p∈(0,1)) is the knob's musical meaning; their Fig 2 gives the shape
taxonomy (constant/inc/dec/bathtub/UBT/S).
SCOPE FENCE: this family is a pure density everywhere (their eq (5)) — it is NOT the clamp/atom math.
The singular branch (rail atoms, lambda>0) stands on the DMO/common-shock references above plus
Marshall-Olkin 1997. Member proposal with scope/tests: PALETTE_KUMARASWAMY_TILT_NOTE.md.
PALETTE-WIDE (2026-10-22 Q&A): the tilt map R_p is EXACTLY the shipped Mobius member's invCDF
(a = 1-p) — the palette already contains the pure MO-extended uniform; the tilt composes with
EVERY member sample-side (monotone self-map of [0,1]; p=0 bit-identical; one mul+div per draw)
and brings NO atoms of its own (absolutely continuous for all p<1; 1/(1-p) density ceiling at
the approached rail; it is the clamp's mass feeder, not a rail source). Applicability table,
rails composition, p=1 ruling and the provisional P_MAX: PALETTE_KUMARASWAMY_TILT_NOTE.md.

### ATOMS, REVERSIBILITY, AND "STILL A COPULA?" (the precise statement)
An atom (clamp, flat segment, or a DMO shared-value shock) does NOT break reversibility — the generative
pipeline (Philox counter → MA sum → warp/clamp → output) is a DETERMINISTIC function of the position
counter, and reversibility = RE-RUNNING it forward from an earlier counter, never INVERTING it. A
non-invertible step (clamp: can't recover which u produced a pinned value) in a forward-reproducible chain
is still perfectly reversible. Reproducibility != invertibility (cf. y=round(x): reproducible, not
invertible). Reversibility rides on the first.
What the atom DOES break is SKLAR UNIQUENESS, not Sklar existence. Existence survives: the output always has
*a* copula. Uniqueness dies: with an atom the OUTPUT marginal CDF jumps, its range has gaps, and the copula
is pinned only on that range → infinitely many copulas reproduce the identical joint law → "the copula is
Gaussian-MA" stops being well-posed.
IMPORTANT — do NOT say "uniform margins die" (Rodney caught this). Two different objects are both loosely
called "margin": (a) the LATENT uniform U — the raw Philox draw (random_) the copula is built on — and (b)
the OUTPUT marginal, the distribution of the clamped/warped value. CLAMPING TOUCHES NEITHER'S UNIFORMITY IN
THE WAY THAT MATTERS: the latent U is UNTOUCHED (still exactly uniform, still the copula's basis, still
position-addressable) — it's the one thing clamping provably leaves alone. And the OUTPUT marginal was never
uniform anyway: ANY warp F^-1 makes it non-uniform; that's the point. So "loses uniformity" is NOT the
change. The actual change vs an ordinary warp is that the output marginal goes from CONTINUOUS (shaped,
non-uniform, but gap-free → Sklar-unique) to DISCONTINUOUS (a point mass at the rail → range gap → Sklar
non-unique). It is the DISCONTINUITY (the atom), not any loss of uniformity, that breaks uniqueness —
continuous-but-non-uniform margins keep uniqueness perfectly.
So the honest line is NOT "reversible but no longer a copula", and NOT "no longer uniform-margin" — it's:
reversible, latent uniforms intact, copula still CONSTRUCTED cleanly on genuine uniforms, but the OUTPUT
marginal is now discontinuous so the copula is no longer IDENTIFIABLE/UNIQUE from the output samples.
Operationally you lose: (1) reading the copula back off the output (the clamp destroys output rank info →
empirical estimate returns some class member, not necessarily Gaussian-MA — the copula is intact in the
CONSTRUCTION, just not OBSERVABLE from the output); (2) rank-correlation stability (ties shift
Spearman/Kendall → Spread calibration drifts). This is EXACTLY why atoms belong to a deliberate OUTPUT-STAGE
effect
(clamp / DMO shock), never the distribution layer — the warp's value is the clean identifiable
uniform-margin Sklar decomposition; atoms trade that description away for a specific effect, so they must be
a visible, chosen stage, not a property of the warp.

## CONCRETE — what you SEE on the Sands bars (2 voices x 16 steps)
Bars show the per-step per-voice probability/value. For two SPREAD-correlated voices:
- **BASELINE (uniform marginals + positive spread):** the two rows' up/down PATTERNS track — high-together,
  low-together, same steps; each row uses the FULL height range. Correlated. (Stronger spread -> rows more
  identical; spread=0 -> independent; spread<0 -> anti, A high where B low.)
- **SAME concave F^-1 (both) + spread:** both rows BUNCH into the same (e.g. low) band, AND their occasional
  tall spikes COINCIDE at the same steps -> visibly CLUSTERED (unison register + co-spiking).
- **DIFFERENT curves (A concave-low, B convex-high) + spread:** rows sit in DIFFERENT height bands, but
  MOTION still correlates (both rise/fall together WITHIN their bands) -> correlated-but-DISPERSED (coupled
  in direction, separated in level; talls don't co-bunch).
Visual signatures: CORRELATION = the two rows' up/down patterns agree. CLUSTERING (the F^-1 effect) = rows
share the same height band + extremes coincide. So F^-1 controls WHERE the correlated pattern lives +
whether voices share that band = the clustering-vs-dispersion axis, as vertical bunching on top of the
correlated pattern.

### Why the PRE/POST toggle earns its place
Bars must show the REALIZED post-F^-1 values to SEE clustering (clustering is a marginal effect). PRE-F^-1
bars show correlation only (raw correlated uniform); POST show correlation + clustering. The pre/post toggle
lets you SEE the distinction -> that's what justifies it (truthful display).

## Note to self
Probability here is COMPLEX with a lot composing at once: base (knob) x correlation INTENSITY (slew temporal
+ spread cross-voice) x F^-1 RESPONSE (marginal shape / clustering) x the copula (fixed Gaussian dependence)
x per-voice (Macro-global/Sands-unlock) x per-lane meaning (value-tilt vs control-response-curve). Good
reminder to keep the layers DISTINCT when reasoning/building: knob=base, slew/spread=how-coupled,
F^-1=how-expressed(marginal), copula=the dependence we don't touch. Conflating them is where confusion (and
bugs) hide.

---

## What the WARP is FOR (purpose, not mechanism) — Rodney
(Corrects an earlier invented "warp=where / slew=how orthogonality": slew is SHARED/per-lane, not per-voice;
that framing added nothing. The real story below.)

### Melody warp = another handle on the polyphonic CHAOS <-> ORDER level
The warp isn't a standalone feature — it's one more control in the set (copula + slew + faders + warp) that
ALL governs how STRUCTURED vs CHAOTIC the polyphony is:
- independent per-voice, no coherence -> soup/chaos
- copula-correlated -> coherent MOTION (less chaos)
- correlated + SHARED warp -> coherent AND same region (clustered; even less chaos)
- per-voice DIFFERENT warps -> controlled DIFFERENTIATION (ordered variety: voices deliberately in different
  registers, not random)
GLOBAL melody warp ~= fader adjustment (redundant for static shaping; useful only as a quick gesture / single
modulation handle vs rebalancing 12 faders). The VALUE is per-voice. It's all chaos-level control.
Also (Rodney): faders (and per-voice warp) MEDIATE how the shared slew's draw-to-draw correlation becomes
note-to-note MOTION — concentrated distribution damps slew's note-motion, spread one amplifies it.

### Octave warp = the distributional layer completing a FOUR-LAYER octave-control stack
The comprehensive answer to the Melodicer-style "limited octave control" criticism. Four ORTHOGONAL musical
dimensions of register (vs Melodicer's ~one knob):
1. STRAITS — deterministic per-voice octave OFFSET (WHERE each voice sits). Static, precise.
2. CAUSEWAY — modulation of those offsets (how the placement DRIFTS over time; temporal).
3. WARP — per-voice DISTRIBUTION within the octave range (favour low/high/spread/cluster).
4. COPULA — INSTANTANEOUS cross-voice correlation/decorrelation (voices change octave TOGETHER = parallel
   registral motion, or APART = contrary/independent = registral voice-leading across the ensemble).
= placement / temporal / distributional / cross-voice-correlational. The warp adds the probabilistic-
distribution layer on top of deterministic (Straits) + dynamic (Causeway) + correlational (copula).

### Unifying: it's all CHAOS-LEVEL control on the register axis
The same four-fold structure (placement/temporal/distributional/correlational) applies to melody AND octave,
and it all reduces to controlling the polyphonic chaos<->order level — on the register axis the COPULA is the
most direct chaos dial (together=ordered / apart=chaotic), warp adds distributional differentiation, Straits/
Causeway add deterministic/temporal structure. The warp ROUNDS OUT existing control systems (chaos-level for
melody; the octave stack for octave) — it is not a separate axis.

---

## WARP per-lane scope — PROVISIONAL/EXPERIMENTAL: add to ALL, feel, peel back (Rodney)
### The isolated-lane verdict (reasoning)
- VALUE/RANGE lanes (melody, octave, note-length-in-clock-mode): warp shapes a real outcome DISTRIBUTION
  -> clear value. Mono value iff the lane LACKS per-outcome faders (octave/length have none -> warp is the
  ONLY distributional control -> mono value; melody HAS per-degree faders -> warp polyphonic-only). (Faders
  and warp are the same role — mono distribution control — delivered differently: faders for few discrete
  meaningful outcomes, warp for ranges where N sliders would be clumsy.)
- THRESHOLD/boolean lanes (rest, legato, accent, qmix): fire-or-not, bar set by Straits poly knob+mod per
  voice. Warp can only reshape draw-before-threshold = a modulation-RESPONSE curve: marginal (inaudible as
  "shape" on a boolean), redundant with the knob. Isolated verdict: no warp.
Lane-principle (sibling of the distribution one): a warp belongs on a lane IFF the lane produces a
distribution over multiple outcomes. Provisional cut line: warp-worthy <=> VISUALISER-worthy (SAME
criterion: "has a meaningful outcome distribution to show/shape").

### BUT isolated reasoning has a BLIND SPOT -> decide by FEEL, not logic
The boolean verdict is ISOLATED-LANE. Three things it can't call (emergent / mode-dependent / perceptual):
1. REST + LEGATO warped TOGETHER: individually boolean, but their INTERACTION makes an emergent gap/phrase-
   length distribution the warp MIGHT shape (long-legato-phrases-with-big-gaps vs choppy cells). A point FOR
   trying.
2. VARIATION in GATE MODE: does something DIFFERENT (not note-length — subgate-related per the rest-source
   scheme). Whether warp helps depends on candidacy/threshold (marginal) vs density/distribution (real) —
   TRACE + feel.
3. GATE/SUBGATE warp with the rest-source scheme: warping the subgate draw might shape ghost/ratchet DENSITY
   distribution (a real range-outcome), interacting with the 2D-copula subgate texture -> potential genuine
   value even for "boolean" lanes.
These are emergent/mode/context effects, NOT callable by isolated-lane reasoning, ultimately PERCEPTUAL.

### Decision (reasoned): ADD WARP TO ALL LANES, experimentally; FEEL; PEEL BACK
Cheap to add (uniform F^-1 mechanism), cheap to remove. Add everywhere, then play — specifically feel the 3
interaction cases above. Carve back on (a) computational cost + (b) FUN FACTOR. Provisional DEFAULT cut =
the visualiser line (warp-worthy<=>visualiser-worthy), but OVERRIDE it wherever an interaction/mode/subgate
effect surprises in play. Reasoning sets the default; EXPERIENCE sets the verdict. (Perceptual question ->
build cheap, feel, decide — don't settle it analytically.)

────────────────────────────────────────────────────────────────────────────────
## CHANGE ALLEY AS THE WHOLE-STACK DEPENDENCE ROUTER (Rodney) — + clamp co-clipping

Two facets of one realization: CA is not just the LATENT-layer router — it can drive the whole
dependence stack coherently, and the clamp's co-clipping is its output-layer shadow.

### CA + clamp = emergent co-clipping, a faithful shadow of the latent structure (NOT DMO)
CA defines correlated submatrices in the UNIFORM layer: e.g. voices 7,8 take input from 6, and
6,7,8 all correlate to 5 to varying degrees — a clean, unique, CA-defined Gaussian-MA copula on
the U's. If those voices also have similar output distributions AND similar clamp geometry, they
cross the rail in near-unison → they CO-CLIP. Key properties:
- The joint-clipping RATE is NOT a parameter you set — it EMERGES from (a) latent correlation (CA)
  and (b) closeness of clamp thresholds + output shapes. Tune by EAR ("more CA correlation → more
  co-clipping"), not by formula.
- This is DISTINCT FROM DMO. DMO puts the atom IN THE CONSTRUCTION (an independent shock forces
  simultaneity; conditional independence otherwise; exact calibratable joint rate — right for
  PRICING a joint event). CA+clamp puts the correlation IN THE UNIFORMS and lets the clamp INDUCE
  co-clipping as a consequence; the atoms are correlated THROUGH the latent Gaussian layer, not an
  independent shock. For an INSTRUMENT this is the better interface (dial a feel, degrades
  gracefully, reuses existing machinery).
- "Correlations still have meaning up to the uniforms" (Rodney): with clamp engaged you ACCEPT you
  are no longer in the unique-copula regime at the OUTPUT (atoms → Sklar non-unique, non-invertible)
  — and that's fine, because your correlation SEMANTICS live at the UNIFORM layer where everything
  is clean and unique. The output co-clipping is a MONOTONE IMAGE (faithful but non-invertible
  shadow) of the latent CA structure: more latent correlation → more co-clipping, controllably.
- SUBTLETY (a feature): co-clipping needs latent correlation AND similar clamp geometry. Two
  voices CA-correlated at 0.9 but with DIFFERENT output shapes / rails cross their thresholds at
  different times → correlated but NOT co-clipping. So "correlated" alone doesn't predict
  co-clipping; "correlated AND similarly-shaped-and-clamped" does. Extra handle: decorrelate the
  clipping of correlated voices by giving them different shapes.
- NOT a "uniqueness opt-out feature" — there is no coherent such feature (the equivalence class is
  output-identical, nothing to navigate). It's just the honest acceptance that clamp → work at the
  uniform layer for correlation meaning, treat the output as a non-unique projection.

### CA can route the w / param MODULATION through its SAME correlation mappings — whole-stack coherence
CA_EXPRESSION_CV_CORRELATION.md already establishes the pattern (option B, CHOSEN + built): CA
PERMUTES EXTERNAL POLY CV through its per-stream LIVE voice correlation (a permutation lookup, no
new draw) — external envelopes come out reordered exactly as CA reorders that stream's notes,
feeding Keppel MPE. Pointing that SAME mechanism at the w-modulation (and the distribution param
modulation) input is NOT a new mechanism — same permute-poly-CV-by-stream-correlation, different
target. Consequence:
- ONE CA mapping then drives BOTH layers coherently: the LATENT coupling (copula on U) AND the
  COMMON-FACTOR loading (shared w-modulation → frailty/marginal structure). The voices CA
  correlates in the uniform layer are the SAME voices whose w-modulation is permuted by the SAME
  correlation → the two layers' loadings are ALIGNED BY CONSTRUCTION, set once.
- This upgrades the §6 "homogeneous loadings add positive dependence" result: the loadings are no
  longer set independently per layer — CA sets the pattern once and both inherit it. Latent
  co-movement and marginal co-opening REINFORCE through one control.
- Combined with clamp: the co-clipping shadow is then driven by a SINGLE coherent CA structure
  spanning all three layers (latent correlation → aligned w-modulation → aligned clamp crossing).
- NOT scheduled — recorded as the design understanding. Routing w/param-mod through CA is a
  natural extension of the existing expression-CV permutation; it belongs AFTER the warp
  foundation (w, mirror) and the param-mod layer exist. The CA plumbing it reuses is already
  designed (CA_EXPRESSION_CV_CORRELATION.md).

────────────────────────────────────────────────────────────────────────────────
## WHAT COPULA TYPE DOES CLAMP/ATOM GIVE? (Rodney) — completes the tail-dependence map

Worked out: with clamp enabled the map g_v = clamp ∘ F_v^-1 is monotone but NOT strictly
increasing — flat on U_v <= F_v(lo) and U_v >= F_v(hi). Those flat regions ARE the atoms (an
interval of U maps to the single value lo/hi). The output copula becomes a PATCHWORK over the
(U_i,U_j) square:
  - BOTH interior (unclamped middle): copula = C_Gauss-MA exactly (restricted to that sub-square) —
    unchanged.
  - ONE clamped, one interior: the pinned coordinate carries no rank info → dependence collapses
    to independence-like on that strip.
  - BOTH clamped SAME rail: Y_i=Y_j=rail → a point mass (perfect comonotonicity on that corner).
  - BOTH clamped OPPOSITE rails: a countermonotone corner point mass.
Region weights are set by the clamp thresholds via F_v(lo), F_v(hi).

TYPE: a copula with a SINGULAR COMPONENT — NOT absolutely continuous. Continuous part (the
Gaussian-MA interior) + singular part (the rail atoms). The corner atoms are EXACTLY the
MARSHALL-OLKIN singular component (MO's famous singular diagonal = a mass where two coords take
an identical extreme together = "both clamp to the same rail"). So:
  clamp gives a copula that is NOT absolutely continuous. By the Lebesgue decomposition it splits
  into a CONTINUOUS part (the Gaussian-MA interior — has a density) + a SINGULAR part (the
  Marshall-Olkin-type rail atoms — positive probability concentrated on a measure-zero set, the
  corners/edges): C = C_abscont + C_singular. (NOTATION: the earlier draft wrote "Gauss-MA (+)
  MO"; the "(+)" is NOT a direct sum ⊕ — it is this continuous+singular measure decomposition.
  Informally a MIXTURE of a Gaussian-MA-like copula and an MO-like copula with clamp-geometry
  weights; the one-clamped/one-interior strips are the loose edge of that gloss.) The SINGULAR
  part is what carries lambda>0; the lambda value = the corner-atom mass. (Finally WHY clamp was
  always "the atoms route to lambda>0".)

IS THE SINGULAR PART STILL "MOVING AVERAGE"? (Rodney) — NO, and the reason is structural.
The two parts share the SAME underlying MA correlation rho_{s,s'}=sum_j w_j w_{j+|s-s'|} but
express it through DIFFERENT functions:
  - CONTINUOUS part: correlation = rho_{s,s'} itself — LINEAR in the MA weights. MA structure
    preserved (strictly-monotone warp F^-1 preserves the copula by Sklar → rank structure, hence
    the MA dependence, is untouched).
  - SINGULAR part: the atom-to-atom co-occurrence between steps s,s' both at the rail is the
    bivariate Gaussian ORTHANT PROBABILITY Phibar_2(t,t; rho_{s,s'}), t=Phi^-1(F(rail)) — a
    NONLINEAR (arcsine-type), threshold-dependent function of rho. So the atoms are the
    EXCEEDANCE INDICATORS {z_s>t} — a THRESHOLDED (clipped) Gaussian process — whose dependence
    is the MA correlation passed through the orthant nonlinearity, NOT the MA correlation itself.
WHY MA dies on the atoms: MA is a LINEAR correlation structure; only linear/affine ops preserve
it. The clamp is a THRESHOLD (maximally nonlinear, rank-destroying on the clamped region) → it
turns the continuous field's MA dependence into the binary field's exceedance-correlation, a
different object. Strictly-monotone warp keeps MA; thresholding (the thing that makes the atom)
does not.
WHAT SURVIVES (the controllable residue): atom co-occurrence is MONOTONE INCREASING in
rho_{s,s'} → more MA/CA correlation at a lag ⟹ more joint-clipping at that lag. So the TEMPORAL
PROFILE of co-clipping inherits the SHAPE of the MA autocorrelation (bursts cluster at the lags
where the MA windows overlap most) — a faithful MONOTONE (non-invertible) shadow. But "inherits
the shape, monotonically" is NOT "is an MA process": the atoms are a clipped-Gaussian process
whose own autocorrelation is the arcsine-warped MA autocorrelation. Do not model/estimate the
singular part AS an MA — it's a thresholded-Gaussian exceedance process.

THE PAPER CONNECTION — clamp and DMO are the SAME singular component from OPPOSITE directions:
  - DMO (the QF paper): CONSTRUCTS the MO atom directly — a shock fires, forces simultaneity; the
    atom mass (hence lambda) is a FREE parameter gamma_Y you calibrate. Right for PRICING a joint event.
  - CLAMP + CA: INDUCES the MO atom indirectly — CA-correlated continuous variables both cross a
    threshold and pile at the rail. The atom mass is NOT free: it's a MONOTONE IMAGE of the CA
    correlation (+ closeness of thresholds/shapes). lambda is EMERGENT from the latent structure,
    steered through the correlation layer, not dialed. Right for PLAYING joint-unison (tune by ear:
    turn up CA correlation → voices clip together more).
Same family (mixed continuous+singular, lambda>0); DIFFERENT handle (DMO calibrates at the shock
layer; clamp+CA steers at the latent layer).

CAVEAT (bounds the clamp+CA route): the interior stays Gaussian (tail-independent), so ALL the
tail dependence comes from the corner atoms, whose mass is capped by how much probability the
clamp cuts off. => can't get arbitrarily strong lambda this way without heavily clamping (which
also flattens the marginals). DMO decouples these (strong joint-default, mild marginal effect).
clamp+CA trades DMO's calibration-freedom + marginal-independence for REUSING EXISTING MACHINERY
(CA correlation + a clamp stage).

THE COMPLETE TAIL-DEPENDENCE MAP (all three points now pinned):
  1. no-atoms warp (shared-theta modulation): lambda=0, clean UNIQUE uniform-margin Gauss-MA copula.
     Finite-quantile co-movement only. (The distribution layer — what we're building.)
  2. clamp + CA: lambda>0 EMERGENT. Gauss-MA continuous part + MO singular part (not abs.
     continuous; Lebesgue decomp, NOT a direct sum), MO mass = monotone image of CA
     correlation. Tune by ear. Reuses existing machinery. (A LIKELY FUTURE DIRECTION — Rodney.)
  3. DMO / common-shock: lambda>0 CALIBRATED. MO constructed directly via shock intensities.
     The rigorous/pricing version; the published route (arXiv 2009.00368).

────────────────────────────────────────────────────────────────────────────────
## IF ALL CDFs ARE STEP FUNCTIONS (Rodney) — the DISCRETE regime (≠ clamp; the native lane case)

DISTINCT from clamp. A step CDF means the output distribution is purely DISCRETE (mass at a
finite set of points, NO continuous stretch anywhere). F^-1 is then itself a step function = a
BIN/FADER map: partition [0,1] at the CDF jumps into intervals I_1..I_K, and Y_v = bin k iff
U_v in I_k. (Clamp = PARTIAL stepping: a continuous CDF with 2 flat spots → MIXED cont+singular.
Full step CDF = COMPLETE stepping: no continuous stretch → DISCRETE. Different objects.)

THE JOINT LAW = the Gaussian-MA copula measure on the bin-rectangle GRID:
    Pr[Y_i=k, Y_j=l] = C_Gauss-MA( I^i_k x I^j_l )   (the copula's measure of that rectangle)
So the dependence is the continuous Gaussian-MA copula INTEGRATED over rectangles — its discrete
footprint. NO Lebesgue cont/singular split to make: the output is discrete, full stop; the copula
is the discrete copula of a lattice-supported vector.

UNIQUENESS (at its most extreme): with discrete margins Sklar uniqueness fails hard — the copula
is pinned ONLY at the grid corners (F_i(jump), F_j(jump)) and is arbitrary (2-increasing) off the
grid. Infinitely many copulas reproduce the same box masses. The output keeps ONLY the rectangle
masses; all sub-rectangle info is gone. "The copula is Gaussian-MA" is true only in the weak
sense that A Gauss-MA copula reproduces the grid.

DOES MA SURVIVE? Yes, but QUANTIZED at the grid resolution (better than the clamp-to-binary case,
because bin assignment is a monotone COARSENING of the rank, not a collapse to one bit):
  - FINE step CDF (many bins): discrete copula ≈ a close lattice approximation of the continuous
    Gaussian-MA copula. MA essentially intact, just quantized.
  - COARSE step CDF (few bins): blocky — rho still present via the rectangle measures, coarsely
    resolved.
  - 2 BINS = one threshold: the binary/PROBIT limit — orthant probabilities Phibar_2(t_i,t_j;rho)
    (= the "full clamp to binary" case; see that note). The classical Gaussian-copula default
    model (Li/CreditMetrics), the ancestor of the DMO paper.
So step-CDF INTERPOLATES: infinitely many bins → clean continuous copula; 2 bins → probit. MA
doesn't die, it renders at the grid resolution.

NOT A CORNER CASE — IT'S WHAT THE DISCRETE VALUE LANES ALREADY DO: a step CDF IS a fader/weight
set, and MELODY/OCTAVE already consume their uniform through a step map (pickSemitone on the
fader weights — the discrete inverse-CDF). So:
  - the discrete value lanes are ALREADY in the step-CDF regime (not a special mode);
  - a CONTINUOUS warp F^-1 composed BEFORE the fader step (faderMap ∘ F^-1 — the melody-seam
    result) is step ∘ continuous = STILL a step CDF overall, with warp-ADJUSTED bin masses. Warp
    on these lanes = reshaping the bin masses of a quantized Gaussian-MA copula.
This reframes the three regimes cleanly:
  CONTINUOUS warp  → clean, unique, uniform-margin Gauss-MA copula (abs. continuous).
  CLAMP (partial step) → MIXED: Gauss-MA continuous part + Marshall-Olkin singular part.
  STEP CDF (full)  → DISCRETE: Gauss-MA copula measure on the bin grid; MA quantized; 2-bin=probit.

────────────────────────────────────────────────────────────────────────────────
## REACHABLE MATH STRUCTURES × DRIVERS — the master table (Rodney)

The dependence drivers do NOT live on one axis. There are THREE things a driver can touch, and
the allowed update CADENCE is DERIVED from one rule (Sklar/uniformity safety), not independent.

### The three axes
- WHAT it drives: draw-source | temporal (cross-step) correlation | cross-voice correlation
  structure | marginal shape | output atoms.
- SCOPE: cross-voice & temporal & sourcing drivers are LANE-GROUP scoped (all melody / all
  rhythm / all qmix together); marginal drivers (w, theta) are PER-VOICE. (Finding: DEPENDENCE
  is authored for the group — it's a property AMONG voices; MARGINALS are per-voice.)
- CADENCE, derived from the rule: SKLAR-SAFE per step -> STEP (or next-step); SKLAR-UNSAFE per
  step -> PHRASE boundary. (Phrase boundary is reserved for exactly the things that would break
  uniform margins / the copula if changed mid-window.)

### Verified facts behind the table (read from code, not assumed)
- Slew applies CONTINUOUSLY, not only on a fresh draw: the z-cache is keyed on (N, frac, r) with
  r = slew-rho (PatternEngine.cpp:688), so a rho change re-interpolates the in-flight window.
  => modulating rho mid-window would break that window's uniformity -> PHRASE-gated. Confirmed.
- A/B mix selects/blends the RAW UNIFORM DRAW SOURCE (not correlation), at lane-group level,
  PER-STEP allowed (switching between independently-uniform sources per step stays uniform ->
  no Sklar violation -> no phrase gate).
- CA: manual pin move -> NEXT STEP; trigger-modulated STRUCTURAL change -> PHRASE boundary
  (a structural correlation change mid-window disturbs in-flight copula consistency).
- w: Sklar-safe per step (monotone mix) -> STEP-latched (from CC_PROMPT_03 / -18b §2).

### THE TABLE
| Driver | Layer / axis | Scope | Static effect | Shared-MODULATED → factor? | Cadence (why) | λ>0? | Reversible? |
|---|---|---|---|---|---|---|---|
| Spread (r) | latent / cross-voice | lane-grp | base copula correlation | it IS the base, not a factor | step | no (Gaussian) | yes |
| A/B mix | SOURCING (raw draw) | lane-grp | picks draw source | yes — REGIME-SWITCH / MIXTURE factor (mixes two draw regimes; NOT a Gaussian-corr factor) | STEP (uniform-safe) | no (both regimes tail-indep) | yes (source is position-keyed) |
| Slew-rho | latent / TEMPORAL (cross-step) | lane-grp | sets MA autocorrelation (temporal smoothness); applies CONTINUOUSLY | yes — common factor in TEMPORAL autocorrelation (stoch-vol-of-smoothness) | PHRASE (per-step breaks in-flight window uniformity → Sklar) | no (steps jointly Gaussian → temporally tail-indep) | iff rho-sequence is position-keyed |
| CA routing | latent / cross-voice | lane-grp | correlation submatrix structure | yes — routes the shared drive → LATENT cross-voice factor | manual pin→NEXT-STEP; triggered struct→PHRASE (gated by COHERENCE, NOT Sklar — see note) | no (Gaussian) | yes |
| w (depth) | MARGINAL | PER-VOICE | shapes marginals; w=1 static = full F^-1, **NO factor** (Sklar-invariant per-coord map) | yes — MARGINAL / FRAILTY factor | STEP (monotone mix = Sklar-safe) | no (bounded, no clamp) | yes (draw-time latched) |
| theta (dist params) | MARGINAL | per-voice | shapes marginal family | yes — marginal/frailty factor | step (draw-time latch) | no (bounded) | yes if latched & position-keyed |
| w UNDER CLAMP | marginal + OUTPUT | per-voice | dials the Lebesgue decomp (cont ↔ MO-singular weight) | yes — frailty factor **+ CO-MODULATED λ** (atom masses breathe together → common factor IN the tail) | step | **YES (atoms)** | yes |
| Clamp depth | OUTPUT | tbd | sets atom mass (MO singular weight) | tbd | tbd | yes (atoms) | yes (deterministic map) |
| DMO shock (future) | OUTPUT | tbd | constructs MO atom directly | calibrated joint rate | tbd | yes (calibrated) | yes if shocks position-keyed |

### THE FOUR ONE-LINE LAWS the table encodes
1. STATIC LEVEL NEVER MAKES A FACTOR (any layer). w=1 static = max marginal shaping, ZERO
   dependence-structure change — it was never a factor model, so w=1 doesn't "lose" one.
2. SHARED MODULATION ALWAYS MAKES A FACTOR — and the factor's TYPE = the layer it acts on:
   latent-cross-voice (CA), temporal (slew-rho), sourcing/regime (A/B), or marginal/frailty (w,θ).
3. λ>0 IS REACHABLE ONLY BY ATOMS (clamp / DMO). No modulation of ANY bounded continuous
   parameter — correlation, rho, source, w, θ — ever breaks the tail barrier; all give λ=0.
4. CADENCE IS DERIVED, NOT CHOSEN — from one of TWO risks:
   - UNIFORMITY (Sklar) risk → phrase: a change that corrupts an in-flight window's uniform
     margin if applied per-step. Slew-rho (changes the MA window WEIGHTS mid-blend → margin
     no longer uniform). w/θ are Sklar-SAFE per step → step-latched.
   - COHERENCE risk → phrase: a change that seams the DEPENDENCE mid-pattern (margins fine,
     but the correlation structure has a discontinuity inside the phrase). This is CA's reason,
     NOT Sklar — a CA pin move swaps the draw SOURCE (not the window weights), so a blend of
     uniforms stays uniform → margins survive a mid-window swap. What it risks is a correlation
     seam mid-phrase.
   The manual-vs-triggered SPLIT within CA is then by trigger-source-and-frequency, NOT by layer:
   - TRIGGERED (CV/mod) structural change can fire ANYWHERE → PHRASE-gate so no mid-pattern seam.
   - MANUAL pin edit is deliberate + rare → NEXT-STEP: the one-step coherence seam is acceptable
     (and immediate feedback matters more — same feel rule as w's edit→next-step; phrase-gating a
     manual edit would feel laggy). KEEP manual=next-step. (FEEL-TEST caveat: if a mid-phrase pin
     move audibly GLITCHES rather than cleanly step-transitions, reconsider — phrase-gate it or
     quantise to next beat. Theory says next-step is fine; ears decide if the seam is ugly.)

────────────────────────────────────────────────────────────────────────────────
## CLAMP ATOMS AT THE DRAW LEVEL — what they DO, and DIRECTIONAL tail via spread sign (Rodney)

### The atoms, concretely (single voice)
Clamping the first/last part of F puts DIRAC MASSES at the output rails: the warped uniform hits
0 with prob p0 = F(lo) and 1 with prob p1 = 1-F(hi). The output marginal is
    mu_Y = p0·delta_0 + p1·delta_1  (SINGULAR part)  +  f_cont(y)dy on (0,1)  (ABS-CONTINUOUS part).
delta_0, delta_1 sit on {0},{1} — LEBESGUE-measure-zero points carrying POSITIVE probability (the
definition of an atom; why the output is not abs. continuous). In COPULA space they land on the
boundary/Fréchet CORNERS — which is WHY they carry lambda>0 (an atom at the (1,1) corner IS the
comonotone/upper-Fréchet touching point); the location is the whole reason clamp reaches lambda,
not an inert spike. (Atoms at exactly 0/1 only if lo/hi = the support rails; interior clamp → atoms
at interior points, still singular, but off the Fréchet corners so the clean lambda reading needs
rail-clamping.)

### Through the lane maps (the draw-level picture)
- MELODY: uniform=0 → bottom bin → LOWEST degree; =1 → top → HIGHEST. So P(lowest pitch) >= p0,
  P(highest) >= p1. ("AT LEAST": the atom is a FLOOR (rigid mass exactly at the rail) ON TOP of the
  continuous part's near-rail mass. The atom is the RIGID extreme; the continuous tail is the SOFT
  near-extreme.)
- OCTAVE: roll=0 → bottom octave, =1 → top. Same.
- VARIATION (gate-mode subgate density): atom at 0 → MUCH shorter/sparsest, atom at 1 → MUCH
  longer/densest, each >= p0/p1. Rodney's "a bit vs much": the CONTINUOUS part gives graded (a bit
  shorter/longer); the ATOMS superimpose rigid EXTREMES (much shorter/longer). Variation becomes
  graded-middle + rigid-extreme spikes.

### Why the atom (not the continuous tail) is the lambda-bearer, seen from the draw
The ATOM mass CO-OCCURS rigidly (two voices both at U=0 → both lowest, locked, same step); the
continuous near-rail mass is correlated but NOT locked. So cross-voice lambda>0 comes from the atom
floor, not the continuous tail — the draw-level reason only atoms reach lambda and the continuous
Gaussian part stays lambda=0. CLEAN TWO-KNOB PICTURE: clamp depth = MARGINAL atom rate (p0,p1);
cross-voice correlation = how much the atoms CO-OCCUR = lambda.

### DIRECTIONAL tail via spread SIGN — and the per-voice structure (VERIFIED)
spread is PER-VOICE-SLOT, PER-LANE, BIPOLAR: spread[kVoiceSlots][NUM_STRANDS], clamped [-1,1],
"negative spread inverts the target" (SequencerEngine.hpp:433; MonsoonExpanderManager.cpp:305 the
spread-sign fix). The leader is the voice's CA-assigned pre-remap draw
(MonsoonExpanderManager.cpp:420). So EACH voice independently sets (leader, signed rho).
  - POSITIVE spread → atoms CO-OCCUR SAME-rail: both lowest together, or both highest together
    (comonotone corners (0,0),(1,1)). Musical: unison tutti at an extreme.
  - NEGATIVE spread → atoms co-occur OPPOSITE-rail vs the leader: voice bottoms as leader tops
    (countermonotone corners (0,1),(1,0)). Musical: voices SPLIT to opposite extremes together —
    max chord spread / one voice sparse while another dense, same step.
  - SPREAD TARGET = mono OR follow-CA (VERIFIED, MonsoonExpanderManager.cpp:420): in follow-CA mode
    the correlation is PAIRWISE ALONG THE CA ROUTING EDGE — "the leader is the post-remap value =
    src[v]'s material" (the voice CA channelled THIS voice from), with this voice's own pre-remap
    draw as the idiosyncratic endpoint. So if CA routes V4 from V2, V4's spread is corr(V2, V4)
    SPECIFICALLY — a signed edge in the CA routing TREE, NOT correlation to one global leader.
    => NEGATIVE edge V2→V4: V2 tops as V4 bottoms, a LOCKED PAIRWISE opposite-tail couple
    (independent of other voices). POSITIVE edge: they co-occur same-rail.
  - STRUCTURE = RANK-1 PER BLOCK (depth-1 single-anchor), NOT a deep tree. The authoritative map is
    SPREAD_TARGET_MODES.md § "Reachable correlation structures": spread gets ONE anchor per lane;
    each voice reads that single anchor's material (follow-CA) with a signed rho. Within a CA block
    the correlation is RANK-1 (one common factor) — so two voices sharing a block anchor get
    ±rho^2 between them (same spread sign → +rho^2 same-rail co-occur; opposite → -rho^2
    opposite-rail). Rodney's V6+/V7-,V8- into anchor V5 = a structured 3-way split {V6} vs {V7,V8},
    which IS a signed rank-1 star on that block. Reachable structures (per that doc): diagonal (no
    corr) / signed rank-1 star / block-diagonal rank-1-per-block (arbitrary partition, per-voice
    sign+strength, zero across blocks) / neighbour-only banded (scatter cycles).
  - NOT reachable (and correct): arbitrary correlation matrices, and COMPOUNDING MULTI-HOP CHAINS
    (V4→V2→V1 products). Spread is depth-1 single-anchor by design (a per-voice target map IS the CA
    pin matrix — not rebuilt in spread); and CA pin CHAINS are a NON-IDEMPOTENT hazard it guards
    against (CHANGE_ALLEY_DESIGN.md:227), not a correlation-authoring feature. (CORRECTS my earlier
    "signed tree with compounding chains" — the shipped structure is rank-1-per-block, not a deep
    tree. The pairwise-edge READ is real (a voice reads its anchor's material), but the reachable
    STRUCTURE is the depth-1 star/block, per SPREAD_TARGET_MODES.md.)

### TWO INDEPENDENT LEVERS for opposite-tail structure
1. CA per-voice (leader + signed rho) → sets the CORRELATION sign between any voice and its leader →
   structured same-rail / opposite-rail subgroups via rho^2 propagation (latent layer).
2. PER-VOICE WARP ORIENTATION (top-clamp vs bottom-clamp — which rail the atom sits on) → flips
   which rail a voice lands on INDEPENDENTLY of its correlation sign; even +correlated voices can
   land opposite rails if oppositely clamped (output layer).
Combined, the full matrix is authorable: WHICH voices co-occur (CA), in WHICH direction
(spread sign), onto WHICH rail (warp orientation).

### FORMAL CAVEAT (so the write-up is airtight)
lambda_U, lambda_L are defined for SAME-direction co-extremity; the opposite-rail (negative-spread)
case is COUNTERMONOTONE atom co-occurrence on the (0,1)/(1,0) corners — sometimes "negative tail
dependence", stated via the survival/countermonotone framing. lambda stays >=0 by definition; what
the spread SIGN flips is WHICH CORNERS carry the atoms, not the sign of lambda.

────────────────────────────────────────────────────────────────────────────────
## DEFERRED CLAMP — control surface DECIDED (Rodney) — "when we build it, this is the shape"

STATUS: DEFERRED, de-risked. Build order: warp foundation + hand-drawn STEP functions FIRST
(step functions already give the dramatic-effect case without atom machinery). Atoms via clamp
come later — but the theory is fully mapped (named structures, reversibility preserved, Lebesgue
decomp controllable, directional tail via signed spread), so switching it on is understood, not
a leap. This note records the DECIDED control surface so the eventual spec starts from decisions,
not a re-derivation.

### Control surface
- lo / hi KNOBS (clamp thresholds in OUTPUT space) + MODULATION inputs, per-voice-per-lane.
- Atom masses: p0 = F(lo) at output 0-rail, p1 = 1-F(hi) at the 1-rail (the Dirac masses).

### CONSTRAINT: lo < hi (enforced) — and what it guards
- lo < hi keeps the continuous middle NON-EMPTY → the MIXED (cont + MO-singular) regime.
- lo == hi collapses to a single step = the full-step-CDF DISCRETE limit (valid, but a DIFFERENT
  regime reached deliberately, not by accident — enforce lo<hi so you don't fall into it).
- lo→0 / hi→1 continuously shrink their atom to zero → sweeps the whole CONTINUOUS↔MIXED spectrum.
  So the two knobs ARE the continuous-to-mixed dial; lo<hi is the guard that keeps you in "mixed".
- POSSIBLE further constraint (feel-decision, later): rail-aligned vs free-interior clamp. Atoms
  at exactly 0/1 land on the Fréchet CORNERS → clean lambda interpretation. Interior clamps are
  legal + musical but their lambda reading is murkier (off-corner). Two modes ("snap to rails" vs
  "free interior") is an option — decide by ear, not now.

### MODULATION cadence — probably STEP-rate (unlike slew)
Modulating lo/hi modulates the ATOM MASS (p0,p1) → modulates lambda itself when shared across
voices (the tail-dependence control; master table). CADENCE: clamp is a POINTWISE output map, NOT
a window operation — changing the threshold per-step does NOT corrupt an in-flight MA window the
way slew-rho does. So by the Sklar-safety cadence rule it is likely STEP-latchable (like w), not
phrase-gated (like slew). CONFIRM against the master-table logic when specced, but
clamp-is-pointwise ⇒ step-safe is the expectation.

### WHY THIS IS MORE REASON FOR SANDS SUBPANELS (the forcing function)
Clamp adds, per lane: lo knob, hi knob, lo-mod jack, hi-mod jack (+ attenuverters) — more
per-voice-per-lane columns on an already-wide panel. Combined with w-level / w-mod / param-mod,
the output+modulation tier exceeds what the WIDE-PANEL interim absorbs gracefully. So "we'll
eventually want clamp" is a concrete FORCING FUNCTION for the subpanel migration. Dependency chain
(all already specced): clamp wants subpanels → subpanels want the connection-model migration
stages 3-4 (id-pairing, see CONNECTION_MODEL_MIGRATION_NOTE.md) → which is half-done. So finishing
the connection-model migration is the real prerequisite for clamp's UI, and clamp is one more
reason to prioritise it.

### WHAT'S ALREADY SETTLED (so the eventual spec inherits, not re-derives)
- The copula it yields: Gauss-MA continuous part + Marshall-Olkin singular part (Lebesgue decomp);
  w (if clamp F) dials the decomposition weight; lambda>0 via the atoms.
- Reversibility: PRESERVED (clamp is a deterministic pointwise map; re-run forward from the
  position counter — invertibility not required).
- Directional tail: +spread = same-rail co-occur (comonotone), -spread = opposite-rail
  (countermonotone), authored via the signed rank-1-per-block spread structure (SPREAD_TARGET_MODES.md).
- Draw-level effect: lowest/highest pitch >= p0/p1 (melody/octave), much-shorter/longer variation;
  atoms co-occur rigidly (the lambda-bearer), continuous tail does not.
- Output-stage placement: a DELIBERATE, VISIBLE stage AFTER the distribution layer (never folded
  into the warp — the warp's value is the clean unique uniform-margin Sklar decomposition; atoms
  trade that away for an effect, so they stay a chosen stage).

────────────────────────────────────────────────────────────────────────────────
## CLAMP + CROSS-LANE TAIL DEPENDENCE — the first (and only) cross-LANE coupling mechanism (Rodney)

VERIFIED PREMISE: cross-LANE correlation is ZERO by architecture. Lanes draw from DOMAIN-SEPARATED
Philox streams — "one seed produces N INDEPENDENT stream keys via a per-stream offset" (PhiloxRng.hpp
:110-114). So a melody uniform and an octave uniform are INDEPENDENT. Everything in the dependence
theory so far (spread, CA, warp/θ modulation) is cross-VOICE (within a lane); cross-LANE has been
structurally zero. Clamp is the FIRST thing that can change that — and only under one condition.

### Does clamp create cross-lane tail dependence? ONLY with a SHARED FACTOR spanning the lanes.
- WITHOUT a shared factor (today's independent lane streams): NO. Two independent lane uniforms, each
  clamped, hit their rails independently → joint rail-hit rate = p_mel · p_oct (the PRODUCT = the
  INDEPENDENCE value). Atoms exist per lane but DON'T co-occur beyond chance. Clamp alone gives
  cross-VOICE tail dependence (same lane, correlated voices) but NOT cross-lane.
- WITH a shared factor across lanes: YES, and it is EXACTLY MARSHALL-OLKIN. If ONE modulation source
  drives the clamp depth (or warp) on BOTH melody AND octave, then when that source pushes both toward
  their rails together, the two lanes atom-hit SIMULTANEOUSLY — an MO shock indexed by the lane-SUBSET
  the modulation touches. The simultaneous rail-hit IS the MO singular diagonal, now in LANE space
  (where before it was only in voice space). Genuine cross-lane tail dependence, calibratable by the
  shared factor's reach.

### Why this is a NEW axis (and musically pointed)
It's the FIRST mechanism that creates cross-LANE coupling at all (the streams are independent by
design). And it does it specifically IN THE TAIL (the atoms). Musically: "melody AND octave both slam
to their extremes in the SAME step" = the whole VOICE lurches together across lanes — distinct from
cross-voice ("voices 1-4 all go high together, in one lane"). Two orthogonal co-extremity gestures.

### λ status — true λ>0 here (atoms), bounded by the shared factor's reach
Unlike the continuous frailty case (λ=0, bounded factor), clamp makes a REAL point mass, so the
cross-lane MO atom is genuine λ>0. But the MASS in that cross-lane atom (how often the lanes co-hit)
is set by BOTH: clamp depth (per-lane marginal atom rate p_lane) AND the shared factor's reach (how
correlated the two lanes' rail-crossings are). Bounded shared factor → elevated finite-quantile
cross-lane co-occurrence + a real-but-small cross-lane atom. The product-rate (no shared factor) is
the λ=0 cross-lane floor.

### DESIGN RECIPE (if cross-lane tail coupling is ever wanted — the whole-voice-lurch gesture)
Now known precisely: (1) clamp BOTH lanes; (2) route ONE modulation source to BOTH lanes' clamp-depth
(or warp). Clamp provides the atoms; the shared cross-lane modulation provides the coupling; together
= an MO singular component in lane-space. WITHOUT step (2), clamp's atoms stay cross-lane-independent
(product rate, no coupling). This is the ONLY route to cross-lane dependence in the whole architecture
(domain-separated streams give nothing else), so if the gesture is wanted, this is how; if cross-lane
independence is wanted (the default), simply don't share a factor across lanes' clamps.

────────────────────────────────────────────────────────────────────────────────
## BOOLEAN-LANE CROSS-LANE COUPLING — the case that STRENGTHENS "booleans need shared modulation" (Rodney)

Rodney's example: link legato's "long" extreme to rest's (or accent's) extreme — same OR opposite
direction — via one modulation source driving both lanes' clamp (or θ). This is the cross-lane MO
result applied to BOOLEAN lanes, and it is the MOST musically vivid form.

### What it does (boolean lanes = threshold, not value)
Legato/rest/accent are BOOLEAN lanes: the uniform crosses a threshold (r < prob), it does not pick a
value. So there is no "extreme value" atom like melody's lowest-pitch — the clamp atom on a boolean
lane is an atom in the FIRING DECISION: "this step LOCKED to fire" / "LOCKED to rest". A shared mod
source at its extreme then makes, in the SAME step: legato LOCKED-long AND rest LOCKED-present (same
rail) — or legato-long with rest LOCKED-absent (opposite rail, sign of the mod into each lane's clamp).
=> a RIGID cross-lane co-occurrence of the GATE decisions — the whole voice's ARTICULATION snaps to a
configuration together. HARDER / more audible than value-lane co-leaning (a rhythmic co-event, not a
soft lean). Examples: "long held notes ⟺ sparse rests" (same-rail), "sustained ⟺ dense activity"
(opposite-rail) — neither reachable by any per-lane knob setting.

### Why it SURVIVES the boolean-redundancy proof (and strengthens the earlier conclusion)
Earlier result (-18b §3): on a boolean lane, STATIC warp/clamp is REDUNDANT with the threshold knob
({F^-1(U)>t} ≡ {U>F(t)} — a knob move). We concluded boolean-lane distribution is "only relevant
under shared modulation". THIS RESULT STRENGTHENS THAT, because it shows WHY and gives the missing
positive content:
- The redundancy is MARGINAL (a single lane's own firing RATE). It does NOT touch the cross-lane JOINT
  structure. Knobs are per-lane-INDEPENDENT → they CANNOT correlate two lanes' firing.
- So shared-modulated clamp/θ across booleans is NOT redundant: it creates cross-lane co-occurrence
  (MO coupling of the gate decisions) that NO knob arrangement reaches — a JOINT effect, not a marginal
  one. The redundancy proof only ever ruled out the marginal; the joint was always open.
TWO EARLIER THEOREMS CONVERGE on the same gate: (1) boolean static warp = redundant (marginal), and
(2) cross-lane = independent by default (product rate). BOTH are escaped ONLY by shared modulation —
which is therefore the UNIQUE regime where boolean-lane distribution control does something knobs
can't, and that something is cross-lane tail coupling.

### "Distribution beyond rescaling knob output" — scoped precisely
- VALUE lanes (mel/oct): warp is beyond-rescaling even STATICALLY (non-affine marginal reshape; a knob
  only shifts/scales). PLUS cross-lane coupling under shared mod.
- BOOLEAN lanes (rest/acc/var/leg): warp is beyond-rescaling ONLY under shared modulation — where it
  becomes cross-lane coupling (this note) or per-voice modulation-response (-18b §3). Static boolean
  warp is exactly a rescaled knob.
So "distribution goes beyond rescaling" is TRUE for booleans precisely WHEN scoped to shared modulation
— exactly the boundary Rodney drew. The headline: shared-modulated clamp/θ is the ONE regime that gives
boolean-lane distribution control a job knobs cannot do, and that job is cross-lane MO tail coupling.

────────────────────────────────────────────────────────────────────────────────
## DECISION: DON'T exclude boolean lanes from the distribution layer (Rodney) — the justification ledger

A plausible SCOPE-CUT was available and nearly correct: "boolean lanes have a threshold; static warp
on a threshold is PROVABLY redundant with the knob ({F^-1(U)>t}≡{U>F(t)}); therefore don't wire
distribution to boolean lanes — save the complexity." That logic is CORRECT FOR THE MARGINAL and
LOCALLY airtight. It is GLOBALLY WRONG. Record of why, so the cut isn't re-proposed:

### The justification ledger — which reasons are REAL vs CONVENIENCE
1. Shared modulation of a boolean's OWN firing RATE → KNOB-REACHABLE = CONVENIENCE ONLY. Modulating
   the clamp/θ to change a single lane's own rate over time is reachable by modulating the KNOB
   directly (marginal redundancy holds under modulation too, for a lane's own rate). A CV shaper does
   it. Real but weak.
2. RANDOM SOURCE (the probability CV OUT) → NOT knob-reachable = REAL. The boolean uniform feeds BOTH
   the threshold AND the continuous prob-CV-out. On the CV OUT, warp is a genuine marginal reshape —
   the lane EMITS a shaped continuous value; the knob only moves the fire/don't-fire decision, never
   the emitted signal. The redundancy proof covered the THRESHOLDED consumption, NOT the CONTINUOUS
   emission. Same uniform, two consumers, different verdicts (cf. the per-lane display gate).
3. CROSS-LANE MO COUPLING under shared modulation → NOT knob-reachable = STRONG. Joint co-extremity
   across lanes (legato⟺rest/accent) that per-lane-INDEPENDENT knobs structurally cannot produce.
4. PER-VOICE MODULATION-RESPONSE under shared modulation (Sands unlock) → NOT knob-reachable = REAL.
   16 voices, 16 different response curves to one shared CV; one knob per lane can't do per-voice.
=> THREE of four reasons (2,3,4) are genuinely non-redundant; only (1) is convenience. Any one of 2/3/4
alone justifies keeping booleans in.

### The OVERRIDING (decision-level) reason — stronger than any individual item
Even if every reason were weak, DON'T exclude, because:
- Excluding is STRUCTURAL not behavioural: it removes the CAPACITY for 2/3/4, which can't be retrofit
  onto a lane whose distribution machinery was never built. And 2/3/4 were NOT VISIBLE when the
  exclusion decision would have been made — the marginal proof is a LOCAL maximum that forecloses the
  better global result.
- Excluding creates a LANE-IS-SPECIAL SEAM in the distribution layer ("these lanes don't have warp") —
  exactly the class of distinction that breeds the bugs the whole seam-eradication effort exists to
  kill. Including costs only INERT DEFAULT STATE (IdentityWarp, w=0) on those lanes.
This is the SAME discipline as routing every voice through warpRow (CC_PROMPT_05): uniformity in the
plumbing is what lets capability arrive later WITHOUT a seam-closing fight. Keep ALL lanes in the
distribution layer, inert-by-default on booleans.

GENERALIZABLE RULE: do NOT exclude a lane/voice from a layer on MARGINAL-REDUNDANCY grounds — the
joint/modulated/alternate-consumer effects are often invisible at exclusion time, excluding
forecloses them, and the exclusion is itself a seam. Inert-uniform beats absent-special.

────────────────────────────────────────────────────────────────────────────────
## CROSS-LANE COUPLING — the USER-FACING feature + the singular-part rule (Rodney)

### THE UNIFYING CRITERION (supersedes/refines the two prior cross-lane notes)
Shared modulation creates cross-lane / binary-lane MO coupling under clamp IFF it moves the SINGULAR
part of the Lebesgue decomposition (the atom) in a SHARED, DIRECTIONAL way. Sort every modulation by
"does it move the atom directionally?":
  MOVES THE SINGULAR PART → COUPLES:
   - shared w (THE Lebesgue MIXING WEIGHT — `(1-w)·id + w·curve`; w scales the atom's weight directly;
     shared-w makes the decomposition more/less discontinuous in sync → couples). [corrects my earlier
     under-statement of w.]
   - shared clamp-point (lo/hi) — moves where/how big the atoms are, jointly.
   - shared GAUSSIAN-CORRELATION (spread) under clamp — the clamp RECTIFIES bounded-Gaussian corr into
     atom co-occurrence; modulating the shared corr modulates that co-occurrence. [CORRECTS last note's
     "cross-voice only / cross-lane synchronized-only" restriction — under clamp on a continuous dist
     it DOES create cross-lane incl binary coupling.]
  MOVES ONLY THE CONTINUOUS PART → DOES NOT COUPLE:
   - shared θ (distribution PARAMS) — reshapes the interior density; the atom comes from the CLAMP
     operation, which θ doesn't control. w = "how much atom"; θ = "shape of the non-atom part". Only w
     is singular-part modulation.
  MOVES THE WHOLE THING NON-DIRECTIONALLY → DOES NOT COUPLE:
   - A/B mix (regime switch — moves between draw sources, not toward/away a shared rail).
   - slew-rho (temporal autocorrelation — within-lane across-step, not cross-lane atoms).
WHY w co+θ differ (airtight): w is the MIXING COEFFICIENT of the decomposition (scales atom weight);
θ parameterizes the continuous CURVE SHAPE (the atom's existence/mass is the clamp's, not θ's).

### THE USER-FACING FEATURE: "gang chosen lanes' extremes"
The copula/MO math is the HOW; the WHAT is a compositional control: pick a set of lanes, a DIRECTION
(same/opposite extreme) per lane, and a shared mod source → those lanes' EXTREME events fire together.
All SEVEN lanes are coupling-capable (each has a uniform → clampable): MEL/OCT/QMIX/REST/ACCENT/
VARIATION/LEGATO. (Confirms the "don't exclude any lane from distribution" decision — the feature NEEDS
all 7 distribution-capable.) Note on "binary": REST/ACCENT/LEGATO are threshold lanes — their "extreme"
= extreme FIRING PROBABILITY (locked-fire / locked-rest), NOT a continuous value. VARIATION is the
probabilistic NOTE-LENGTH-amount lane (how far length varies from base) — NOT LOR — so "long/short
notes" couples via the standard mechanism.

Worked musical couplings (each via clamp + shared singular-mod):
  - accent/legato/rest ⟺ high/low OCTAVE or scale DEGREE ("accented notes land high", "rests cluster low")
  - accent/legato ⟺ VARIATION extreme ("accents come with longest/shortest notes")
  - accent/legato ⟺ QMIX extreme ("accented steps favour quantiser notes over seq notes")
Direction (same-rail vs opposite-rail) = the SIGN of the shared mod into each lane's clamp (cf. the
per-voice signed-spread result). None reachable by per-lane-independent knobs.

### GATE-MODE SUBGATE COUPLING — mostly FREE (planned subgate design, not cross-lane machinery)
PLANNED: in gate mode, VARIATION is assignable to subgate rest/legato decisions, which can ALSO take
signal from the main rest/legato knobs. CONSEQUENCE: the subgates in a ratchet burst SHARE their
driving uniform BY CONSTRUCTION (Variation drives them, optionally blended with main rest/legato). So
"correlate subgates" is NOT the cross-lane MO machinery — it's intrinsic: CLAMPING VARIATION directly
shapes the whole burst's articulation together (all subgates read the same clamped Variation uniform).
Two intrinsic channels: Variation→subgates, and main-rest/legato→subgates (the blend) — each clampable.
The cross-lane MO machinery is for DIFFERENT lanes; the subgate coupling is WITHIN Variation's reach,
shared by design → cheaper. (Design consequence to realise when the planned gate-mode subgate feature
is built; not verified in code — it's planned.)

────────────────────────────────────────────────────────────────────────────────
## 3×3 CROSS-DOMAIN CORRELATION — considered, SET ASIDE (Rodney; echoes the earlier CA decision)

QUESTION: a standing correlation among the THREE RNG DOMAINS — pitch (mel+oct), rhythm (rest+accent+
variation+legato), qmix — i.e. ρ_pitch-rhythm, ρ_pitch-qmix, ρ_rhythm-qmix. (NOT 7×7 by lane: the 7
lanes aren't independent DOF — mel/oct share the pitch stream, the rhythm lanes share the rhythm
stream. The 3 DOMAINS are where the independence is enforced, so 3×3 is the correctly-dimensioned
object, not a reduced 7×7.)

FEASIBILITY: genuinely feasible (unlike a 7×7): 3 numbers, PSD-trivial, implementable as ONE shared
Gaussian factor injected into the 3 domains' latent draws with per-domain loadings. A clean, legible
control ("how much do pitch / rhythm / qmix decisions move together").

WHY SET ASIDE (same reasoning that made CA back away from it earlier — and it still holds):
1. It BREAKS a DESIGNED INVARIANT — the 3 domains are independent Philox streams BY DESIGN (pitch
   randomness must not contaminate rhythm randomness). A standing 3×3 factor couples them ALWAYS,
   sacrificing that cleanliness even when you don't want the coupling.
2. λ=0 WITHOUT CLAMP — a continuous cross-domain correlation is bounded-Gaussian → finite-quantile
   co-leaning only, weak/subtle. The AUDIBLE cross-domain effect (co-EXTREMITY: "melody slams high
   exactly when rhythm goes dense") needs CLAMP — at which point it's the cross-lane MO route already
   mapped (clamp + shared SINGULAR-part modulation), which needs NO standing correlation matrix.
3. STANDING vs OPT-IN — the decisive point. The clamp route gives the SAME cross-domain coupling but
   OPT-IN and TRANSIENT (domains independent by default; coupled only when you deliberately modulate
   their clamps/w together). A standing 3×3 factor is coupled ALWAYS — strictly worse: it spends the
   domain-independence invariant permanently for an effect you only sometimes want and can get
   on-demand. This is why CA set it aside: standing coupling < on-demand clamp coupling.

CONCLUSION: do NOT build the standing 3×3 cross-domain correlation. Cross-domain co-movement is better
served by clamp + shared-singular-modulation (opt-in, keeps domain independence as the default,
mechanism already in the theory). 7×7 cross-lane is even less justified (needs up-to-7 factors, PSD
projection, same λ=0-without-clamp weakness). The independent-streams architecture is a FEATURE here:
it makes cross-domain coupling opt-in-via-clamp rather than a standing cost.

────────────────────────────────────────────────────────────────────────────────
## THE CRISPEST STATEMENT — why only atoms give λ, + the two coupling routes by scope (Rodney)

### INTUITION PUMP: same marginal mass, spread vs atomic — only atomic is JOINTLY-OCCUPIABLE
Two ways to get "30% chance of output above 0.99", marginally near-identical:
- CONTINUOUS TILT (distribution params θ): 30% of mass SPREAD across (0.99, 1.0). Each lane lands
  there independently AT DIFFERENT EXACT VALUES (0.994 vs 0.997 — both high, neither EQUAL/locked).
- RAIL ATOM (clamp at 1.0): 30% of mass piled EXACTLY at 1.0 — a point mass.
Marginally the same (~30% very-high). CATEGORICALLY different jointly:
- Continuous: as the threshold → 1 (0.99 → 0.999 → …), P(both high) → P_i·P_j (the PRODUCT). It keeps
  FACTORING. That limiting product IS λ=0. A continuous tail is tail-independent NO MATTER how extreme
  you tilt it — and λ>0 is UNREACHABLE (no set two variables can hit EXACTLY together).
- Atom: {1.0} is a set of POSITIVE measure that two variables can occupy SIMULTANEOUSLY AND EXACTLY.
  That joint-occupiable set is the ENTIRE source of tail dependence — the lump at the (1,1) corner is
  what λ measures. An atom, however small, is the ONLY thing that can CARRY λ>0.
ESSENCE: tail dependence is NOT "how much mass is in the tail" (both have 30%) — it is the GEOMETRY of
the extreme (SPREAD vs ATOMIC). Spreading mass near 1 = "often high, independently"; piling mass at
exactly 1 = "often maxed, and COUPLABLE to other lanes being maxed". Same marginal, different joint
potential. (This is the intuition-pump form of the orthant-probability Phibar_2 math above.)

### THE TWO COUPLING ROUTES, SCOPED BY WHERE THE UNIFORMS ARE CORRELATED
Atom = the ENABLER (necessary for any λ>0, both cases). The COUPLING that makes atoms CO-OCCUR comes
from whatever correlates the rail-hits — and that differs by scope because of the stream architecture:
- CROSS-VOICE (within a lane): the voices SHARE the stream → real latent Gaussian-copula correlation
  (spread/CA). Pushing that correlation UP, WITH CLAMP, drives correlated voices across the rail
  TOGETHER — the clamp RECTIFIES existing ρ into atom co-occurrence. Strength tunable ρ=0 (independent)
  → ρ=1 (locked) via the correlation you already control. Works BECAUSE within-lane uniforms are
  correlated.
- CROSS-LANE: the uniforms are INDEPENDENT (domain-separated streams) → NO latent ρ exists to turn up
  → Gaussian-copula correlation CANNOT be the mechanism. The ONLY route is SHARED MODULATION OF THE
  ATOM PROBABILITY (shared w / clamp-point / singular-part mod): when the shared signal pushes, both
  lanes' atom masses grow/fire TOGETHER. Coupling = the SYNC of the shared modulation's effect on each
  lane's atom mass, NOT any draw correlation (there is none to use).
THE DIVIDING LINE IS FORCED BY THE ARCHITECTURE: shared streams within a lane → correlation+clamp
works; independent streams across lanes → only shared-atom-modulation works. (Within a lane,
shared-mod would ALSO work; cross-lane it is the ONLY option.)

ONE-LINER: Clamp provides the atom (the joint-occupiable set — necessary for any λ>0). The coupling
that makes atoms co-occur is whatever correlates the rail-hits: within a lane, the Gaussian-copula
correlation of the shared stream (clamp rectifies it); across lanes, where streams are independent,
ONLY shared modulation of the atom probability itself.

────────────────────────────────────────────────────────────────────────────────
## CLAMP BUILD-CASE VERDICT (Rodney 2026-10-22) — threshold-relativity demotes it; distribution-mod suffices

### THE DECIDING PRINCIPLE: an atom carries λ only against a threshold AT the atom (threshold-relativity)
λ is a LIMITING property ("as the decision threshold → the extreme"). So a rail atom at 1.0 creates
tail dependence ONLY when the operative musical decision tests for EXACTLY the rail (or a threshold
approaching it). Against a FIXED SUB-RAIL threshold it is indistinguishable from continuous mass in
the same region — MARGINALLY AND JOINTLY.
WORKED (Rodney): legato & accent both 5% base, both trigger on OUTPUT > 0.95, independent uniforms,
warp maps 30% of mass to output > 0.95. Joint P(both > 0.95) = 0.30×0.30 = 0.09 — THE SAME whether
the 30% got there by continuous warp OR by a rail atom at 1.0. Because 1.0 and 0.96 are BOTH just
"> 0.95" — the atom sits INTERIOR to the trigger region, not AT the boundary. The knobs can't tell
how the mass got above 0.95. (Corrects an earlier overstatement that the atom is "jointly essential"
— it is essential ONLY when the threshold coincides with the atom.)

### CONSEQUENCE: clamp is only worth building if a musical decision tests EXACTLY 0 or 1
The test for clamp's value: does dot.modular make (or want) a decision that fires on EXACTLY-the-rail
("hard max/min" events — "max density ONLY when variation is maxed", "both at the ABSOLUTE top") as
opposed to the threshold/high-low decisions it actually uses? If NO → the continuous warp already does
everything the thresholds can perceive, and clamp adds machinery (lo/hi knobs, singular branch,
Straits/Causeway layout) with NO audible return. If YES → build clamp, scoped NARROWLY to those
rail-tested decisions only.
HONEST READ: the instrument's decisions are "how likely / how high / how dense" (threshold + value-map),
NOT "exactly maxed". So clamp's build-case is WEAK. DISTRIBUTION MODULATION SUFFICES.

### STATUS: DEFERRED, NOT DISMISSED (Rodney — "experience says don't dismiss features")
Clamp stays documented + decided (control surface, copula type, DMO link — all filed) so it can be
picked up IMMEDIATELY if a rail-tested use appears. Demoted from "likely future direction" to "build
only if an exact-0/1 decision is specifically wanted." The MO/DMO/cross-lane theory remains a correct
DESCRIPTION of what clamp would do — it's just that what clamp does isn't something the instrument's
current decisions can see.

### WHERE THE MO TILT FITS (relative to this verdict) — it's the WIN, independent of clamp
The KwMO "MO tilt" R_p (PALETTE_KUMARASWAMY_TILT_NOTE.md) is CONTINUOUS distribution mod — it bends
mass TOWARD a rail via one hazard-tilt knob but creates NO atom (λ=0, monotone (0,1)->(0,1)). So it is
squarely in the "distribution-mod suffices" category: it delivers rail-ward shaping as CONTINUOUS mod,
palette-wide, cheap (one mul+div, p=0 bit-identical), WITHOUT needing clamp. And it is forward-
compatible — it's the clamp's MASS FEEDER if clamp is ever built — so shipping the tilt de-risks a
future clamp without committing to it. The tilt is the buildable embodiment of "distribution-mod
suffices, clamp deferred-not-dismissed".

────────────────────────────────────────────────────────────────────────────────
## FINAL ATOM/CLAMP RESOLUTION (Rodney 2026-10-22) — separate the MECHANISM (atom) from the PLACEMENT (rail)

### Tutti needs NO rail — continuous joint-skew delivers it
The "whole voice/ensemble lands at the extreme together" gesture does NOT need atoms. Jointly skewing
the relevant distributions toward the top (shared MO-tilt / shared θ, under correlation) gives a strong
CLUSTER of high values co-occurring = the tutti SOUND, as continuous mass, λ=0. The rail would only add
EXACT simultaneity at the identical value — and a listener CANNOT distinguish "all at 0.98" from "all at
exactly 1.0". Tutti is a cluster-at-the-top perception, not an exact-rail perception → continuous
joint-skew suffices, rail adds nothing audible. (Removes the last candidate internal use for rail atoms.)

### THE CRYSTALLIZED PRINCIPLE: musical perception is CLUSTER/THRESHOLD, never MEASURE-ZERO
An atom is positive mass on a MEASURE-ZERO set. That mathematical specialness (→ λ>0) has a perceptual
correlate ONLY if an explicit decision tests the EXACT point. The instrument's perceptions are all "how
high / how likely / how clustered / how dense" — threshold/cluster based, never "is it exactly this
measure-zero value". So atoms at perceptually-undifferentiated locations (the RAILS — extremes are just
"high/low") are perceptually INERT. This is WHY continuous distribution mod suffices: not as an
approximation to clamp, but because clamp's distinguishing feature is perceptually invisible here.
(Atoms matter in CREDIT because default IS a rail/boundary event; music has no such exact-boundary
decision.)

### SO: the rail-parametric CLAMP feature has NO internal application → NOT worth building
Every internal gesture that SOUNDS like it wants the rail (tutti, co-extremity, "everyone maxed") is a
cluster/threshold perception that continuous joint-skew delivers WITHOUT atoms. The only conceivable use
is EXTERNAL: a user patches a CV OUT into a downstream module that behaves differently at EXACTLY max (a
comparator at the rail, a precise clip stage) — rare, user-constructed, not an instrument need. Clamp
(parametric lo/hi rail mass + mod) stays DEFERRED-NOT-DISMISSED but with essentially no pull.

### BUT — atoms as a MECHANISM are useful at INTERIOR locations, and you ALREADY HAVE them (hand-drawn)
The atom is a MECHANISM; the rail is one PLACEMENT. The HAND-DRAWN curve editor can place a flat segment
— hence an atom — ANYWHERE, not just the rails. And an INTERIOR atom IS perceptually live on value lanes,
because the lane maps the uniform to a VALUE the listener TRACKS:
- MELODY: atom at a degree = a recurring exact note = an ANCHOR / PEDAL / motif. Musically strong
  (unlike the rail atom, which was just "lowest/highest").
- OCTAVE / QMIX: atom at a mid value = "keeps returning to this register / this exact mix".
- CO-OCCURRENCE works too: two within-lane correlated voices with a hand-drawn atom at the SAME degree
  LOCK onto that exact note TOGETHER (recurring unison on a specific pitch) — within-lane MO coupling via
  the flat, reachable through the editor, NO clamp feature needed.
This needs NO new feature — the drawn-curve pool already exists (CC_PROMPT_02). Interior atoms come FREE
from the editor you built.
CAVEAT: interior atoms carry the SAME Sklar/singular consequences as rail atoms (a flat = an atom = a
singular component = loss of copula uniqueness + rank-recoverability over that segment). The difference
is LOCATION (→ perceptual relevance), not math. Hand-drawn flats are interior atoms with the full
singular-part behaviour, placed where they're musically useful.

### THE DEFINITIVE STATEMENT
Atom MECHANISM: useful — INTERIOR, as recurring-value anchors + voices-lock-to-shared-anchor, and you
ALREADY HAVE it via hand-drawn curves. Rail-parametric CLAMP feature: unmotivated — extremes are
cluster-perceived, no exact-rail decision exists, so it adds machinery with no audible return. Not "atoms
are useless"; rather "the useful atoms are interior and already available; the clamp FEATURE specifically
is what's unmotivated."

────────────────────────────────────────────────────────────────────────────────
## THE ONE USEFUL ATOM FEATURE — injected interior atom on a parametric curve (Rodney 2026-10-22)

Supersedes BOTH clamp (rail atoms, perceptually inert) AND static hand-drawn atoms (interior but NOT
modulatable). This is the one place the mechanism, the perception, the modulation, and the coupling all
line up.

### THE GAP IT FILLS
- Static arbitrary shape incl. interior atoms → hand-drawn ✓ but NOT modulatable (a curve is a VECTOR
  of points — no scalar axis to route a CV at; drawn curves are a STATIC AUTHORING tool, inherently).
- Modulatable shape → parametric ✓ but can't place an ARBITRARY interior atom.
- Modulatable INTERIOR atom → NEITHER does it. ← this feature.
(The only clean way to modulate a DRAWN curve is A→B CDF-morph: resample both to a common u-grid and
lerp the OUTPUTS — monotone-preserving, Sklar-safe — but that is a heavier, separate thing. The injected
atom below is lighter and more useful.)

### THE CONSTRUCTION
Parametric quantile (Kumaraswamy etc. — fully modulatable via its own handles) PLUS one injected atom:
  Q(u) = parametric F^-1(u), EXCEPT a u-sub-interval of width m all maps to the single output ℓ.
TWO SCALAR HANDLES, both MODULATABLE (the whole point — scalars are CV destinations, a point-vector is not):
  - ℓ (LOCATION) — modulate it → the recurring anchor DRIFTS.
  - m (MASS)     — modulate it → the atom BREATHES (m=0 absent → dominant); m=0 = NEUTRAL = bit-identical
    to the un-injected parametric curve (house neutral-parameter rule, as MO-tilt p=0 / spread r=0).

### PROPERTIES
- MONOTONE-SAFE: inserting a flat into a monotone quantile stays monotone → Sklar structure intact; the
  output is the parametric continuous part + ONE Dirac at ℓ of mass m — a CONTROLLED Lebesgue decomp
  where you MODULATE the singular part's LOCATION and WEIGHT. (Clamp's "modulate the atom" capability,
  but for one INTERIOR atom instead of inert rail atoms.)
- PERCEPTUALLY LIVE: interior atom on a VALUE lane = a recurring exact value the listener TRACKS =
  anchor / pedal / motif (melody degree, octave register, qmix point). Unlike the rail atom (just
  "high/low", perceptually inert).
- CHEAP: a branch in invCDF ("u in the atom interval → return ℓ; else parametric F^-1 rescaled for the
  consumed mass m"). One compare + a rescale.
- MASS MODEL (a feel choice to pin when built): INSERT/DISPLACE (atom ADDS an anchor, pushes the rest of
  the distribution — "add a recurring note on top of my shape") — LEAN THIS; vs REPLACE (atom absorbs the
  local continuous mass near ℓ). Insert/displace reads as additive-anchor, the more musical default.

### THE COUPLING PAYOFF — performable voice-convergence-on-anchor
SHARED MODULATION of m across within-lane CORRELATED voices → the injected atoms grow TOGETHER → the
voices LOCK onto the shared anchor ℓ together, and you DIAL how strongly with a CV. "Voices converge on a
recurring unison note, modulatably." The within-lane MO coupling becomes a PERFORMABLE gesture — reachable
ONLY this way (static drawn atoms can't modulate the convergence strength). (Cross-lane still needs
shared atom-mass modulation per the independence rule; here that's literally sharing the m-modulation
across lanes' injected atoms — a clean, concrete realization of the cross-lane MO route, now with a REAL
modulatable m to share.)

### VERDICT
IF any atom feature is ever built, it is THIS — injected interior atom (ℓ, m) on a parametric curve — NOT
clamp. It is the only atom construction that is simultaneously perceptually relevant (interior),
modulatable (two scalars), cheap, neutral-at-m=0, and coupling-capable (shared-m → performable
convergence). Clamp (rail-parametric) remains unmotivated. Static hand-drawn interior atoms remain
available + useful for FIXED anchors (no new feature). This is the MODULATABLE upgrade, filed as the
atom feature to build IF the performable-anchor gesture is wanted. Not scheduled; sequenced after warp.

────────────────────────────────────────────────────────────────────────────────
## INJECTED-ATOM SCOPE — per-CONSUMER not per-lane; build on ALL lanes (Rodney 2026-10-22)

### CORRECTIONS
- QMIX IS BINARY (a which-source gate: quantiser vs seq, not a continuous blend). So qmix joins
  rest/accent/legato as a THRESHOLD lane — the injected atom's INTERNAL effect is inert there too
  (threshold-relativity: an atom not AT the threshold is marginally invisible; a binary decision has
  no exact value to recur). So the value lanes with a real INTERNAL anchor effect are just MELODY,
  OCTAVE, VARIATION.
- ℓ=0 or ℓ=1 are valid locations (no harm) → the injected atom at a rail IS a rail atom. So this
  feature SUBSUMES clamp entirely (clamp = this with ℓ pinned to a rail, non-modulatable). Another
  reason clamp-as-its-own-feature is dead: this does it and more, modulatably.

### THE SCOPE RULE: applicability is per-CONSUMER, not per-lane (same as warp's gate)
Every lane has TWO consumers of its uniform (the session's recurring "same uniform, two consumers"):
the INTERNAL decision, and the continuous probability CV OUT (random source).
- INTERNAL decision consumer: injected atom live only where the decision TRACKS AN EXACT VALUE →
  MELODY / OCTAVE / VARIATION (recurring anchor pitch / register / exact length). INERT on the
  threshold lanes REST/ACCENT/LEGATO/QMIX (binary — no exact value to recur; atom marginally invisible).
- CV-OUT (random-source) consumer: the CV out is CONTINUOUS on EVERY lane → the atom is live on ALL 7
  lanes (a recurring EXACT VOLTAGE = a usable quantized-CV modulation source downstream, regardless of
  whether that lane's internal decision is binary). An injected atom on ACCENT does nothing to accent's
  gate but makes accent's CV OUT emit a recurring exact voltage — a real mod source.

### THEREFORE: BUILD IT ON ALL 7 LANES (uniform, seam-free)
Rodney: "keep it on all lanes for random source." Correct — and seam-consistent with the whole
architecture: PLUMB uniformly (every lane gets the injected-atom capability), let the CONSUMER determine
the effect. NO boolean-lane carve-out (which would be a lane-is-special seam). Cost of all-lanes =
only the inert-on-booleans-INTERNALLY default (m=0-equivalent effect on those gates even when set —
harmless); the CV-OUT use JUSTIFIES the uniformity.
Summary:
  Lane      Internal-decision effect     CV-out effect
  MELODY    anchor pitch (strong)        exact-voltage source
  OCTAVE    anchor register (weaker)     exact-voltage source
  VARIATION anchor length/motif          exact-voltage source
  QMIX      inert (binary)               exact-voltage source
  REST      inert (binary)               exact-voltage source
  ACCENT    inert (binary)               exact-voltage source
  LEGATO    inert (binary)               exact-voltage source
PRINCIPLE: the injected atom is plumbed uniformly on all lanes; its INTERNAL effect is live only where
the decision tracks an exact value (mel/oct/var), its CV-OUT effect is live everywhere (all CV outs
continuous). Per-consumer, not per-lane — exactly warp's gate pattern. That is WHY "all lanes" is right
rather than "just the three".

────────────────────────────────────────────────────────────────────────────────
## MO TILT × INTERNAL RAIL ATOM — they COMPOSE (reinforce, same direction) (Rodney 2026-10-22)

Two distribution features that point the same way: MO tilt R_p bends CONTINUOUS density toward a rail
(p>0 → bottom, p<0 → top; no atom); the injected rail atom (ℓ=0 or 1, mass m) piles mass EXACTLY at
that rail. Composing them = a REINFORCED RAIL CONCENTRATION: the tilt makes the near-rail neighbourhood
dense, the atom adds an exact-rail point mass right in that dense region → a big atom + a heavy continuous
shoulder beside it, same direction, compounding. (This is the "tilt is the clamp's MASS FEEDER" relation,
now concrete.)

### WHAT IT OFFERS — per consumer (same per-consumer scope as the injected atom)
- CV OUT (all lanes, STRONG): a CV that LIVES at the max/min rail with frequent EXACT-rail hits. Tilt =
  how clustered near the rail; atom mass m = how often exactly AT it. Two knobs shaping a rail-dwelling
  CV source. Genuinely useful ("a signal pinned at the ceiling, spiking to exactly max").
- INTERNAL VALUE LANE (mel/oct/var, REAL): tilt-toward-top + rail-atom at ℓ=1 = "notes strongly favour
  the top AND frequently land on exactly the top note" — hover-near-the-ceiling + exact-top anchor/pedal.
  More specific than tilt alone (tilt = high common; atom = exact-top recurs).
- INTERNAL THRESHOLD LANE (rest/accent/legato/qmix, NOTHING NEW): threshold-relativity — tilt-toward-top
  shifts the firing RATE (marginal, knob-reachable → redundant-but-harmless); the rail atom is INTERIOR
  to a sub-rail trigger → invisible. Booleans see the marginal, never the atom. (Consistent with every
  prior boolean result.)

### DESIGN COUPLING: the interaction informs the atom's MASS MODEL choice
How the tilt's near-rail mass meets the atom depends on the injected-atom mass model (insert/displace vs
replace/absorb — flagged open in the injected-atom note):
- REPLACE/ABSORB: the tilt's near-rail density is partly absorbed INTO the atom → more tilt FEEDS a bigger
  effective atom. ONE control (tilt) grows the rail mass. This is the cleaner "tilt feeds the rail"
  behaviour and probably the more musical (one knob dialing rail-concentration).
- INSERT/DISPLACE: the atom ADDS exact-rail mass ON TOP of the tilt's cluster → two INDEPENDENT controls
  (tilt = cluster, m = exact-rail anchor), but risks near-degenerate "almost always at the rail" when both
  are high.
=> if "tilt feeds the rail" (one-knob rail concentration) is wanted, lean REPLACE/ABSORB; if independent
cluster + anchor is wanted, lean INSERT/DISPLACE. The two features' interaction is thus a factor in the
mass-model decision — pin together when either is built. They do NOT conflict either way (same direction);
the choice is only about whether tilt and m are coupled (absorb) or independent (displace).

────────────────────────────────────────────────────────────────────────────────
## DESCRIBING THE ATOM DISTRIBUTION — MO at the CORNER, comonotone INTERIOR; ℓ slides the diagonal (Rodney)

Our injected atom is authorable at ANY ℓ ∈ [0,1] — INTERIOR or CORNER. Both are the SAME mechanism
(a Dirac of mass m at ℓ) and the SAME decomposition TYPE; only the diagonal LOCATION differs, and that
location determines whether it is a tail object or a co-occurrence object. This refines the earlier MO
notes, which were all written for the rail/corner case.

### THE DECOMPOSITION (same form for interior OR corner)
Marginal: a MIXED distribution — NOT absolutely continuous. Lebesgue decomp:
    mu_Y = f_cont(y)dy  (abs-continuous: the tilted parametric density)  +  m·delta_ℓ  (singular: one
    Dirac of mass m at ℓ).
Joint (two within-lane correlated voices, atoms at the same ℓ): Gaussian-MA copula (continuous part)
+ a SINGULAR DIAGONAL POINT-MASS on {Y_i=ℓ, Y_j=ℓ}. The decomposition form is identical whether ℓ is
interior or corner.

### WHAT CHANGES WITH ℓ — the naming + the λ consequence
- ℓ = CORNER (0 or 1): the singular diagonal mass is at the EXTREME corner → MARSHALL-OLKIN. MO is
  SPECIFICALLY the joint-extreme/corner structure; it carries lambda>0 (tail dependence) — a TAIL object.
- ℓ = INTERIOR (0,1): the singular diagonal mass is at a NON-extreme point → NOT MO. It is a
  COMONOTONE INTERIOR ATOM (a co-occurrence atom): two voices land on the exact value ℓ TOGETHER, but
  lambda = 0 — tail dependence is a LIMITING property as you approach the extreme, and an interior atom
  (bounded away from the extreme) contributes nothing in that limit. So interior = FINITE co-occurrence
  at ℓ, NOT tail dependence.
=> Reserve "Marshall-Olkin" for ℓ at a rail. Interior ℓ → "interior comonotone atom" / "co-occurrence
   atom", lambda=0. The tell is lambda: >0 at the corner (MO), =0 inside.

### THE UNIFYING FRAME
ℓ SLIDES THE ATOM ALONG THE DIAGONAL. Corner → MO / lambda>0 / tail object. Interior → comonotone /
lambda=0 / co-occurrence object. Crossing to the corner is what TURNS it into MO. One mechanism, one
decomposition type, a continuum of diagonal locations; the musical meaning tracks ℓ:
- interior ℓ = recurring ANCHOR/PEDAL value, and (correlated voices) a mid-range UNISON on that value —
  the musically useful case (co-occurrence, no tail).
- corner ℓ = rail atom (MO, tail) — perceptually inert unless a decision tests the exact rail
  (CLAMP BUILD-CASE VERDICT) — so musically the weaker end, kept for completeness.
This is why the feel-test frees ℓ: interior is where the value is; the corner is the MO special case.

────────────────────────────────────────────────────────────────────────────────
## DECISION: FREE ℓ (atom anywhere), NOT corner-only — confirmed by feel-test (Rodney 2026-10)

Feel-tested live in the editor (tilt p + atom ℓ + atom m, three knobs): sweeping ℓ from interior
(0.56, 0.71 — sings a recurring anchor pitch) to the corner (1.00 — just "stuck at the top") CONFIRMED
by ear what the theory predicted. DECISION: the atom's location ℓ is FREE over [0,1]; we do NOT restrict
to corners (which is what a dedicated "clamp" feature would have been).

WHY FREE ℓ IS STRICTLY BETTER THAN CORNER-ONLY:
- CORNER-ONLY gives exactly TWO gestures (stuck-top / stuck-bottom), both perceptually blunt and both
  the MO case the clamp verdict demoted to "inert unless a decision tests the exact rail".
- FREE ℓ reaches the ENTIRE diagonal — a continuous family of recurring anchors at ANY value — and the
  USEFUL region (interior comonotone anchors) is the part corner-only could NOT reach at all.
- ONE mechanism, ONE parameter, SUBSUMES clamp: clamp = this atom with ℓ pinned to an endpoint. So we
  build ONE injected atom (free ℓ), not clamp + a separate interior feature. Consolidation that EXPANDS
  capability — fewer concepts, fewer controls, more range. The opposite of feature bloat.
- Upholds "distribution-mod suffices, don't dismiss features": the free-ℓ atom IS distribution-mod (a
  continuous shape control that includes the atom mechanism, placeable anywhere) — the expressive atom
  gesture WITHOUT dedicated clamp machinery and WITHOUT the rail-only perceptual deadness.
RECORD so nobody later proposes "just do rails, it's simpler": corner-only is both LESS expressive AND
not actually simpler (free ℓ is one knob either way). The corner (MO) case is retained as the ℓ=0/1
ENDPOINTS, for completeness, not as the feature.
