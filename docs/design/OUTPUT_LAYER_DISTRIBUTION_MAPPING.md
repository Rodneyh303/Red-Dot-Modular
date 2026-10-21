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
