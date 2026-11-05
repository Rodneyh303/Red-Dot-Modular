# PHASE TIER — build plan (transport phase span + subphase), staged

Design authority: `docs/design/PHASE_TIER_SYSTEM_IDEA.md` (decided). Build spec:
`docs/design/CC_PROMPT_PHASE_TIER.md`. This plan carries the implementation map and the
adjudications made while landing it. Branch: `feat/phase-tier-subphase`, built off the 32-step
feature chain (Rodney: "It can also work off the 32 step branch" — the branch is cut from
`feat/phase-duals-gaps`, the current tip, which contains `feat/steps-32-stage1` in full).

## THE TREE THE SPEC LANDS ON (deltas vs the spec's WHAT EXISTS NOW)

The CC spec was written against lane-expander-refactor. The 32-step work landed since; the
"what exists now" section reads slightly differently on this tree:

- `snapToPhaseStep` (SequencerEngine.cpp:940) already mods by `kMaskRing` — which the 32-step
  Stage 2a flipped from 16 to 32 (`kMaskRing == kPatternLengthCap == 32`). The hardcoded-16
  modulo the spec quotes is gone; the step-index side is already span-general.
- The SPAN side is still hardcoded: `PhaseEngine.hpp:79` computes `PPB = p16 * 16` — the phase
  input spans one 16-step bar regardless of the active pattern length. That is Stage 1's target.
- The drift check (Monsoon.cpp:1149) still mods by **16**: `phaseStep = ((absStep % 16)+16)%16`
  while `snapToPhaseStep` mods by 32. **Latent bug found while planning**: `pulsePos` is
  UNWRAPPED (PhaseEngine tracks `prevContPhase` continuously, so `pulsePos` grows across bar
  wraps), so from the second forward bar onward `absStep >= 16` and the drift check disagrees
  with `snapToPhaseStep`'s modulo every sample — `snapToPhaseStep` fires every sample
  (anchorTransport + deriveHolders + recomputeLaneTicks per sample). Output positions stay
  correct (each snap re-derives the same answer) but the per-sample snap is waste, and it is
  exactly the mismatch Stage 1's span generalisation would turn into visible wrongness at 32.
  Stage 1 fixes it: ONE shared modulo rule (kMaskRing) for both sites.

## STAGE 1 — TRANSPORT PHASE SPAN (bit-identical at 16; the extension gate)

Generalise the span from the hardcoded 16-step bar to the active pattern length.

