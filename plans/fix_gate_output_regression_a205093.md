# URGENT regression — GATE_OUTPUT dead in modes A, B, C, D (all tested) on master

Symptom (Rodney, in Rack): no gate output across modes A, B, C, D. Suite GREEN — nothing caught it.

## IMPORTANT: diagnose by BISECT/BUILD, not by trusting this doc's commit-pointing
This was mis-attributed to `a205093` (the Mode D fix) at first; closer reading says `a205093` is
clean and the GATE_OUTPUT-write removal happened EARLIER in the subgate work (`d0b8ea4` "Schmitt
gate edges / restore MODEL 1 legato bridge" and `10f4c68` "subGate ghost GATE re-plumb" each removed
a GATE_OUTPUT line). **CC: git bisect in Rack (does GATE_OUTPUT fire?) across 10f4c68..HEAD to confirm
the exact culprit before fixing.** Do not assume.

## Likely root cause (verify)
`src/dsp/managers/MonsoonOutputGenerator.cpp:32` does `float gateV = outputs[GATE_OUTPUT].getVoltage();`
— it READS the output jack's current voltage, mutes it, and writes it back (line 159
`setGateWithMute_(outputs[GATE_OUTPUT], gateV, ...)`). So the generator is read-modify-write; it does
NOT compute the gate from engine state. The code that USED to write GATE_OUTPUT from `gs` state (the
"post-drive GATE_OUTPUT override", per the comment at Monsoon.cpp:1005) was REMOVED during the subgate
work, on the assumption that `gs.process()` would carry it. If nothing now drives GATE_OUTPUT high from
`engine.gs.gateHeld`/`gs.process()`, the jack stays low in every mode. That matches "A/B/C/D all dead"
(shared output stage, not a mode-specific path — the per-mode execute dispatch is unchanged).

## Fix
1. **Bisect to confirm** the commit where GATE_OUTPUT stopped firing (Rack test, all modes).
2. Trace the `gs.gateHeld`/`gs.process()` → GATE_OUTPUT path through
   `MonsoonOutputGenerator::drive`. Determine whether `gs.process()` is supposed to SET the gate
   voltage (and regressed) or whether the removed override was the ONLY writer.
3. Restore a single, correct GATE_OUTPUT writer from engine gate state, shared by ALL modes (B/D's
   gateHeld driver block sets `gs.gateHeld`; A/C/E/F set it via their step results — the generator must
   turn `gs` state into the jack voltage for every mode). Do NOT re-introduce the read-back self-write
   as the source of truth.
4. Confirm in Rack: all six modes emit a gate on a non-rest step.

## Mode D input topology (separate, intended — keep + document)
`a205093` correctly moved Mode D's MAIN gate Gate2→Gate1 (B/D share Gate1=main/Gate2=ratchet/Gate3=ghost,
the §421 invariant). Keep it; document the breaking change (Mode D main gate is now Gate 1).

## REQUIRED new test — this is why it shipped green
Add a per-mode GATE SMOKE test: for modes A..F, clock/gate running + a non-rest step, assert
GATE_OUTPUT goes high. `test_gate_mode_agnostic` only compared B vs D; nothing asserted the most basic
thing — that a gate comes out. Must land with the fix; register in run_all.sh.

## Priority
Merged-to-master regression killing core output. HOTFIX off master before all other roadmap work.
