# CC FIX  remove the derived BPM from PhaseEngine (readout with no reader); keep the internal clock untouched

Build on feat/phase-duals-gaps. Rodney: no BPM readout on panel (TBD, doubtful); BPM is impossible in gate mode; may be
extrapolated from the last pulse in external clock or phase mode; internal clock ultimately TBD. Warp must be tempo-
independent (it is).

## EVIDENCE (verified)
- PhaseEngine derives bpm and sixteenthSec from phaseVel (PhaseEngine.hpp:185-194, 385-390). phaseVel is written only
  in PhaseEngine and read only there, to compute bpm/sixteenthSec.
- Nothing reads PhaseEngine::bpm or sixteenthSec outside the phase engine. (Monsoon.cpp, MonsoonWidget.cpp and
  ClockEngine's BPM references are the INTERNAL CLOCK and its readout, not this derivation.)
- The distribution warp (warpDepth) does not use phaseVel. Warp is already tempo-independent.
- The header comment "for bpm + warp" (PhaseEngine.hpp:48) is stale.

## FIX
1. VERIFY no reader first: grep for PhaseEngine::bpm, sixteenthSec, and phaseVel readers across src/. If any reader exists
   outside PhaseEngine, STOP and report it; do not remove.
2. REMOVE from PhaseEngine: bpm, sixteenthSec, the velocity-to-tempo block (lines 185-194 and 385-390), and the
   bpm/sixteenthSec resets (line 94). Keep phaseVel only if a remaining reader needs it; otherwise remove it too.
3. Fix the stale comment at PhaseEngine.hpp:48 and :16 to state the phase path is pulse-quantised and tempo-independent.
4. Do NOT touch: the internal ClockEngine BPM (its generation and any readout), the 48 PPQN work, or the step/pulse
   timing. The internal clock's future is a separate decision.

## GATES
- Warp: the distribution warp output is bit-identical before and after (warp is tempo-independent; the test pins it at
  two different tempos driven by the same phase input).
- Phase step sequence: bit-identical before and after for a sweep (the removal changes no audio path).
- No readout regression: nothing that was displayed changes (the derived BPM had no panel readout).
- Internal clock: unchanged.
- run_all green on both SDK legs.

## DO NOT
- Remove the internal clock BPM or change external/internal clock behaviour.
- Add a BPM readout, or a tempo extrapolation, in this change.
