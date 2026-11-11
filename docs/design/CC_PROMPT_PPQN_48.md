# CC PROMPT  48 PPQN (32 steps = one bar at 1/32; 16 steps = half a bar). Staged, 24 PPQN goldens are the gate throughout.

Build on feat/phase-duals-gaps. Supersedes the "require 24" decision in PPQN_48_96_NOT_SAFE.md.

## DECISIONS (Rodney)
1. A STEP IS 6 PULSES, FIXED. The musical division is derived from PPQN.
2. The DISC is UNCHANGED: its hemispheres still mark the two halves of the 32-step pattern, whatever the PPQN.
3. PPQN IS SAVED in the patch. Recalling a patch at a different PPQN must restore that patch's PPQN.
4. The INTERNAL CLOCK generates 48 PPQN too (the multiplier). External clock input at 48 is multiplied so the step rate
   at the same BPM doubles.
Also: Lantern stays 16 columns (half a bar at 48). Gate mode is unaffected by PPQN.

## THE CORE CHANGE: separate pulses-per-step from pulses-per-sixteenth
- Today p16 (pulses per sixteenth) is used as the STEP size. At 24 PPQN p16 = 6; at 48 it becomes 12, which would double
  every step. That is wrong under decision 1.
- Introduce PULSES_PER_STEP = 6 (constant) and keep PULSES_PER_SIXTEENTH = PPQN / 4 (24 -> 6, 48 -> 12) for MUSICAL
  durations only.
- Every STEP-indexed computation (snap, held cells, step boundaries, the advance, the lane walks) uses PULSES_PER_STEP.
  Only the note-value / musical-duration computations use PULSES_PER_SIXTEENTH.

## STAGE 1  pulses-per-step / pulses-per-sixteenth split (24 PPQN BIT-IDENTICAL)
- Introduce the two constants. Change every step-indexed use of p16 to PULSES_PER_STEP. At 24 PPQN both are 6, so the
  output is identical.
- GATE: the full 24 PPQN golden battery bit-identical, no re-pins.

## STAGE 2  the note-value table in pulses (24 PPQN BIT-IDENTICAL)
- Express each division in pulses: at 24 PPQN 1/16 = 6, 1/32 = 3 (the 1/32 needs a half-step, see below), triplets and
  dotted as their pulse counts. At 24 the table must reproduce today's lengths exactly.
- Legato-lead eligibility: a division can lead a legato only if it ends on a STEP boundary, i.e. its pulse count is a
  multiple of PULSES_PER_STEP (6). At 24 PPQN 1/32 is NOT a step boundary (3 pulses), so it stays non-lead, as today.
- GATE: 24 PPQN goldens bit-identical; the legato-lead set at 24 unchanged.

## STAGE 3  48 PPQN as a selectable, saved setting
- Re-add 48 to the PPQN menu (it was removed as a footgun; it is now real). The setting is saved in the patch and restored
  on load.
- With PPQN = 48 and the incoming clock at 24 (external), the clock multiplies by 2 so each step is 6 pulses of a 48
  grid = a 1/32. The step rate at the same BPM is 2x.
- GATE: a patch saved at 48 reloads at 48; a patch at 24 reloads at 24 (no change).

## STAGE 4  the internal clock at 48
- The internal clock generates 2 grid pulses per internal tick when PPQN = 48. Step counts per tick are unchanged; the
  step rate doubles at the same BPM.
- GATE: internal clock at 48 produces exactly 2x the pulses per bar of 24; BPM readout (internal only) unchanged in meaning.

## STAGE 5  phase at 48
- Rerun the phase PPQN gate, the held-cell core and the SAMPLE-POINT EQUIVALENCE test with PULSES_PER_STEP = 6 and the
  sixteenth at 12. The phase positions per bar become 192. These tests were only ever run at 24, so this is a real test.
- GATE: at 48 the phase step sequence equals the 24 sequence for the same phase input (per bar, scaled), and the held
  cell equivalence holds.

## STAGE 6  verification at 48
- Legato lead: a 1/32 note ends on a step and CAN lead a legato; 1/16T and 1/8T (non-multiples of 6) still CANNOT.
- Gate mode: unchanged at 24 and 48 (no PPQN dependence). Confirm with a gate sequence at both settings.
- Disc unchanged; Lantern shows half a bar (16 columns) at 48; the ring twice around is one bar at 48 (two bars at 24).
- CPU: match STEP RATE, not BPM. Compare 24 PPQN at 2x BPM with 48 PPQN at 1x BPM; the difference is pure per-pulse cost.
- run_all green on both SDK legs at 24 and at 48.

## DO NOT
- Change the disc semantics, Lantern's column count, or gate mode.
- Enable sub-step legato for triplets or dotted values (parked).
- Include 96 PPQN (not in scope).
