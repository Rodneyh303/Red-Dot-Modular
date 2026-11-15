# CC FIX  standalone PhaseDistortion does not use the PPQN-held cell core (Monsoon does)

Build on feat/phase-duals-gaps.

## EVIDENCE
- The subpanel cost is now negligible; the standalone PhaseDistortion module is still expensive.
- PhaseDistortion::process (PhaseDistortion.cpp:52-55) calls ch_[k].process(in, reader_.snap_[k]) for every channel
  every sample. ch_[k] is the PER-SAMPLE channel core from dsp/PhaseDistCore.hpp, which calls
  dotModular::phaseDistort(normPhase(delayed), cell) every sample (PhaseDistCore.hpp:85).
- The PPQN cell-held core already exists: dsp/PhaseDistCellHold.hpp (heldVolt, phaseDistort at line 105, evaluated once
  per cell and held). Monsoon uses it (Monsoon.hpp member; Monsoon.cpp:941-1005 phaseDistSnap_[..].cell()).
- So the standalone never received the PPQN rule; only Monsoon did.

## FIX
1. Replace the standalone's per-sample channel core with the cell-held core, the SAME type Monsoon uses.
   In PhaseDistortion.hpp, ch_ becomes the held-cell core (PhaseDistCellHold), one per channel. In process(), the call
   stays `ch_[k].process(in, reader_.snap_[k])`; the held core is what evaluates the transform, once per cell.
2. Do NOT re-implement the cell logic in the standalone. Reuse PhaseDistCellHold exactly as Monsoon does. One
   implementation.
3. Keep the identity early-out (an identity set passes the input through with no transform), as the held core already does.
4. The standalone's editor plot: confirm it is framebuffered (the DistributionCurveWidget framebuffer from a3ab43b). If the
   editor draws its own curve widget outside that path, wrap it the same way. Do not change the curve maths.

## GATES
- Standalone CPU in phase mode with a non-uniform set returns to near the identity level, the same profile as the
  expander-hosted version (no per-sample transform).
- SAMPLE-POINT EQUIVALENCE (as for Monsoon): at each cell evaluation point the held output equals phaseDistort
  per-sample at that point, bit for bit.
- Identity: bit-identical to today.
- The standalone's output with the held core matches Monsoon's held output for the same input and set (the two
  consumers agree).
- The 8-channel layout, the channel focus, and the editor behaviour are unchanged.
- run_all green on both SDK legs.

## DO NOT
- Re-implement a second cell or hold path in the standalone. Use PhaseDistCellHold.
- Change the transform maths, the set store, or the snapshot.
- Add a tolerance or timer; the rule is the PPQN cell.