- `PhaseEngine` gains `int patternLength = 16;` (default 16 — every existing caller and test
  keeps today's behaviour bit-identical). `PPB = p16 * patternLength` (PhaseEngine.hpp:79).
  Phase 0→1 now spans the ACTIVE pattern (one bar at 16; two bars at 32).
- Monsoon sets `phase.patternLength = engine.cachedLength` each block before `phase.process`
  (the active length; the throttled control block's `updateWindow` result applies from the next
  block — one block of latency on length changes, absorbed by the snap/drift machinery, same
  as every other length-change consumer).
- Drift check (Monsoon.cpp:1149): `% 16` → `% kMaskRing` — the same modulo `snapToPhaseStep`
  uses, one rule, both sites. Kills the latent per-sample snap above; bit-identical at
  length 16 within the first bar (where all existing goldens live: absStep < 16 → the two
  modulos agree).
- Jump replay cap (Monsoon.cpp:1104): `if (steps > 16) steps = 16` → the active length
  (`engine.cachedLength`) — the "within-draw" bound the PhaseEngine comment already promises
  ("the replay is bounded to the current phrase"). At 16: identical.
- `snapToPhaseStep` itself: NO code change (already kMaskRing-general on this tree); comments
  updated. The bar modulo inside PhaseEngine (`step16`, the sixteenth/quarter edge feed) STAYS
  16 — a bar is always 16 steps; the pattern can be 16 or 32 (the spec's own words).
- Gate: the whole battery bit-identical (no re-pins) + new pins: a 32-step phase sweep covers
  all 32 steps; two consecutive sweeps digest equal (period 32); the ≤16 span formula pinned
  directly (`pulsePos == floor(phase * p16 * length)` probes).

## STAGE 2 — SUBPHASE INPUT + CALIBRATION (CV3 in; the quotient/remainder)

Rodney's jack assignment (verbatim): "Currently there are 3 cv in on monsoon. We use cv1 in
for phase. Lets use cv3 in for subphase. Cv2 in is currently used for quantiser."

- **CV3 = SUBPHASE in Mode E.** `CV3_MOD_INPUT` currently carries the assignable modulation
  target (cv3Mods → rhythm/melody slew/mix, plus the Raffles expander sum). In the phase origin
  (modeSelect == 2) CV3 is repurposed as the subphase input — the exact mirror of how Mode E
  already takes over CV1 exclusively ("mirrors how Mode B repurposes gate1"). Outside Mode E,
  CV3 keeps its assignable role untouched. While Mode E owns CV3, the assignable-mod path and
  the Raffles sum skip it (no double-consumption of one jack).
- **SUBPHASE CALIBRATION param** (`SUBPHASE_CAL_PARAM`, appended to ParamIds — the stable-id
  rule; old patches default it): regionSize knob, quarter .. bar, CONTINUOUS (the design doc:
  "anything between (quarter-note → bar)"). Knob 0..1 → regionSixteenths = 4 + 12·v (4 = one
  quarter, 16 = one bar). Default = 1 bar (v = 1.0) — the showcase value (2 regions at 32;
  "no jumps between bars" preserved by construction).
- **The composition lives INSIDE PhaseEngine** (new `processTiers` path), NOT at the voltage
  level. Reason (adjudication): composing the two wrapped phases into a composed *voltage*
  aliases at the transport cycle wrap — the composed position's mod-1 delta is ambiguous
  (0.99 → 0.49 reads as −0.5 where the true motion is +0.5 ring-forward), and the shortest-path
  rescue only works for deltas strictly beyond ±0.5. The engine instead tracks the transport
  UNWRAPPED (it already does — `prevContPhase`) and composes in the unwrapped domain:
  ```
  rF      = regionSixteenths / patternLength          // region fraction of the full span
  coarse  = floor(contT / rF)                         // unwrapped region address
  composed = coarse·rF + s·rF                         // s = subphase, wrapped [0,1)
  ```
  `s` is used WRAPPED — the subphase only ever addresses WITHIN the selected region (the cap:
  "Subphase is capped at ≤1 bar … its jumps stay inside one bar" — preserved by construction,
  the spec's own "falls out" language). `contT` is UNWRAPPED, so the transport cycle wrap is a
  region crossing into the next cycle's region — the composed motion continues ring-forward
  (pinned in test). The composed value maps to pulses with the Stage-1 PPB; everything
  downstream (jump detection, the replay, `snapToPhaseStep`, the drift check) is the EXISTING
  machinery applied to the composed position — "not a new jump class, the existing one applied
  twice" (design doc). Region landings ARE the arrival rule: the replay executes the crossed
  steps fresh (frozen modulation = arrival probabilities) and `snapToPhaseStep` parks the
  transport exactly — no new code.
- **Direction: the sign product** (`dirT · dirS`), replacing the raw composed-motion sign at
  the per-block application point (Monsoon.cpp:864-865). The engine exposes each tier's own
  per-sample motion sign (shortest-path deltas, exactly how the transport's `reverse` is
  derived today); Monsoon composes:
  - `dirT` = transport delta sign, `dirS` = subphase delta sign, flat (0) = NEUTRAL —
    "Flat input carries no direction (no memory; decided)": a neutral tier contributes no flip,
    the product degenerates to the other tier's sign. Neutral maps to forward (+1), matching
    today's stalled-ramp reading (`reverse = (step < 0)` — a parked ramp is forward).
  - Unpatched subphase → dirS neutral → the product IS `phase.reverse` — bit-identical.
  - The jump path keeps replaying in the COMPOSED jump direction (position congruence — which
    steps were physically crossed), while the post-landing semantic direction is the sign
    product (captured in `savedReverse`, restored after the snap). The two roles were already
    separate on this tree; Stage 2 only changes what feeds the semantic one.
- **The both-reverse = forward case** (the design doc's open check, the CC spec's Stage-2
  pin): the sign-product rule is the DECIDED rule ("using the same sign-product composition as
  voice and lane direction (playhead × voice × lane)"), and under it two reversals compose to
  forward — the same result the voice/lane composition produces everywhere else in the
  instrument. Adjudication: it IS the intended musical result (consistency with the existing
  sign-product composition, not a coincidence of this instance), and it is pinned as such in
  the test so the intent is enforceable. Flagged here for Rodney's rack review.
- The knob path (`PHASE_PARAM`) remains a legal transport source with the subphase composed on
  top (the knob is just CV1's fallback; the composition is source-agnostic).
- Gate: unpatched = Stage 1 bit-identical; quotient/remainder pins at regionSize = 1 bar and
  = 1 quarter (both directions); the sign-product matrix; both-reverse = forward; the no-cross
  property (a full subphase sweep never leaves its region); a region crossing fires the
  existing jump path with `jumpSixteenths == regionSixteenths`.

## STAGE 3 — DISPLAY (confirm only)

The ring/disc already show bar and step (the 32-step work). Subphase changes WHICH step within
the bar is read, not the ring's geometry — the disc tracks the engine bar (stepIndex), which
the composed position drives through the same `snapToPhaseStep`. Confirm via the existing
indicator test battery (test_ring_bar_indicator) staying green under the Stage-2 engine, plus
a Stage-2 test asserting stepIndex tracks the composed position at region landings. No new
display surface.

## OUT OF SCOPE (per the CC spec)

Phase distortion through the distribution gap/atom (needs the extracted primitive), co-jumps
(tail dependence across the two tiers), PPQN finer-than-step landing. GOLDEN_A untouched
throughout; run_all green on both SDK legs is the standing gate for every stage.
