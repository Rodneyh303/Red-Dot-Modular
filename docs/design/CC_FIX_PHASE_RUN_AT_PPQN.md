# CC FIX  phase mode runs per-sample; gate the quantised work to PPQN pulse changes

Build on feat/phase-duals-gaps. Engine/UI timing fix; no change to musical result.

## EVIDENCE
- Switching Monsoon into phase mode (modeSelect == 2) raises CPU from ~0.5% to 1.5-2.0%, expander attached or not,
  sets at identity. Clock/gate modes do not. The standalone phase module ~2%.
- The phaseDistort calls are already identity-gated (Monsoon.cpp:941/974 behind !phaseDistIdentity_), so the transform
  is NOT the cost at identity.
- Monsoon.cpp:976 calls phase.processTiers EVERY SAMPLE unconditionally. snapToPhaseStep + the drift check also run per
  sample in the Mode-E block. There is no pulse-change gate (searched: no lastPulsePos / pulseChanged).

## THE RULE (Rodney): phase only needs to run at PPQN
The phase VOLTAGE changes every sample, but the quantised PULSE POSITION (pulseInBar = floor(phase * PPB),
PhaseEngine.hpp:131) changes only at PPQN boundaries  far less than once per sample at musical rates. The tier
output, edge flags, jump detection, snapToPhaseStep and the drift check all key off pulseInBar. So the expensive
tail must run ONLY when pulseInBar changes, matching the rate the rest of the instrument already runs at.

## THE FIX  nothing is continuous: run the phase path at pulse rate
Rodney: modulation is shown at video rate and SAMPLED AT STEP RATE. Nothing in the phase path is continuous.
The header confirms it (PhaseEngine.hpp:13 "integer pulses with no continuous-time component"). prevContPhase is
NOT a continuous output  it is an unwrapped pulse ACCUMULATOR used only to compute the next pulse target
(llround(prevContPhase*PPB), tierPulseFor). Nothing downstream reads a continuous phase between pulses; Monsoon reads
the quantised step.
So gate the WHOLE quantised path on pulse change:
1. EVERY SAMPLE (cheap): phase = normPhase(v); pulseInBar = floor(phase * PPB); and ACCUMULATE the phase delta into
   prevContPhase (prevContPhase += shortest-path d)  this is a running sum, not a recompute, so the unwrapped
   position is correct whenever the next pulse computation runs.
2. ONLY WHEN pulseInBar != prevPulse: run the tier composition, phaseDistort (already identity-gated), snapToPhaseStep,
   the drift check and edge-flag emission. Otherwise hold edges low, stepIndex unchanged, and return. Set prevPulse.
3. Monsoon's Mode-E block runs snapToPhaseStep (1265/1296) and the drift path only on that pulse-change signal, not
   every sample.
There is no per-sample continuous consumer to preserve  the earlier "keep continuous bookkeeping per-sample" caveat
is dropped. prevContPhase is maintained as an accumulator across skipped samples (step 1), which is all it needs.

## GATES
- Phase mode CPU returns toward clock/gate levels (expect ~0.5-0.7%, not 1.5-2.0%), identity sets, expander or not.
- Musical result bit-identical: the same step lands at the same phase, forward and reverse, with and without distortion.
  (Pin: a phase sweep produces the identical stepIndex sequence before and after the gate.)
- Jump/scrub still replays; within-region guarantee test still passes.
- run_all green on both SDK legs.

## DO NOT
- Reset prevContPhase instead of accumulating it  the accumulator must carry across skipped samples.
- Change PPB, the region formula, the distortion maths, or snapToPhaseStep's body.
- Introduce a timer; the gate is pulseInBar change, nothing else.
