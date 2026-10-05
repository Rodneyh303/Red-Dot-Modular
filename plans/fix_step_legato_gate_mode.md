# Fix — STEP LEGATO in GATE MODE (edge-truthfulness refinement)

STEP GATE OUT + STEP LEGATO OUT already EMIT (MonsoonOutputGenerator.cpp:68/184-185; verified, incl.
clock mode). The pending item is NARROW: in GATE MODE, the un-fused STEP gate must track the gate
BEFORE legato drops edges, so STEP (and STEP LEGATO) are fully truthful in gate-mode legato cases —
the Monsoon.hpp:437/446 "pending" note. This is the gate-mode edge-truthfulness fix.

## DECISION (Rodney) — tie-across-rest is IGNORED by step legato
When FALSE-mode tie-across-rest is in use, step legato should IGNORE it. Reason: **step legato = the
GATES WITHIN a legato/tie; a REST is not a gate.** A tie-across-rest spans a NON-gate (the rest gap),
so there is no gate there for step legato to mark. Including it would emit a step-legato event where
no gate exists — contradicting what step legato represents.

This makes the fix SIMPLER, not harder: it is not a carve-out, it's what FALLS OUT of the definition.
**Step legato = (GATE ∪ SUBGATE) masked by the slur state.** A tie-across-rest contributes NO gate, so
it is naturally absent from step legato with no special handling. The slur still exists (notes connect
across the rest); step legato, being a gate-marker, simply has nothing to emit at the rest.

## So the model
"We have everything needed with GATE and SUBGATE determining the final legato out when present" —
once the gate + subgate streams are correct and the un-fused STEP gate tracks the pre-legato-drop
edges, STEP LEGATO is derived as the slur-masked union of gate+subgate. Tie-across-rest adds nothing
(no gate), so it needs no code path.

## The fix
- OutputGenerator (gate mode): track the gate state BEFORE legato drops edges (a pre-drop/second
  gate-state flag), so the un-fused STEP gate reflects every sub-note articulation truthfully in
  gate-mode legato — not lastStepResult.decision.
- STEP LEGATO = that STEP gate masked to slurred notes (slurForward OR prevSlur), as the header says.
- Tie-across-rest: no handling needed — it has no gate, so it's absent from STEP by construction.
- Verify: gate-mode legato with subgates emits a truthful STEP / STEP LEGATO; a tie-across-rest shows
  NO step-legato event at the rest (correct); clock-mode STEP unchanged (already correct).
