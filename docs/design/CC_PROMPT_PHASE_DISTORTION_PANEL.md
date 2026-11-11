# CC PROMPT  Phase distortion PANEL (expander + Monsoon index knobs). Panel only, no engine change.

Engine is landed on feat/phase-duals-gaps (commit 08a3edf): PhaseDistStore (8 sets, per-block snapshot), the shared
primitive dotModular::phaseDistort, Monsoon PHASE_SET_PARAM / SUBPHASE_SET_PARAM (Monsoon.hpp:323). Do NOT change
any of that. This prompt adds the controls that expose it.

## TASK 1  Expander panel: eight phase-distortion sets (template = Distribution module, 8 rows)
Build a panel modelled on the Sands Distribution module (SandsDistributionExpander / its panel), with EIGHT ROWS
(one per phase-distortion set) instead of seven lanes. Reuse the existing widgets; do NOT draw new ones.
- Each row = one parameter set. Row k edits PhaseDistStore set k.
- Per-row controls (mirror the distribution row, relabel where the meaning differs):
    WARP, TILT, LEVEL?, MASS (atom mass), GAPLOC, GAPSIZE, plus the ATOM LEVEL and the row's modulation jacks.
  Use the same control types and knob art as the distribution row. Atom knobs sit beside the gap knobs, as landed.
- Modulation jacks per row mirror the distribution row's MOD jacks, one per modulated parameter, same spacing.
- Selected row drives ONE plot, reusing DistributionCurveWidget (the effective-curve plot, spec sec 10). Selection by
  the row's focus button, the same way the distribution panel follows its selected row.
- Header and footer text: "PHASE DISTORTION", and the selected set number.

## TASK 2  Monsoon index knobs  DEFERRED (not in this build)
No phase-input knobs on Monsoon for now. The phase and subphase index stays a MENU choice (MonsoonWidget menu items
setting PHASE_SET_PARAM / SUBPHASE_SET_PARAM). Revisit when the routing panel is decided. Do not add Monsoon knobs.

(Former task, kept for reference only:)
- Place PHASE_SET_PARAM and SUBPHASE_SET_PARAM on the Monsoon panel, next to the existing phase and subphase inputs.
- Knob range 1..8, snapped to integers (the clamp in Monsoon.cpp:1696-1697 already enforces it).
- Label each knob with its input (PHASE SET, SUB SET). Show the set number on the knob face.
- Default value per the existing fresh-module logic (2N-1 / 2N). Do not change the default code.
- Each knob must visibly show the selected set; there is no hidden routing.

## DECISIONS TO CONFIRM (flag in the PR, do not guess)
1. DELEGATION. The spec sec 14 lists delegation in the set, but a phase set has no Macro or East to delegate to. Proposal:
   OMIT delegation from the phase rows, since the sets are global to the module. Confirm before building the row.
2. ATOM LEVEL vs MASS. The distribution row has MASS and LEVEL controls. Confirm which maps to the atom (mass = size of the
   freeze, level = where it sits) and label accordingly.

## GATES
- The expander panel renders 8 rows, each bound to its PhaseDistStore set index. Changing row k's knob changes set k only
  (test with the store: write a value on the panel, read back set k, other sets unchanged).
- The plot follows the selected row.
- Monsoon index: menu-only for now. Setting the index via the menu changes the routed set, and the value persists with the patch.
- Audio: no change. With all sets at identity, output is unchanged from the landed engine build (the expander gate
  already in place). run_all green on both SDK legs.
- Layout: the 8-row panel fits the module's width; check at real brightness in Rack (human review, no automated gate).

## DO NOT
- Change PhaseDistStore, the snapshot, the shared primitive, or the Monsoon routing logic.
- Build the matrix (sec 12) or the module-only model (sec 13).
- Draw new widgets where a distribution widget exists.
