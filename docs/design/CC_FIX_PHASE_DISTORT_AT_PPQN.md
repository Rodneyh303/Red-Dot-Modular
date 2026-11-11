# CC FIX  phaseDistort evaluated at PPQN for the engine; video rate for display

Build on feat/phase-duals-gaps (includes the phase PPQN gate, commit 4ed0f10).

## EVIDENCE (Rodney, measured)
Phase mode, 1 voice, Monsoon: 0.5% with identity distribution; 1.2% with any non-uniform distribution. Expander
attached or not makes no difference. The cost is the distortion transform, not the plot.

## CAUSE (code)
Monsoon.cpp:939-941 and :972-974 call dotModular::phaseDistort(uT / uS, set) EVERY SAMPLE whenever the set is not
identity. The PPQN gate (phase.pulseChanged, Monsoon.cpp:1288, PhaseEngine.hpp) gates the quantised tail
(processTiers, snapToPhaseStep, drift), but NOT the transform that feeds it. So the non-uniform transform, an
inverse CDF plus density per sample, runs at audio rate.

## THE RULE (Rodney)
Calculate the distortion at PPQN rate for the ENGINE, and at VIDEO rate for DISPLAY. No change-tolerance heuristic.

- ENGINE: phaseDistort is evaluated ONCE PER PPQN CELL of the input and held until the next cell. Evaluate it at the
  same grid point the existing pulse quantisation uses for the cell, so the held value is the value the engine would
  have produced at that point. Between cells the engine uses the held value.
- DISPLAY: the plot marker, the incoming-phase marker and the outgoing marker evaluate the transform continuously at
  video rate, as now. They are not the engine's value and need not match it between pulses.

## WHAT CHANGES, AND WHY IT IS RIGHT
A feature narrower than one pulse cell (a steep part of the curve, or an atom or gap edge inside a cell) is resolved at
the PPQN grid: it lands on the next evaluation point rather than mid-cell. This is the spec's existing rule that phase
jumps land on PPQN positions, now applied to the transform, not just the tail. It is the resolution the instrument is
built on, not a loss of accuracy in the musical sense.

## IMPLEMENTATION
1. Compute the input cell each sample (cheap): cell = pulse index of uT / uS on the PPQN grid, from the existing
   quantisation.
2. Keep the last cell. When the cell changes, call phaseDistort ONCE for the new cell, store the result as the held
   phaseVolt / subVolt. When the cell is unchanged, reuse the held value. No other per-sample transform call.
3. Identity sets keep the existing raw path (no transform, no held value needed).
4. The display path (DistributionCurveWidget and the phase markers) keeps its own continuous evaluation at video rate.
   It does not read the engine's held value for its curve, and the engine does not read the display.
5. Subphase follows the same rule with its own cell, on the region-relative grid.

## GATES
- CPU: Monsoon in phase mode with a non-uniform distribution returns to near the identity level (~0.5%).
- SAMPLE-POINT EQUIVALENCE: at each PPQN evaluation point, the held value equals phaseDistort evaluated per-sample at
  that same point, bit-identical. (The gate proves the held value is the correct grid value, not an approximation.)
- Identity: unchanged (bit-identical to today).
- Forward and reverse sweeps step through the same cells, and jumps still replay on the new cell.
- The within-region guarantee and the subphase region tests still pass.
- Display: the curve and the markers still update smoothly at video rate (no change to the display path).
- run_all green on both SDK legs.

## DO NOT
- Add a change-tolerance or threshold heuristic. The rule is the PPQN cell.
- Make the display read the engine's held value, or the engine read the display.
- Change the distortion maths, the set store, or the snapshot.
