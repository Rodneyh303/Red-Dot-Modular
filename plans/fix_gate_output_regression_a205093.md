# URGENT regression — gate output dead in clock/phase modes (A, C, E, F) on master

**Introduced by `a205093`** ("Fix: Mode D gate topology unified with Mode B"), merged to master in
PR #275. Symptom (Rodney, in Rack): **no gate output in Mode A, C** (and by the same cause E, F) —
and Mode D's gate input moved. Suite is GREEN, so no test caught it.

## Diagnosis
`a205093` removed the "post-drive GATE_OUTPUT override" (see the comment it left at
`src/Monsoon.cpp` ~1005) on the assumption that `gs.process()` inside `outputGenerator->drive()`
would carry the gate from engine state. BUT the gate-state writer `engine.gs.gateHeld = gateOpen`
now lives ONLY inside the gate-mode driver block guarded by `(modeSelect == 1 || modeSelect == 3)`
(~Monsoon.cpp:937). For the CLOCK/PHASE modes — A(0), C(2), E(4), F(5) — that block never runs, so
when the override was removed those modes lost the thing that set their output gate state. Result:
`gs.gateHeld` is not driven for A/C/E/F → no gate out.

The driver block itself was CORRECTLY added for B/D. The bug is that the SAME commit removed the
gate-state source the OTHER modes depended on, without giving them a replacement.

(Also: the commit deleted a Mode-D-specific `else if (modeSelect==3 ...)` gate-state block and routed
D through the B driver — that part is intended, see below.)

## Fix direction (CC: diagnose precisely, then repair — do NOT blind-patch)
1. Determine how `gs.gateHeld` (and `gsStep.gateHeld`) were set for A/C/E/F BEFORE `a205093` — i.e.
   what the removed override did for the non-gate modes, or whether the step result's own gate
   decision was meant to feed `gs` via `executeModeA`/`gs.process()`.
2. Restore gate-state driving for modes NOT in {1,3}: EITHER keep a version of the removed override
   scoped to `modeSelect != 1 && modeSelect != 3`, OR repair the clock/phase path so the step
   decision reaches `gs.gateHeld`. Keep the B/D driver block as-is (it is correct).
3. Confirm the fix restores A and C gate output in Rack; E/F (phase) share the path so verify too.

## Mode D input topology — confirm, don't revert blindly
`a205093` moved Mode D's MAIN gate from **Gate 2 → Gate 1** (so B and D share Gate1=main,
Gate2=ratchet, Gate3=ghost — the §421 mode-agnostic invariant). The engine `executeModeD` now
correctly reads `input.gate1` as main and mirrors `executeModeB`. This is INTENDED and should stay
(reverting D to Gate-2-main would break the very invariant the work established). **Decision for
Rodney:** this is a breaking change to existing Mode D patches (they fed Gate 2). Pre-release, so
acceptable — just DOCUMENT it (Mode D now takes its main gate on Gate 1, ratchet on Gate 2).

## REQUIRED new test (this is why it shipped silently)
`test_gate_mode_agnostic` only covers the B≡D q-mix invariant. NOTHING asserts that A/C/E/F actually
EMIT a gate. Add a per-mode gate SMOKE test: for each of modes A, B, C, D, E, F — clock (or gate)
running, a non-rest step — assert `GATE_OUTPUT` goes high. This would have caught the regression at
commit time and must exist before this fix is considered done. Register in run_all.sh.

## Priority
This is a merged-to-master regression killing four of six modes' core output. Fix BEFORE any other
roadmap work (seq/quant unification, Sands). Hotfix branch off master.
