# SHARED RAIL MODULATION — the deterministic route to multi-lane extremes

STATUS: FILED 2026-10-23 — answers Rodney's carried point from the clamp/palette thread:
"We are forgetting modulation. Shared rail modulation to push lanes to extremes when its
desired." This note is a DESIGN REQUIREMENT folded into the pending clamp feature, NOT a new
engine feature. The clamp's lambda inputs do not exist yet (the clamp itself is not landed) —
the whole point of the note is that they must be designed patch-addressable FROM DAY ONE so
the patch can drive several lanes to the rails together. Nothing here schedules ahead of the
clamp (Rodney's sequencing ruling stands); when clamp implementation starts, its lambda-CV
design reads this note first. Awaiting Rodney's read before anything is treated as decided.

────────────────────────────────────────────────────────────────────────────────
## The gap being closed

Everything the palette/tilt/clamp thread filed is PER-LANE: the tilt bends one lane's
density toward a rail, the two-rail clamp gives one lane calibrated lambda_B / lambda_T
atom masses. Cross-lane structure today is EMERGENT ONLY — independent lanes multiply to
the product baseline (per-lane marginals compose as prod lambda_i for the joint), and the
filed scope ruling DECLINED to shape that product in the engine (scatter-inaudible; no
built-in cross-lane co-hit mechanism). That ruling was correct and stands. But it leaves a
real authoring hole: the patch cannot say "THESE five lanes all slam to their rails on this
envelope, NOW" — the single most obvious extreme-request a player reaches for. Emergent
products never reliably deliver it. Rodney's point names the missing surface: MODULATION.

## The route — one CV source, per-lane lambda inputs, correlation in the WIRING

One CV source fans out to the per-lane lambda modulation inputs of every lane the patcher
wants to move. Per lane l, at the eval point:

    lambda_B(l) = base_B(l) + sum_m  att_B(l,m) * cv_m      (all mods summed RAW)
    lambda_T(l) = base_T(l) + sum_m  att_T(l,m) * cv_m
    then the ONE feasibility projection below, then the closed-form solves
        eps_B = R_p(Q_base(lambda_B)),  eps_T = R_p(Q_base(1 - lambda_T))

- The sharing lives in the PATCH WIRING (same source, per-lane attenuators), not in any
  engine coupling. The engine stays per-lane and per-draw; each lane's clamp solve is
  untouched; no cross-lane state, no joint term, no product shaping. The declined mechanism
  stays declined.
- Per-lane attenuators GRADE the extremes: bass lane at full depth slams, melody lane at
  half depth leans. Graded participation is something a built-in coupling could never offer.
- "When its desired" is literal determinism: the marginal rail-hit probabilities track the
  shared CV script. Same CV -> same multi-lane extreme sequence, scrub/reverse-safe like
  every lambda in the mass model (lambda sets expected density; the RNG stream stays the
  clock).

## Scripted vs emergent — where this sits next to the copula

The two layers are orthogonal and compose:

- SHARED MODULATION scripts the MARGINALS over time: it moves lambda_i(t) of several lanes
  TOGETHER, deterministically, when the patch says so.
- THE COPULA shapes the JOINT given those marginals: within-lane co-hit texture via
  C(eps_B, eps_T) (the corridor eps^2 -> eps), run-clustering via r; across lanes the joint
  stays emergent at the product baseline — by filed ruling.
- The landed within-lane across-voices ruling is unchanged: the voices' existing copula
  propagates through the monotone clamp read; no new mechanism there either.

So: deterministic WHEN via patch wiring, stochastic TEXTURE via copula. Neither replaces
the other; the hole was that only one of them existed.

## The one hard requirement — feasibility under ANY modulation depth

The two-rail model's sole constraint (lambda_B + lambda_T < 1, joint authoring clamp
<= 0.9, the 1 - lambda_B - lambda_T leftover IS the non-rail material; the -> 1 limit is
the rank-destroying two-point Bernoulli hocket) must hold not just for knobs but for EVERY
modulated eval, per lane, per control tick. The projection (RECOMMENDED, feel-test
calibratable):

    per-rail ceiling first:  lambda_B, lambda_T <= LAMBDA_MAX (P_MAX-class, provisional)
    joint projection second: if lambda_B + lambda_T > LAMBDA_JOINT (0.9 provisional),
                             scale BOTH by LAMBDA_JOINT / (lambda_B + lambda_T)

- Proportional rescale preserves the rail RATIO the patcher authored — both rails keep
  their relative pull; the alternative (clamp the pushed rail only) makes the outcome
  order-dependent and silently discards headroom, the exact disease
  MODULATION_CLAMP_INVARIANT.md exists to prevent.
- This projection IS the "one end-clamp at the consumption point" of the house invariant,
  generalized to a two-component constraint: intermediate layers (CV reads, attenuverters,
  sums) return RAW; the lambda consumer projects once, then feeds the closed-form solves.
  No partial clamping anywhere upstream.
- Eval-side only (the clampR precedent from the P_MAX enforcement note): neither a knob
  nor a modulator can ever present the solve with an infeasible pair. NaN-free by
  construction.
- LAMBDA_MAX / LAMBDA_JOINT are PROVISIONAL constants of the same class as P_MAX = 0.95 —
  their feel-test derivation joins the ALREADY-QUEUED single listening session (P_MAX
  calibration + clamp mass calibration; the projection constants live and die with the
  same audible material).

## Patch sketches (what it unlocks)

- Envelope -> shared lambda_T, five lanes at graded depths: one gate = ensemble top-rail
  slam, bass first, melody leaning — authored, repeatable, reversible.
- Slow LFO -> shared lambda_B/lambda_T pair: the whole patch breathes toward its bottom
  rail and back, lanes in coherent waves — the "pushed to extremes when desired" arc.
- Gate sequence -> shared input with per-lane attenuverter polarity: lanes alternate rail
  pressure — hocketed extremes WITHOUT any engine-side coupling term.

## Scope fence (what this note does NOT reopen)

- NOT the declined built-in cross-lane co-hit shaper: no joint term, no product shaping,
  no engine-side coupling. The correlation is patch wiring or it does not exist.
- NOT DMO's joint-lambda calibration — that stays deferred territory per the filed
  "WHAT lambda IS NOT" list; shared mod moves per-lane marginals only.
- NO new module, no new jacks beyond what the clamp feature's own lambda inputs provide.
  If the clamp lands with per-lane lambda CV, this feature costs ZERO additional code.

## Sequencing

- RODNEY'S CALL STANDS: nothing schedules ahead of the clamp. This note rides the clamp's
  design phase; its constants join the queued calibration session.
- Cross-refs: PALETTE_KUMARASWAMY_TILT_NOTE.md (RAILS section — the two-rail model,
  P_MAX provenance, eval-side enforcement); OUTPUT_LAYER_DISTRIBUTION_MAPPING.md (the
  clamp half, DMO fence); MODULATION_CLAMP_INVARIANT.md (sum-then-project-once); the
  copula corridor (MA copula note) for the stochastic layer this complements.
