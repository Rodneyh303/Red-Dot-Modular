# CC PROMPT — VARIATION centre: knob 0.5 = repeat-in-place (no forward creep)

A small, self-contained gate-mode fix. Variation stays COUPLED to direction (NOT decoupled);
only the CENTRE behaviour changes. Reference: `DESIGN_POSITION_UNIFICATION.md` → DRIVE DIRECTION
MODEL → "VARIATION — keep it coupled to direction; fix the CENTRE".

## CARDINAL RULE (non-negotiable)
VERIFY AGAINST CODE AND TESTS, never a plausible narrative. Prove the fix with a test that FAILS
before and PASSES after. No Rack here — mechanical only; Rodney feel-tests.

## BRANCH
Off `fix/macro-voicelane-shared-frame`. This is independent of the position-resolver migration
and of the variation-scan DISPLAY branch (`fix/variation-scan-display`) — it can land on its
own, and it makes the restored VAR chevron sit still at the centre (consistent with that branch).

## THE BUG (verified in code)
In GATE mode `variationProb = (2*knob − 1) + CV2 + Junction`, clamped [−1,+1]
(`MonsoonModeController.cpp:160` — knob **0.5 → p = 0**, "the by-design stutter"). At p = 0 the
engine correctly HOLDS (no advance). BUT on every held rise the HOLD branch calls `scanHold_()`
(`SequencerEngine.cpp:~1908` — "HELD: scan the window forward, the freeze fix"), which grows
`scanOfsV_[0]`. Because |p| = 0 the freeze-fix escape never comes (escape ≈ 1/|p| held rises =
∞), so `scanOfsV_` grows without bound and the VARIATION lane / chevron **creeps forward
indefinitely** at the neutral centre — the arbitrary-forward Rodney observed. (`stepAdvanceScanDelta`,
`:2794`, returns `±scanOfsV_` → the chevron follows the creep.) The forward bias is baked in by
`decideStepAdvance_`'s `signedDelta = (p < 0) ? −delta : +delta`, which lumps p == 0 with forward.

## THE FIX (minimal, surgical)
At the neutral centre there is no escape to scan toward, so the scan must not run:
- **Gate the scan accumulation on `p != 0`.** In the HOLD branch, call `scanHold_()` only when
  the resolved mono `variationProb != 0` (a true-zero test; see the epsilon note). At p == 0:
  HOLD with NO scan — `scanOfsV_[0]` stays 0, the lane repeats the current step in place, and
  `stepAdvanceScanDelta` returns 0 (chevron at rest).
- Keep p > 0 and p < 0 EXACTLY as today (the freeze-fix signed scan — forward for p > 0, backward
  for p < 0). Only the p == 0 case changes, from forward-creep to clean repeat.
- Optional polish (not required, do only if clean): make `decideStepAdvance_`'s `signedDelta` a
  three-way so p == 0 yields 0 rather than +delta — moot once the scan is gated off at the
  centre, but it documents the intent at the sign site.

Variation still drives voice direction (the SIGN of p = that voice's advance direction) — the
coupling is unchanged; this is purely the centre.

### Epsilon note (one judgement call to state in the PR)
`p` is a float; knob 0.5 maps to exactly 0, but CV2/Junction can make it a tiny non-zero. Decide
and document: either gate on exact `p == 0.f`, or on `|p| < eps` for a small eps so that a
near-zero p (which would otherwise scan over an enormous-but-finite horizon) also holds cleanly.
Prefer the eps form if a tiny CV offset would otherwise reintroduce a slow creep; keep eps small
enough that ordinary small-but-intentional p still scans. State the choice and the value.

## REPRODUCE-FIRST TEST (new — `test/test_variation_centre_hold.cpp`, registered `$SE $GS $PE $CE`)
- **p == 0 (knob 0.5)**: drive many gate rises with the mono `variationProb` at 0. Assert, over
  the whole run: every decision is HOLD (no advance — `stepIndex` unchanged) AND
  `scanOfsV_[0] == 0` after each rise (NO creep) AND `stepAdvanceScanDelta(mono) == 0`. This FAILS
  today (scanOfsV_ grows) and passes after.
- **p > 0**: the held-rise scan still accumulates FORWARD (existing freeze-fix behaviour — a held
  step is not a permanent wall; the escape comes within ~1/p rises). Unchanged.
- **p < 0**: the held-rise scan accumulates BACKWARD (signed). Unchanged.
- Bit-identity: the existing step-advance tests
  (`test_step_advance*`, the freeze/bipolar suites) stay green — p ≠ 0 behaviour is untouched.

## INVARIANTS
- Goldens BIT-IDENTICAL: clock/phase modes don't use `p` (unipolar note-length reads); gate
  `p ≠ 0` is unchanged; only gate `p == 0`-with-motion changes (forward-creep → clean hold).
  Confirm no golden drives gate at `p == 0` expecting the creep (state it in the PR).
- Full battery green, including the new test.

## NON-SCOPE (do NOT touch — separate decisions)
- REVERSE-ALL / global drive direction, and per-voice HOLD (decision 2) — they ride the
  counter-removal migration.
- Decoupling variation from direction — explicitly REJECTED; variation stays bipolar and keeps
  driving voice direction.
- The VAR chevron DISPLAY sites — owned by `fix/variation-scan-display`; this fix makes them sit
  still at the centre through `scanOfsV_`/`stepAdvanceScanDelta` alone, no display edit here.

## DELIVERABLE
The `scanHold_()`-gating change (+ optional three-way sign), the new `test_variation_centre_hold`
registered and green, the full battery green, goldens bit-identical. PR states the epsilon choice
and the no-golden-pins-centre-creep confirmation.
