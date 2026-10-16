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
