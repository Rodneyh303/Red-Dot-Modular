# CC PROMPT  PHASE TIER (transport + subphase), staged. Phase direction work, part 1.

Design authority: docs/design/PHASE_TIER_SYSTEM_IDEA.md (the full design, decided). This is the CC build
spec. Build on lane-expander-refactor (32 steps, position holders, UAS all merged). PHASE DISTORTION
(the distribution gap/atom through phase) is a SEPARATE later piece  not this spec.

## WHAT EXISTS NOW
- Phase (Mode E): snapToPhaseStep (SequencerEngine.cpp:926) maps a phase voltage to a step. It hardcodes a
  16-step bar: absStep = pulsePos / p16; phaseStep = ((absStep % 16)+16)%16; stepIndex = phaseStep.
- Phase source: CV1 when patched, else PHASE_PARAM (Monsoon.cpp:848859). Range 0..1.
- PPB today = p16 * 16 (one bar at 16 steps).

## THE MODEL (decided)
Transport phase alone = the full-span mapping. Add a subphase and transport phase is QUANTISED to select
which region; subphase positions WITHIN that region. Quotient/remainder:
    position = quantise(transportPhase, regionSize) + subphase * regionSize
- regionSize = the subphase calibration (quarter note .. one bar). Subphase is capped at <= 1 bar.
- Quantising transport phase gives the coarse region address; subphase is the fine position within.
- Direction: transport phase sets the OVERALL direction; subphase direction MULTIPLIES it (the sign-product
  composition used for voice/lane direction). Flat input carries no direction (no memory; decided).

## STAGE 1  TRANSPORT PHASE SPAN (bit-identical at 16; the extension gate)
Generalise snapToPhaseStep from the hardcoded 16-step bar to the pattern length.
- PPB = p16 * patternLength (not * 16). absStep uses the active length. The bar modulo stays 16 (a bar is
  always 16 steps; the pattern can be 16 or 32).
- At patternLength = 16 the result MUST be identical to today. Golden battery bit-identical  the gate.
- At patternLength = 32, transport phase spans all 32 steps (2 bars). This is the "expand to 192" step from
  the plan: PPB = p16 * 32.
- No subphase yet. One phase input, full span.
Gate: <=16 phase goldens bit-identical, no re-pins. A 32-step phase sweep covers all 32 steps, period 32.

## STAGE 2  SUBPHASE INPUT + CALIBRATION
- Add a SUBPHASE input (a second phase CV) and a SUBPHASE CALIBRATION control (regionSize: quarter .. bar).
- When the subphase input is UNPATCHED, behaviour is EXACTLY Stage 1 (transport phase alone). Bit-identical
  gate holds.
- When patched: position = quantise(transportPhase, regionSize) + subphase * regionSize, per the model.
  regionSize quantises transport phase to region boundaries; subphase positions within the region.
- "No jumps between bars" at regionSize = 1 bar falls out: subphase only addresses within the selected
  region, so it cannot cross the bar boundary.
- Direction: sign(transport change) * sign(subphase change), the existing sign-product rule. Reversal in
  subphase within a forward transport is a backward step within the region.
- Mid-step arrival/jumps reuse the existing snap/drift machinery (snapToPhaseStep's park + the drift check);
  the arrival rule (fresh step, no carry-over, at arrival probabilities) applies at region landings.
Gate: subphase unpatched = Stage 1 bit-identical. Subphase patched: a pin for the quotient/remainder
decomposition at regionSize = 1 bar and = 1 quarter, both directions, including the both-reverse = forward
case (confirm that is intended, per the design doc's open check).

## STAGE 3  DISPLAY
- The ring/disc already show bar and step (32-step work). Subphase does not change the ring; it changes
  WHICH step within the bar is read. Confirm the disc still tracks the engine bar under subphase.
- No new display is required for Stage 2 beyond confirming the existing indicators remain correct.

## OUT OF SCOPE (later)
- Phase distortion (transport/subphase through the distribution gap/atom). Separate spec; needs the gap stage
  (PHASE_DISTRIBUTION_SPEC.md) first.
- Co-jumps (tail dependence across the two tiers). Follows the distortion work.
- PPQN finer-than-step landing (the pulse-resolution playhead). Separate decision.

## PINS SUMMARY
- Stage 1: <=16 phase goldens bit-identical; 32-step sweep period 32.
- Stage 2: subphase unpatched bit-identical; quotient/remainder at 1 bar and 1 quarter; sign-product
  direction; both-reverse case.
- GOLDEN_A untouched throughout. run_all green (both SDK legs).

════════════════════════════════════════════════════════════════════════════════
## CALIBRATION  QUANTISE TO 24 PPQN PULSES, STRAIGHT LIST ONLY (Rodney, decided)
The subphase calibration currently maps continuously (Monsoon.cpp:883: regionSixteenths = 4 + 12*knob),
which allows fractional-step regions (e.g. 6.3 sixteenths). That breaks the quotient/remainder tiling. Fix:

- Calibration is a DISCRETE list of region sizes, in 24-PPQN PULSES (6 pulses per 16th; a bar = 96 pulses).
- STRAIGHT LIST (ship this first; nothing else yet):
    bar     = 16 steps = 96 pulses   (1 region/bar)
    half    =  8 steps = 48 pulses   (2)
    quarter =  4 steps = 24 pulses   (4)
    eighth  =  2 steps = 12 pulses   (8)
    16th    =  1 step  =  6 pulses   (16)
- The knob SNAPS to these detents; no continuous sweep.
- Transport phase QUANTISES TO PULSES. The region index and the within-region position both come from a
  PULSE count, not a continuous step value. This keeps subphase on the same 24-PPQN grid as everything else
  and prevents fractional regions returning.
- Region boundaries therefore always fall on pulse positions, and the straight list always tiles the 96-pulse
  bar exactly.

## DEFERRED (not this work)
- Non-straight groupings (e.g. 6/6/4) and Euclidean partitions (E(3,16)=6,5,5; E(5,16)=3,3,4,3,3;
  E(7,16)=2,2,3,2,2,3,2). These are other INTEGER-PULSE partitions of the 96-pulse bar  the same calibration
  table, generated differently. Add after phase-through-distributions (freeze + jump) has landed. Rotation on
  the Euclidean ones is a later question.

## PRIORITY
Land PHASE THROUGH DISTRIBUTIONS (freeze + jump) FIRST. The calibration snap-to-pulses fix above is small and
can ride with it. The groupings/Euclidean come after.
