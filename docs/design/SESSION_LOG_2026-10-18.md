# SESSION LOG 2026-10-18 — Phase 3 design: warp storage, modulation, and what the copula can/can't do

Design-only session. No code changed. Outcome: Phase 3's storage architecture is settled, parameter
modulation is designed and scoped, the boolean-lane question is resolved by proof rather than by feel,
and three claims in OUTPUT_LAYER_DISTRIBUTION_MAPPING.md are corrected (two of them claims Claude made
earlier in this same session and then had to withdraw — recorded below so they don't get re-derived).

---

## 0. BRANCH STATE — the docs were describing work that wasn't on the branch

Found by `git ls-remote` + reading the code, not by trusting the handoff.

`lane-expander-refactor` has Phase 1 (Distribution.hpp + MonotoneCurve.hpp) and all the design docs.
It does NOT have the Phase 2 editor widget or Phase 2.5. Those live on two unmerged branches:

- `feat/dmcurve-widget-phase2` — the editor widget
- `feat/dmcurve-phase2-5` — contains the widget branch PLUS all of Phase 2.5

Phase 2.5 is DONE on that branch, verified in code not commit messages: `Distribution.hpp` declares 8
concrete classes with `Mobius` and `LogitSigmoid` present and `Arcsine` gone;
`MonotoneCurve::fromDistribution` exists; `DmcurveDevHost` has the generic param knobs.
Built `test_distribution.cpp` from that branch and ran it: **56,395 passed, 0 failed**, including the
Mobius and logit-sigmoid contract batteries. `git merge-tree` shows the merge into
`lane-expander-refactor` is conflict-free.

**ACTION OUTSTANDING: merge `feat/dmcurve-phase2-5` into `lane-expander-refactor`.** Phase 3 should
build on one trunk. A second unmerged layer is how this drift compounds.

(The handoff also cited `HEAD ~2789a923` while the handoff commit was `a3d044c` — stale on arrival.
This is exactly what the end-of-session handoff update is for.)

---

## 1. WHERE THE WARP PHYSICALLY GOES — the mirror

Insertion point: `PatternEngine::polyRandom(voiceSlot, engLane)[step]` → `random_[16][7][16]`, the final
post-everything (A/B-mix + spread + LOR) uniform. Everything upstream is the copula; everything
downstream consumes a uniform.

Three candidate shapes, and the project's OWN cacheability rule decides it:

`GaussianCopula.hpp` lines 37-46 state the rule — the LUT (`PhiLUT`) is for UNCACHEABLE hot
float-precision callers (slew readout's `applyZ`, 544x/window; spread's `mix2`); CACHED callers
(`PhiInv`, once per new draw) can afford the exact erfc `Phi`. The split axis is **cacheability**, not
hotness.

**The warp is a cached caller.** No-parameter-modulation (as originally scoped) means the distribution
is set-and-forget and the uniform only changes on a new draw or scrub. So:

- **DECIDED: `warped_[16][7][16]` mirror alongside `random_`.** The mirror IS the cache that makes the
  warp a cached caller.
- **DECIDED: exact double `invCDF` on the rebuild path.** No new numerical path. Distribution layer
  keeps calling exact `redDot::copula::PhiInv`, already covered by the 56,395-assertion battery. No new
  LUT to build, warm, validate for monotonicity, or check for reversal-neutrality.
- **DECIDED: double compute, float store** (Rodney's original plan). Consistent with `random_` already
  being float and the 1e-7 float-lane target behind `PhiLUT`.
- **DECIDED: scoped dirty MASK, not a bool.** A Macro edit is one lane across delegating voices
  (16x16 = 256 cells); a Sands-east edit is one lane, one voice (16 cells). NOT 1792 — that's the whole
  array, which only turns over on pattern regeneration. Macro rebuilds must SKIP currently-unlocked
  voices (they're on their own curve; nothing about them changed), so 256 is the no-unlocks worst case.
- Raw `random_` retained as the pre-toggle source. The pre/post toggle becomes two array selections.

Rejected: **in-place mutation** of `random_` (destroys the raw uniform, kills the toggle, non-idempotent
on pipeline re-entry). **At-read** (pure, and viable on cost — see below — but every consumer must
REMEMBER to apply the warp; any site reading `polyRandom()` directly silently gets unwarped values and
looks plausible doing it. That is the exact shape of the polyBankIndex bug. The mirror makes the warped
value the default and the raw value the explicit request). At-read remains a legitimate alternative IF
guarded by a contract test asserting every consumption site routes through the warp accessor.

Reversibility unaffected either way: warp is stateless on a Philox-addressable uniform, so scrub/reverse
regenerates and re-applies. The mirror is pure cache.

**Cost correction recorded:** Claude initially claimed ~10^5 invCDF calls/frame for display. Wrong by
~60x — the whole array is 1792 cells. Even exact `PhiInv` (~80ns: Acklam rational, 11 mults + a divide,
plus one Halley step costing an erfc and an exp) makes a full sweep ~140us. At-read was never a cost
problem. The mirror is justified on single-source-of-truth grounds, not performance.

**`PhiInvFast` should NOT be borrowed into the distribution layer.** It is a binary search over a
4097-entry double table (~12 dependent loads through 32KB, latency-bound) then a linear interp — against
a branch-light rational approximation with no table, it is plausibly SLOWER. Its reason for existing is
that forward spread and reverse traverse the identical table so bit-identity holds through `mix2` — it
is there for reversibility, not throughput.

---

## 2. THE VISUALISER NEEDS THE FORWARD CDF, NOT INVERSIONS

Rodney's framing was ~16x45 inversions/frame for voice x pitch-octave-and-variation targets. It's not
inversions.

With warp applied to the uniform AHEAD of the fader map, the faders partition [0,1] into intervals by
cumulative weight, bin k owning [c_{k-1}, c_k]. Target k fires iff F^-1(U) in [c_{k-1}, c_k], i.e.
U in [F(c_{k-1}), F(c_k)]. So:

    P(bin k) = F(c_k) - F(c_{k-1})

**Forward F at the fader boundaries. No inversion anywhere in the visualiser.** And it telescopes:
46 evaluations of F give all 45 masses, since each boundary is shared between adjacent bins.

Count: 46 CDF evals per DISTINCT (curve, fader-set) pair. Voices delegating to the Macro curve with
shared faders are one computation reused 16x. If melody faders are per-voice and independently
modulated, 16 x 46 = 736.

`Distribution` already exposes `cdf()` alongside `inv()`. Nothing new needed on the math side.

**Modulated curves kill the case for a sampled-CDF LUT.** A LUT wins when you evaluate a FIXED curve
many times. Here you evaluate a CHANGING curve few times — 46 boundaries is all the visualiser needs.
Building a 1024-entry table per frame to read 46 points from it is strictly worse; with per-voice theta
it's 16 tables/frame vs 736 direct evals. Direct exact evaluation at frame rate:

| workload                                   | per frame | per second | core  |
|--------------------------------------------|-----------|------------|-------|
| bar masses (16 voices x 46 boundaries)     | 736       | 44k        | ~0.35%|
| curve rendering (16 x ~200 points)         | 3,200     | 192k       | ~1.5% |

So **frame-rate modulation is affordable with exact PhiInv/Phi and no second numerical path.** `PhiLUT`
stays where it is (`applyZ`, `mix2`); the warp never needs one.

### 2a. Latched vs live theta on the display — DECIDED: show both

The engine latches theta at draw time; the visualiser runs at 60Hz. Between draws these differ, and at
slow clocks they differ for a long time.

- Display latched only: truthful about the current step, but a knob turned between steps appears dead.
  At 1Hz that reads as broken.
- Display live only: responsive, but the bars show a distribution that ISN'T in effect — user sees one
  thing, hears another. That is the failure class that cost weeks.

**DECIDED:** latched = solid bars (what's in effect); live = ghosted outline (what the next draw will
use). The gap between them IS the information — at fast clocks they converge and the ghost vanishes; at
slow clocks you watch the modulation load up before it fires. Makes draw-time latching legible instead
of a hidden implementation detail.

Architecturally: two consumers with different theta sources, and they must be NAMED as such in the code.
`warped_` is engine truth from latched theta; the visualiser path takes live theta and never reads the
mirror. Crossing them produces a plausible-looking display of the wrong thing.

---

## 3. THRESHOLD / BOOLEAN LANES — resolved by proof, not by feel

The handoff's provisional plan was "add warp to ALL lanes experimentally, FEEL it, peel back." That plan
is superseded. The answer is decidable.

Code convention (`SequencerEngine.cpp:603,1097`): `r_rest < v.restProb` — fires BELOW threshold.
`restProb` is per-voice, from expander knobs.

### Static threshold: warp is EXACTLY a knob move

    {F^-1(U) < t} == {U < F(t)}

Identical events on the same U — same realisations, step for step, not merely the same marginal. And
since the copula is untouched, the JOINT law of a boolean pair is fully determined by the two thresholds:

    {F1^-1(U1) < t1, F2^-1(U2) < t2} == {U1 < F1(t1), U2 < F2(t2)}

So the "rest+legato emergent gap distribution" blind spot is EMPTY under fixed thresholds — the gap
distribution is unchanged up to two knob offsets. Nothing to feel, because nothing different to hear.

### The nested-set invariant (Rodney's firing-example, resolved)

Rodney: "uniform draw 0.5 is below threshold but warp draw 0.6 is above — different events fire. But it
may be uniform's 0.6 that fire." Correct, and the resolution is his second half. Define

    S(c) := { s : U_s < c }

These are TOTALLY ORDERED BY INCLUSION: c1 < c2 => S(c1) subset S(c2). Steps enter in rank order of U_s
and never leave. Warp lands you at c = F(t) instead of c = t — a DIFFERENT link in the chain, hence a
different firing pattern. What it CANNOT do is produce a set OUTSIDE the chain: you can never get step 7
firing while step 3 doesn't when U_3 < U_7. No monotone F reaches it. Warp moves you along the nest; so
does the knob; they reach the same links.

Per-step/per-voice VARYING threshold escapes the chain (modulation does that on its own, warp or not):
{s : U_{v,s} < t_{v,s}} is not of the form S(c). With warp, {s : U_{v,s} < F_v(t_{v,s})} — the escape
route is reshaped.

### Modulated threshold: the pdf IS the gain profile

    P_fire(m) = F(t(m)),   t(m) = clamp(k + d*m)
    dP/dm     = d * f(t(m)),   f = F'

Uniform lane: P = t(m), dP/dm = d — **affine** in the CV. Two parameters, offset and gain. That is the
entire reachable family.

Warp: any **monotone** function of m. affine is a strict subset of monotone, so warp is NOT reachable
from knob + depth. Sensitivity to CV is base depth scaled by the density sitting at the current
threshold: where the warped distribution piles up mass a small CV move sweeps a lot of probability;
where it's thin you get a deliberate dead zone. (F(x)=x^2: P=t^2, dP/dm=2td — LFO does almost nothing
across the low half of its sweep then accelerates. Hockey stick vs flat ramp, same CV.)

### But it IS externally reachable, except per-voice

    F(k + d*m) == k' + d'*g(m)

so warp on a boolean lane is exactly a nonlinear shaper on the CV ahead of the threshold. Rodney:
"we could put waveshapers on polyphonic modulation." Correct — a poly-cable shaper applies ONE transfer
function across all channels, giving F(k_v + d*m) with shared shape, which Macro-global warp plus the
existing per-voice `restProb` knobs already matches. **So Macro-global warp on a boolean lane is
externally reachable. CONCEDED.**

What no standard VCV shaper does is carry SIXTEEN DIFFERENT transfer functions on sixteen channels.
That is the Sands-unlock case and only that.

### Subgates: settled

Rodney: subgates always test against rest from the main rest knob or the variation knob — FLAT threshold
across subgates. Then E[count] = J*F(t) vs J*t: a knob move. **Blind spot #3 is closed.** (If subgate
thresholds had been SPREAD across the ratchet, t_j = t + j*delta, warp would map them to F(t + j*delta)
and reshape the SPACING, which no setting of t or delta reaches. They aren't, so it doesn't.)

### DECIDED — what ships

1. **Warp applies to EVERY lane's uniform. One mechanism, uniformly applied.** The per-lane verdict
   falls out of what each CONSUMER does, not from a case-by-case lane decision. No "add to all, feel,
   peel back" — no peel-back needed.
2. Lanes that THRESHOLD internally are unaffected except via modulation response.
3. Lanes that MAP continuously (melody/octave via faders) get the full marginal effect.
4. **The CV OUTS are the irreducible consumer** — continuous, rank survives to the jack, no fader or
   threshold between the warped value and the output. A warped marginal there has no knob equivalent.
   Same uniform, two consumers, opposite verdicts: the redundancy was never a property of the LANE, only
   of the thresholding.
5. A boolean lane's modulation-response curve is a REAL but narrow feature (per-voice response curves).
   Ship it LATER as a response curve on the modulation input, under a different name and in a different
   UI location — a boolean lane has no distribution, it has a RATE. Calling it a "distribution" on rest
   invites the user to believe they're shaping rest-pattern character when the ordering invariant
   guarantees character is untouched. `.dmcurve` is already specified as a general-purpose monotone
   curve reusable anywhere, so this costs a wrapper once the curve stack exists.

---

## 4. PARAMETER MODULATION — IN, with scope

Originally scoped out. Rodney reopened it on the random-source argument and killed the objection:
**Monsoon's reversibility guarantee never covered modulation.**

Verified: `AB_MIX_COPULA_FIX.md:142-145` — backward = decrement the counters = exact regeneration, and
what buys this is keying by the POSITION COUNTER rather than drawing from running state. That is a
property of the DRAW STREAM. Nothing snapshots knob or CV state. Scrub back with an LFO at a different
phase and `restProb`, slew, spread and the faders all land elsewhere. So theta(t) adds no new class of
breakage. **Claude's reversibility objection was holding warp to a standard nothing else in the
instrument meets. Withdrawn.**

Better than parity: warp is **pointwise** — out_s = F^-1_theta(U_s), memoryless, no coupling across
positions. Slew modulation moves a moving-average window and genuinely entangles neighbouring draws.
Modulated warp is the most benign modulation class in the instrument.

### DECIDED

- **theta is LATCHED AT DRAW TIME**, output held across the step. Cost = lanes x voices per step (~112
  evals), nothing. Mirror rebuilds on draw as it already does. Reversibility sits in the same class as
  `restProb`. S&H character preserved.
  - Rejected for now: **continuous tracking** (output tracks theta BETWEEN draws, so a held value drifts
    as the curve sweeps under it). Per-sample evaluation, and it changes what the instrument is —
    stepped randomness becomes shaped drift. Wonderful on a CV out, wrong on pitch. Noted as a distinct
    future MODE, not entangled here.
- **Warp DEPTH w is the universal modulation handle:**

      out = (1-w)*U + w*F^-1(U),   w in [0,1]

  A convex combination of monotone increasing functions is monotone increasing, so Sklar survives for
  every w, and the pdf-as-gain result carries through with f replaced by the blended density. Works on
  the WHOLE palette including DRAWN curves (which have no theta to modulate — this solves the
  half-palette problem). One knob, one CV input.
- Parametric distributions may ADDITIONALLY modulate their native theta (closed-form subset).
- Depth sweep is the natural performance control for the random-source framing: sweep w and the
  marginals open from uniform to shaped across all lanes at once, copula untouched throughout.

### w scope — DECIDED: per voice, Macro-delegated default, Sands unlock

Static per-voice w adds no EXPRESSIVE power over per-voice F (the blend is itself a monotone curve you
could have drawn). **Modulation decides it:** w is a SCALAR, hence CV-addressable; a curve isn't. So
per-voice w is the only modulatable handle on per-voice warp depth — and per Section 6 that is what lets
shared modulation SUBTRACT dependence instead of only adding it.

Two things delegate/unlock INDEPENDENTLY, and the distinction is where the dependence consequences live:

- **LEVEL** (w_v's base value) — heterogeneity here is static, same factor, same sign. One-factor with
  unequal loadings. Useful, not structurally new.
- **MODULATION SOURCE** (which CV drives w_v) — THIS changes the copula class. Different sources per
  voice = genuine multi-factor; opposed polarity on a shared source = negative common-factor dependence.

If the existing Sands machinery delegates them together, the structurally interesting case is only
reachable as a side effect of unlocking for an unrelated reason. **Make them separately unlockable if
cheap** — a voice on the Macro curve and Macro level but its OWN mod source is a coherent, useful state.

**POLARITY MUST BE REACHABLE.** Negative common-factor dependence requires OPPOSED loadings. Either w is
bipolar, or the mod input is attenuverted per voice. If w in [0,1] with unipolar modulation only, the
subtractive case is unreachable and the whole heterogeneity result stays theoretical. One sign bit
decides whether that capability exists.

**w is a LANE PARAM, not curve data.** Poly, modulatable, latched at draw time, lives with `restProb`
and the faders. The curve stays the delegated object. Different lifetimes; only w is written on the
control path.

**Change Alley connection (UNVERIFIED — CA docs not read this session):** if CA routes which modulation
source reaches which voice's w, it is selecting the LOADING PATTERN on the common factors, which IS the
dependence structure. Block routing -> block dependence; scatter -> heterogeneous loadings; collapse ->
homogeneous. CA would stop being a modulation router and become a factor-structure router. **Confirm
against the CA design docs before relying on this.**

---

## 5. THE COPULA UNDER SHARED MODULATION — what it actually becomes

This is the deep result of the session, and it UPGRADES the doc's current claim.

### The class

With U_v = Phi(Z_v), Z ~ N(0,Sigma), and X_v = F_theta^-1(U_v):

    X_v = g_theta(Z_v),   g_theta := F_theta^-1 . Phi

**A common random monotone transform applied coordinatewise to a Gaussian vector.** The copula is

    C_mix(u) = INTEGRAL C_r( h_theta(u_1), ..., h_theta(u_n) ) dP(theta),   h_theta := F_theta . Fbar^-1

with INTEGRAL h_theta dP = identity (which is what keeps the margins uniform).

Formally a **one-factor copula** (Krupskii & Joe 2013, JMVA) with conditionally GAUSSIAN rather than
conditionally independent coordinates. **Not Archimedean in general** — the Marshall-Olkin/frailty
construction that generates Archimedean families requires conditional INDEPENDENCE given the frailty,
and conditional on theta the coordinates are still Gaussian-coupled at r.

(Name collision flagged by Rodney: the JOE COPULA is Archimedean, generator -log[1-(1-t)^theta]. Harry
Joe is the author of the factor-copula paper AND the eponym of the Joe family. Unrelated classes, same
surname. Cite as "Krupskii & Joe" the paper, never as a class label.)

### What P is

P is the law of the LATCHED theta at draw times — the sampling distribution of the shared modulation
value, one draw per step, common to all voices. The frailty mixing distribution. Concretely:

- Free-running LFO incommensurate with the clock: theta_s equidistributes per the waveform's occupation
  measure. Triangle/ramp -> uniform. **Sine -> arcsine on [-A,A]**, density 1/(pi*sqrt(A^2-x^2)), mass
  piled at the turning points. (Arcsine re-enters as the MIXING law right after being removed from the
  palette.)
- External random CV / S&H: that signal's marginal.
- Clock-synced LFO: deterministic and periodic; P is discrete on the cycle's values and successive steps
  are NOT iid.
- Hand on a knob: nonstationary, no fixed P.

### The r = 0 Archimedean case (Rodney's catch)

Claude's "not Archimedean" was only conditionally true. At r = 0 the construction collapses into exactly
the Marshall-Olkin mixture form, and for ONE palette member it is Archimedean on the nose.

`PowerDistribution`, F_theta(x) = x^theta, is multiplicative in cumulative hazard:
x^theta = exp(-theta * (-log x)) — precisely the frailty form. Then

    H(x,y) = INTEGRAL (xy)^theta dP = M_P( log x + log y )

with margins M_P(log x), where M_P is the mixing law's MGF. Standardising u = M_P(log x):

    C(u,v) = M_P( M_P^-1(u) + M_P^-1(v) )

**Archimedean, generator phi = M_P^-1 — the inverse MGF of the modulation source's amplitude
distribution.** With theta gamma-distributed on (0,inf) that is the classical gamma-frailty route to
Clayton, lambda_L = 2^(-1/alpha) > 0. The only thing standing between Monsoon and genuine lower tail
dependence is that a knob range is BOUNDED while gamma isn't.

Three consequences worth recording:

1. **The copula CLASS depends on the Spread knob.** r = 0 with a multiplicative-hazard marginal gives
   Archimedean; r > 0 gives a one-factor copula with conditionally Gaussian coordinates. Spread moves
   between dependence CLASSES, not just between correlation values.
2. **Palette members are NOT interchangeable here.** Only `PowerDistribution` has the hazard-
   multiplicative form giving a clean generator. Kumaraswamy, logit-normal etc. give Marshall-Olkin
   mixtures with no closed-form generator. "Modulate the parameter" means something structurally
   different depending on which distribution is loaded.
3. **M_P^-1 makes the mixing law load-bearing.** A triangle LFO's uniform occupation measure and a
   sine's arcsine law give different GENERATORS, hence different dependence. P is not a footnote.

### Multiple shared sources (w + native theta + ...)

**Dimension of Theta does NOT add factors.** With homogeneous warps every voice loads identically on the
whole shared vector Theta_s = (w_s, theta_s, ...), so it is still ONE factor — one common shock, one
loading pattern. Extra dimensions buy a richer family of random functions g_Theta (shape can vary
independently of location, where a single theta forces lockstep), changing how association varies across
quantiles, not how many factors there are. **Genuine multi-factor needs DIFFERENT LOADINGS PER VOICE.**

**w gates theta multiplicatively:**

    mu(Theta) = E[X | Theta] = 1/2 + w*( m(theta) - 1/2 ),   m(theta) = INTEGRAL F_theta^-1

so the common-factor covariance contribution is Var( w*(m(theta) - 1/2) ) — NOT additive. At w = 0 the
warp is identity for every theta and theta's factor vanishes entirely. So **shared w modulation is a
dependence-STRENGTH control**: sweep it and the common-factor dependence rises and falls over time. A
non-stationary copula with rho untouched. No analogue in the existing controls.

**The symmetric-warp case — a DISPERSION factor with no mean co-movement.** If the warp is symmetric
(m(theta) = 1/2) then mu(Theta) = 1/2 identically and the location factor is identically zero — but
Var(X | Theta) still moves with w:

    X_v ~ 1/2 + sigma(Theta) * eps_v

A bounded stochastic-volatility structure. Voices' SPREAD co-varies while their CENTRES don't. Pearson
and Spearman see essentially nothing; quadrant / co-extremity measures see it clearly. Musically: all
voices get wild together and tame together without drifting up or down together.

**So curve SYMMETRY selects between two qualitatively different common factors** — asymmetric warps give
location co-movement, symmetric warps give dispersion co-movement — and w scales both. Richer control
surface than nu would have been, at no extra cost.

**TODO (devhost):** add a QUADRANT-ASSOCIATION readout. Spearman alone will not show the dispersion
factor and it will look like nothing is happening.

---

## 6. SIGN OF THE DEPENDENCE — and the leader-follower structure

Verified in code before answering: `mix2` is **LEADER-FOLLOWER ONE-FACTOR**,
z_v = rho*z_L + sqrt(1-rho^2)*xi_v, with rho<0 giving the mirror (rho=-1 => exactly 1-leader).

**Consequences (Claude's earlier equicorrelation/PSD worry does NOT apply):**

- No PSD constraint. Every rho in [-1,1] is admissible for any number of voices. The
  r >= -1/(n-1) equicorrelation bound is irrelevant to this construction.
- **Corr(follower_i, follower_j) = rho^2 >= 0, ALWAYS**, whatever the sign of rho. Negative spread
  anti-correlates each voice with the LEADER while the pack moves TOGETHER. At rho=-1 every follower is
  exactly 1-z_L, so followers are perfectly correlated with each other. Musically: contrary motion of a
  BLOCK against a leader, not voices scattering.

### Law of total covariance — the sign result

    Cov(X_i, X_j) = E[ Cov(X_i,X_j | theta) ]  +  Cov( mu_i(theta), mu_j(theta) )

With shared theta and the SAME warp on every voice, mu_i = mu_j = mu, so the second term is
Var(mu(theta)) >= 0. **The mixture contributes POSITIVE dependence to every pair.**

So under negative spread the two controls act ASYMMETRICALLY: modulation ERODES the leader-follower
anti-correlation and REINFORCES the already-positive follower-follower correlation. Push modulation hard
enough and unconditional leader-follower covariance can cross zero while the pack tightens.

**Structural statement: frailty mixtures with homogeneous loadings are PQD — they can only ADD positive
dependence. Negative dependence is reachable only through the latent Gaussian layer. Modulation spans
the positive side only.**

Cleanest framing: a **TWO-FACTOR copula** — the leader is factor one with SIGNED loading rho; theta is
factor two with a loading that is unsigned-positive by construction. (Which explains why r=0 was where
the Archimedean case appeared: kill factor one and the frailty factor is all that's left.)

### Where it breaks — heterogeneous warps

The >= 0 depends on HOMOGENEOUS warps. With Sands unlock, voice i can take a curve whose mass rises with
theta while voice j takes one where it falls — mu_i' > 0, mu_j' < 0 — and then

    Cov( mu_i(theta), mu_j(theta) ) < 0

**Negative common-factor dependence from ONE shared modulation source.** Not reachable by any rho,
because the latent layer gives one signed loading shared by all followers, whereas opposed warps give
PER-VOICE SIGNED LOADINGS on the modulation factor.

This is the same irreducibility that surfaced FOUR times this session — poly shapers, boolean response
curves, negative common-factor dependence, and w's modulatability — all landing on per-voice
differentiated warp. **Sands unlock is the feature; Macro-global warp is the convenience.**

### At rho = 1 — w cannot touch the dependence, and modulation runs BACKWARDS

`mix2` returns the leader exactly, so every voice shares one uniform U. Then X_v = g_v(U) with g_v
increasing, and

    P[X_1<=x_1, X_2<=x_2] = P[ U <= min(g_1^-1(x_1), g_2^-1(x_2)) ] = M( F_1(x_1), F_2(x_2) )

**The copula is M, the Frechet upper bound, for ANY w_v and ANY F_v.** Already at maximal dependence; a
monotone per-coordinate transform cannot move it.

What per-voice warp changes instead is the MARGINALS while lockstep holds. All voices still rise and
fall together, rank-identically, but each occupies a different register or density. rho=1 + homogeneous
warp = UNISON; **rho=1 + heterogeneous warp = PARALLEL VOICING** — fixed harmonic relationships moving
as a block. Genuinely useful musical mode, not obvious from the controls.

**And modulation at rho=1 DECORRELATES.** With shared Theta and heterogeneous warps the unconditional
joint is INTEGRAL min(a_Theta, b_Theta) dP against margins INTEGRAL a_Theta dP and INTEGRAL b_Theta dP.
Since min is CONCAVE:

    INTEGRAL min(a,b) dP  <=  min( INTEGRAL a dP, INTEGRAL b dP )

strictly whenever a_Theta - b_Theta changes sign across Theta. So the unconditional copula is STRICTLY
BELOW M.

Mirror of everything upstream: at rho<1 shared modulation with homogeneous warps ADDS positive
dependence; at rho=1 there is no room above, so heterogeneous modulation can only SUBTRACT. Same
mechanism, opposite sign, determined by position relative to the Frechet bound. Musically: a lockstep
ensemble that LOOSENS as modulation deepens and tightens back to parallel motion as it shallows, with
Spread pinned at maximum throughout. A second, independent grip on ensemble tightness.

---

## 7. TAIL DEPENDENCE — not reachable, and that's fine

Three claims were made and withdrawn during this session. Recorded so they are not re-derived.

### WITHDRAWN #1: "shared modulation gives real tail dependence"

Too strong. The canonical frailty result (gamma -> Clayton) needs the mixing variable acting
MULTIPLICATIVELY ON THE HAZARD with theta on (0,inf) and an UNBOUNDED mixing law. Here theta lives on a
compact knob range and every palette member has full support on (0,1) for all parameter values. When all
F_theta share support and theta is bounded, F_theta(x) -> 1 uniformly as x -> 1 and the asymptotic
coefficient collapses to lambda = 0.

### WITHDRAWN #2: "modulating a truncation point is the route to lambda > 0"

Wrong. The bound covers it. With Cbar the Gaussian survival copula, its diagonal is regularly varying:

    Cbar(s,s) ~ s^kappa * L(s),    kappa = 2/(1+r) > 1 for r < 1

With G(x|theta) = P[X > x | theta] and G_max(x) := sup_theta G(x|theta):

    INTEGRAL G^kappa dP  <=  G_max^(kappa-1) * INTEGRAL G dP
    =>  lambda_U  <=  G_max(x)^(kappa-1)

If every F_theta is CONTINUOUS then G(x|theta) -> 0 for each theta, so G_max -> 0 and **lambda_U = 0.
No choice of P rescues it**, because kappa > 1 is fixed by the Gaussian copula. Truncation or affine
rescaling to [lo(theta), hi(theta)] leaves the conditional laws continuous — lambda_U = 0.

Intuition: at the very top, every still-reachable theta regime has x deep in ITS OWN tail, so the
Gaussian copula governs within each regime, and mixing tail-independent regimes gives tail independence.

For r < 0 the bound only TIGHTENS: kappa = 2/(1+r) > 2.

### WITHDRAWN #3: "nothing to feel on boolean lanes"

Right for FIXED thresholds, wrong for MODULATED per-voice thresholds. See Section 3.

### What WOULD give lambda > 0: an ATOM

Give F_theta an atom of mass a(theta) > 0 at x*. Then G_max does not -> 0, the bound stops biting, and

    lambda_U = INTEGRAL Cbar(a,a) dP / INTEGRAL a dP  >  0

No modulation required — holds for fixed a. On the curve the atom is a **FLAT SEGMENT**, not a free
endpoint:

- F^-1(1) = 0.8 with the curve strictly increasing -> support TRUNCATED to [.,0.8], continuous,
  lambda_U = 0. Pure rescale.
- F^-1(u) = 0.8 for u in [0.9,1] -> ATOM of mass 0.1 at 0.8. lambda_U > 0.

**But it is co-CLIPPING, not tail dependence in the useful sense** — both voices pinned at the same
ceiling value in the same step, and the atom returns the IDENTICAL value every time (stuck unison on
melody). Sklar UNIQUENESS fails with atoms, so the clean "copula untouched, marginals shaped" story needs
a caveat the moment flat segments are legal.

No parametric palette member can get there: all have fixed support on (0,1) or [0,1] and are continuous
for all parameter values, so G_max -> 0 always.

---

## 8. STRICT INCREASE — the editor constraint (DECIDED)

Rank preservation needs F^-1 **STRICTLY** increasing. Flat segments are the ONLY thing in the editor that
breaks it. Everything else a user draws — steep, shallow, S-shaped, asymmetric, lumpy — is strictly
increasing and sits entirely inside the clean Sklar story. **Drawn curves are saved; one degenerate
corner of their input space is excluded.**

**DECIDED: editor invariant changes from MONOTONE NON-DECREASING to STRICTLY INCREASING with minimum
slope epsilon.** Enforced at control-point level: y_{k+1} - y_k >= epsilon*(x_{k+1} - x_k), with the drag
handler clamping against it so a user dragging a point flat gets RESISTANCE at near-flat rather than an
error. Same feel as the existing monotonicity clamp. PCHIP cooperates: monotonicity-preserving on
monotone data, and strictly increasing control points with Fritsch-Carlson limiting give a strictly
increasing interpolant.

**epsilon ~ 1e-3.** Small enough that the steepest usable curves are unaffected, large enough that float
storage cannot collapse a segment to ties.

### Follow-ons

- **Float-store property test:** with epsilon enforced in double, the minimum output gap across a 16-step
  draw stays well above float resolution, so `warped_` as float preserves STRICT ordering. Assert
  **strictly increasing**, not the weaker non-decreasing. (Earlier "ties are harmless rounding noise" was
  too casual — ties are atoms, and atoms have dependence consequences.)
- **Audit the Phase 2.5 palette for parameter values producing NEAR-FLAT regions numerically.**
  Logit-sigmoid with heavy tails is the likely candidate (extreme parameters flatten the middle). Any
  member that can go flat within its parameter range needs the same epsilon floor, or the invariant holds
  in the editor and LEAKS through the palette.
- **`MonotoneCurve::fromDistribution` must land on strictly increasing control points.** It will if the
  source is strictly increasing and sampling is fine enough — ASSERT it rather than assume; it is the one
  path where a palette curve becomes editor data.
- **Check the existing battery for any assertion that empirical rank correlation matches r.** That
  assertion is already false the moment flat segments are legal, independent of the clamp.

---

## 9. OUTPUT-STAGE CLAMP — deferred, and mostly redundant anyway

Investigated as the explicit route to co-clipping. Result: reachable from existing controls on every lane
except one.

**Fader-mapped lanes: EXACTLY reachable.** Clamp at hi falling inside bin K gives masses m_k unchanged
for k<K, zero above K, and m_K' = (hi - c_{K-1}) + (1 - hi) = 1 - c_{K-1}. Now set faders above K to zero
and w_K' = w_K + SUM_{j>K} w_j: total unchanged, every m_k for k<K survives, and
m_K' = (c_K - c_{K-1}) + (1 - c_K) = 1 - c_{K-1}. **Identical — exactly, and independently of where in
the bin hi sits.** So on melody and octave the clamp is a FADER SETTING.

**Thresholded lanes:** clamping below a fixed threshold is a threshold shift. Redundant.

**The CV OUTS are the only irreducible consumer** — same conclusion as the warp itself.

**Two features usually conflated, and they want SEPARATE controls:**

- **AFFINE RANGE** — out = lo + (hi-lo)*x. Strictly increasing, no atoms, Sklar intact, rank intact,
  every doc claim untouched. This is what most people mean by "limit the range". Cheap and
  uncontroversial; if anything ships here, this is it.
- **HARD CLAMP** — clamp(x, lo, hi). Atoms at the rails, lambda_U > 0, rank destroyed at the edges, and
  Spearman/Kendall drift AGAINST the Spread knob's calibration (ties drag both toward zero while
  co-clipping INCREASES — two dependence measures moving in opposite directions under one control).

Soft saturation is strictly increasing and behaves like affine here — no atoms, no co-clipping.

**DECIDED: DEFER the hard clamp.** If built: stateless, pointwise, **CV-out path only**, downstream of
the mirror (no dirty flag, no cache interaction), parameters as LANE PARAMS and **NOT in `.dmcurve`** —
that file stays a pure monotone curve reusable anywhere. Reversibility inherits free. The visualiser
answers the invisibility objection: an atom is illegible as a flat curve segment but perfectly legible as
a spike at the rail.

Note what "breaks Sklar" means precisely: EXISTENCE survives (every joint law has a copula); UNIQUENESS
fails (the copula is pinned only on Ran F_1 x ... x Ran F_n, which has gaps). The GENERATIVE claim is
unaffected — you still draw U ~ C and apply g coordinatewise. The DESCRIPTIVE claim acquires a caveat:
clamping destroys rank information, so you cannot read the copula back off the output.

---

## 10. t-COPULA — rejected, with the real reason

Rodney: "professionally I found it difficult to deal with numerically. Also we excluded Beta due to
computational cost."

**Being fair to it first.** Construction: W_s ~ chi^2_nu per step shared across voices, T_v =
Z_v/sqrt(W_s/nu), U_v = t_nu(T_v). Marginals exactly uniform, Sigma untouched, nu->inf recovers Gaussian,
one extra scalar per step, Philox-addressable so reversibility survives.

- **The professional pain mostly does NOT transfer.** Most of it is CALIBRATION and TAIL MEASUREMENT —
  estimating nu against a famously flat likelihood, then evaluating the incomplete beta at extreme
  arguments where conditioning falls apart. Here nu is a KNOB (no estimation) and t_nu is only ever
  evaluated FORWARD, on a grid, at set time. The easy direction of the hard function.
- **The Beta precedent cuts IN ITS FAVOUR.** The doc did not reject Beta on cost — it rejected it because
  Kumaraswamy gets the same shapes more cheaply, while explicitly blessing the escape hatch: build the
  LUT at set time, lookup forever, runtime does zero Beta math. nu is set-and-forget, so a t_nu LUT
  rebuilt on knob change is exactly that pattern.

**What actually kills it: heavy tails are incompatible with a saturating fixed-grid LUT.**

`PhiLUT` works because Phi SATURATES — 4096 entries over z in [0,6], max error 9.3e-8, clamp to 0/1
beyond |z|>=6 with truncation error 2e-9, invisible.

t_nu has POLYNOMIAL tails. For nu=3, P(T>6) ~ 5e-3. Clamping there pushes ~1% of draws to exactly 0 or 1
— **ATOMS AT THE RAILS**, precisely the pathology Section 8 designs out of the editor. Uniformity gone,
Sklar uniqueness gone, stuck unisons.

To get nu=3 truncation to 1e-7 you need |t| ~ 200-300. A uniform 4096-point grid over [0,300] gives step
0.073, so only ~40 points cover [0,3] where essentially all the mass lives — interpolation error orders
of magnitude worse than the 1e-7 float-lane target. You would need a non-uniform grid, probability-space
spacing, or a piecewise scheme, each rebuilt per nu, each needing its own monotonicity and
reversal-neutrality proof. And `PhiLUT` is load-bearing for `applyZ`.

**REJECTED.** (The handoff's existing one-line reason — "t-quantile unstable at high df" — is not the
real reason and should be replaced by the above.)

---

## 11. BERNOULLI COMONOTONE REGIME — proposed, then rejected

Offered as the cheap route to genuine lambda > 0. Per step draw B_s ~ Bernoulli(p) from Philox; if B_s=1
all voices share one uniform, else the usual Gaussian copula at rho. Then

    P[U_1>q, U_2>q] = p(1-q) + (1-p)*Cbar_rho(1-q,1-q)
    lambda_U = lambda_L = p

Exact tail dependence in both tails, marginals exactly uniform (a mixture of uniforms is uniform), one
Bernoulli and a branch, reversible by the position counter.

**REJECTED on three counts:**

1. **It is not what a t-copula does.** The spike step makes all 16 voices share ONE IDENTICAL uniform —
   every voice on the same degree, same octave, resting together. A TUTTI UNISON. The t gives GRADED
   co-extremity (voices lean high together while staying distinct); the Bernoulli gives IDENTITY. Same
   lambda, completely different musical object. It also flattens per-voice warp differentiation on those
   steps — the one capability the session kept identifying as irreducible.
2. **It breaks the exact-correlation claim.** `GaussianCopula.hpp` opens by stating the correlation is
   the dot product of the weight vectors — analytic, no fitting, exactly specified. With the mixture,
   realised follower-follower rank correlation becomes **rho_S = p + (1-p)*rho^2**, so Spread at zero no
   longer gives independence; it gives p. The knob stops meaning what the header says. Same failure mode
   as the clamp eroding Spearman, but hitting a HEADLINE invariant.
3. **It punches a hole in the MA structure.** Correlation across steps comes from overlapping weight
   windows; substituting a shared uniform at step s breaks the MA autocorrelation at exactly that step,
   and the OU bridge and slew readout both assume that smoothness. A spike step gets slewed into —
   smeared or jumped depending on slew setting. Neither is designed behaviour. Plus a branch in `applyZ`.

**If tutti hits are wanted — and a sudden collapse to unison is a strong musical gesture — build them as
an explicit MUSICAL feature with their own name and probability control, applied AFTER the copula as a
deliberate override** (where the discontinuity is the point). Same sound, exact-correlation guarantee
intact, MA structure unperturbed, and the user understands the knob.

### The knife-edge lesson (recurring theme of the session)

Soften the spike regime from comonotone to Gaussian at rho=0.99 and lambda drops back to **ZERO** —
every Gaussian copula with rho<1 is tail-independent. Perceptually nothing changed.

**lambda > 0 is a knife-edge limit property, achievable only by exact comonotonicity or atoms, and
entirely inaudible.** The thing actually wanted — voices slamming together more often than Gaussian
allows — is a FINITE-QUANTILE effect, already delivered by shared-theta modulation.

---

## 12. NET POSITION

**What the layer gives, stated accurately:**

- Fixed warp: reshapes marginals per lane, copula EXACTLY invariant, reversibility intact.
- Shared-theta modulation: a genuine one-factor / frailty mixture on top of the Gaussian copula.
  The unconditional copula is NOT C. Elevated dependence at AUDIBLE quantiles. Co-spiking is real.
- At r=0 with `PowerDistribution` it is literally Archimedean with generator M_P^-1.
- Per-voice heterogeneous warps/loadings: multi-factor structure, and the only route to NEGATIVE
  common-factor dependence.
- At rho=1: parallel voicing, and modulation DECORRELATES.
- **lambda = 0 throughout.** No tail dependence, and it is not reachable without atoms, which are
  excluded by design. This is fine — lambda is inaudible.

**Pitch language:** "common-factor dependence", "co-spiking at audible quantiles", "a copula with
user-chosen marginals". **NOT "tail dependence".** A reader who knows the frailty literature will check
it and the claim will not survive.

**Honest three-tier framing for the manual:**
1. Inside Monsoon: uniform marginals, position-addressable draws, Gaussian copula, reversible.
2. Shared EXTERNAL modulation of the warp: adds a common factor, richer dependence, still reversible in
   the sense Monsoon guarantees.
3. Patch the outs back into the warp inputs: a DYNAMICAL SYSTEM. Fun, yours, not guaranteed. Keep
   feedback EXTERNAL and patchable — never internal — so the shipped guarantees hold and the user who
   patches the loop has visibly chosen to leave the guaranteed regime. (Hazard: positive feedback can
   drive the marginal to concentrate, driving theta further the same way, until the "random" source emits
   a near-constant. Bounded amplitude, dead output. Depth limit or inverted polarity avoids it.)

---

## 13. STILL OPEN — blocks CC

**`Distribution` ownership: value variant vs `unique_ptr`.** Modulation made this concrete and urgent.
The structure now holds THREE lifetimes:

- a per-voice modulatable SCALAR (w) read every draw, written on the control path
- a per-voice CURVE OBJECT read on mirror rebuild
- a Macro-delegated DEFAULT behind both

and the scoped dirty mask keys off all three. Macro scope is up to 7 lanes x (1 global + 16 per-voice) =
119 instances. `unique_ptr` means allocation during modulation — which points at a VALUE VARIANT, but
**read the Sands/Macro delegation code before committing.**

Also unresolved: **`melodySource[16][16]` is written in four places and read in NONE outside persistence.**
The degree mapping does not consume it — something reads `polyRandom(v, PL_MELODY)` directly, or
melodySource is a display/persistence mirror that has drifted. Since faders ARE a discrete inverse-CDF
(widths = mass), the composition faderMap . F^-1 is where melody warp actually lands. **Pin this before
designing that seam** — do not design it on a guess.

**UNVERIFIED this session:** Change Alley routing docs (the factor-structure claim in Section 4).
