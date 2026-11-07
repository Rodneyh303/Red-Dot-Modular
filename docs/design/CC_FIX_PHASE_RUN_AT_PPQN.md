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

## THE FIX  gate the quantised tail on pulse change; keep continuous bookkeeping per-sample
In PhaseEngine::processTiers (and the Mode-E block in Monsoon.cpp:899-985):
1. EVERY SAMPLE (cheap, keep): compute phase = normPhase(v); pulseInBar = floor(phase * PPB); and UPDATE the
   continuous bookkeeping the within-region guarantee needs  prevContPhase / the recomputed-from-formula contT
   (PhaseEngine.hpp:54-57 warns this MUST update every sample or the within-region guarantee dies). Do NOT gate this.
2. GATE ON CHANGE: only when pulseInBar != prevPulse, run the EXPENSIVE tail:
   - the tier composition (composedPulse / region quotient-remainder),
   - phaseDistort (already identity-gated; leave that),
   - snapToPhaseStep,
   - the drift check and edge-flag emission.
   When pulseInBar == prevPulse, SKIP all of it and reuse the last quantised result (hold pulseEdge/sixteenthEdge low,
   stepIndex unchanged). Set prevPulse = pulseInBar after running.
3. Monsoon's Mode-E block mirrors the gate: snapToPhaseStep (1265/1296) and the drift path run only on the pulse-change
   signal processTiers reports (reuse phase.pulseEdge or a new "pulseAdvanced" flag), not every sample.

## CARE
- The continuous-phase recompute (prevContPhase) stays per-sample. Only the QUANTISED tail is gated. Verify the
  within-region guarantee test still holds (subphase strictly inside its region at every s < 1).
- Jump/scrub detection must still fire: a large discontinuity changes pulseInBar, so it falls inside the gated path
  naturally  confirm a jump still sets jumped / jumpSixteenths.
- Reverse motion: pulseInBar decreasing is still a change, so the gate runs. Confirm backward phase still steps.

## GATES
- Phase mode CPU returns toward clock/gate levels (expect ~0.5-0.7%, not 1.5-2.0%), identity sets, expander or not.
- Musical result bit-identical: the same step lands at the same phase, forward and reverse, with and without distortion.
  (Pin: a phase sweep produces the identical stepIndex sequence before and after the gate.)
- Jump/scrub still replays; within-region guarantee test still passes.
- run_all green on both SDK legs.

## DO NOT
- Gate the continuous-phase bookkeeping (prevContPhase) off  it must run every sample.
- Change PPB, the region formula, the distortion maths, or snapToPhaseStep's body.
- Introduce a timer; the gate is pulseInBar change, nothing else.
