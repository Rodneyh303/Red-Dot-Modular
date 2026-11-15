# CC PROMPT — Position Resolver SLICE 3: migrate the CONSUMER reads to playedColumn (+ fold in the scanHold ODR consolidation)

Follow-on to slice 2 / 2b. The resolver (`pos_`) is a proven mirror: `test_position_resolver_
identity` pins `pos_.playedColumn() == stepIndex` at EVERY step (every window, every direction,
across the phase snap). This slice migrates the position CONSUMERS to read `pos_.playedColumn()`
so a later slice (step 4) can delete the redundant `stepIndex` field. Reference:
`DESIGN_POSITION_UNIFICATION.md` migration step 3.

Two independent, bit-identical slices in one prompt (Rodney: fold the scanHold consolidation in so
it rides the same battery run). PR them together or separately — both must keep the full battery
green and goldens bit-identical.

## CARDINAL RULE (non-negotiable)
VERIFY AGAINST CODE AND TESTS, never a plausible narrative. Both slices are PURE REFACTORS — they
must be BIT-IDENTICAL (goldens + the identity harness are the proof). No Rack here; widget sites
are Rodney's leg.

## BRANCH
Off `lane-expander-refactor` (which now carries slice 2/2b via its branch — confirm 2/2b is merged
first, or cut off the slice-2 branch). PR back into `lane-expander-refactor`.

────────────────────────────────────────────────────────────────────────
## SLICE 3 — migrate the CONSUMER reads of stepIndex → pos_.playedColumn()

### The distinction that matters (do NOT get this wrong)
`stepIndex` reads split into two kinds. Migrate ONLY the CONSUMERS; leave the AUTHORITY alone.

**MIGRATE (consumers — they READ the played position for display / result / bookkeeping):**
- the Lantern / LED-ring display math in SequencerEngine.cpp (~:1334, :1341, :1392, :1403, :1404,
  :1425, :1440) — the current-step and trail rendering;
- `result.forStep = stepIndex` (:1510, :1708) — the step a StepResult is tagged with;
- the manager reads: `MonsoonLockManager.hpp` (:175 `prevStep_ = stepIndex`),
  `MonsoonModeController.hpp` (:95 `lastStepIndex = engine.stepIndex`).
Each becomes `pos_.playedColumn()`. Bit-identical by the identity harness.

**DO NOT MIGRATE (authority / advance-internals — they MAINTAIN the position):**
- setWindow's window re-check (:283);
- advancePlayhead's working column + window scans (:415 `prevStep = stepIndex`, :481, :490);
- snapToPhaseStep's window scans (:1260, :1262).
`stepIndex` is the WORKING variable being advanced/snapped at these sites; `pos_` is advanced
ALONGSIDE it. Swapping these for `playedColumn()` would be circular. Leave them exactly as they are.

**Out of scope entirely:** `PatternEngine::switchMelodyMode/switchRhythmMode` take `int& stepIndex`
by reference (a passed parameter, not a `pos_` consumer) — leave them.

### Do NOT
- delete the `stepIndex` field or any `stepIndex = …` WRITE — that is step 4, after no consumers
  remain;
- touch the derive (slice 2's `sPos` law) or `pos_`'s advance/snap;
- migrate the WIDGET reads — `Intertropical.cpp`, `Lantern.cpp`, the Sands visuals,
  `VisualExpanderHelpers.hpp`, `LanternRecordCore.hpp`, `Monsoon.cpp` — those are Rodney's Rack
  leg and a SEPARATE later pass (step 3b). List them in the PR; do not edit them here.

### Acceptance (bit-identical)
- `GOLDEN_A/B/BS`, `UA_NONDIV` BIT-IDENTICAL; `test_position_resolver_identity` stays green (it is
  the guarantee that `playedColumn() == stepIndex`); the Lantern tests and full battery green.
- Add a small STRUCTURAL pin (grep-in-test, the house `D`-precedent): the migrated consumer sites
  read `pos_.playedColumn()` and no longer read the bare `stepIndex` — so a regression that
  re-introduces the field read is caught headlessly.

────────────────────────────────────────────────────────────────────────
## SLICE X — consolidate scanHold_ + scanHoldV_ into one scanHoldFor_ (the ODR fix, Rodney)

There is a `TODO(ODR/DRY — consolidate)` in `SequencerEngine.hpp` above the two functions. They are
the SAME law written twice — the "mono is special" split that let the variation-centre poly creep
slip through (mono got the centre-gate; the per-voice twin did not, first pass).

### The fix
Collapse `scanHold_(float p)` (mono, `scanOfsV_[0]`, mono LOR) and `scanHoldV_(VoiceSlot bank)`
(per-voice, `scanOfsV_[bank.v]`, polyLOR) into ONE:

    void scanHoldFor_(dotModular::VoiceSlot bank) {
        const float p = voices[bank].variationProb;                 // the voice's own p
        if (p < kVariationCentreEps && p > -kVariationCentreEps) return;   // the ONE centre gate
        const int varLen = /* mono: lor(STRAND_VARIATION,LEN); poly: polyLOR(bank,VAR,LEN) */;
        scanOfsV_[bank.v] = (scanOfsV_[bank.v] + 1) % (varLen > 0 ? varLen : 1);
    }

Mono is just `bank == kMonoVoiceSlot`: select the LOR source and the `scanOfsV_` index by the bank
(`bank.v == 0` for mono). Read `p` from `voices[bank].variationProb` — the same value
`decideStepAdvance_` resolves on the main path, exactly as the two functions do today. Update the
callers: the mono HOLD site (`scanHold_(voices[mono()].variationProb)` → `scanHoldFor_(mono())`)
and the per-voice HOLD site (`scanHoldV_(vs)` → `scanHoldFor_(vs)`). Delete the TODO.

### Acceptance (bit-identical — it is a pure refactor)
- `test_variation_centre_hold` stays green: mono GATES 1-4 (centre hold, p>0/p<0 scan law, eps
  boundary) AND poly GATE 5 (the per-voice centre + accumulate) already pin BOTH paths — the
  consolidated function must reproduce them exactly. Add a direct `scanHoldFor_` unit check only if
  it sharpens the pin.
- `test_step_advance` (the WANDER/PV centre gates) and the full battery green; goldens bit-identical.
- Confirm the mono `varLen` (`lor(STRAND_VARIATION, LOR_LEN)`) and poly `varLen`
  (`polyLOR(bank, VAR, LOR_LEN)`) are selected correctly by bank — a swap here would silently change
  the mono vs poly window bound.

## DELIVERABLE
Slice 3: the consumer reads on `pos_.playedColumn()` + the structural pin, authority/advance reads
and the field untouched, widgets listed for the later pass. Slice X: the single `scanHoldFor_`, both
callers routed through it, the TODO gone. Full battery green, goldens + identity harness + the
reverse/phase oracles all bit-identical. PR notes which `stepIndex` sites were migrated vs left (and
why), and confirms the scanHold varLen selection.
