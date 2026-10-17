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
