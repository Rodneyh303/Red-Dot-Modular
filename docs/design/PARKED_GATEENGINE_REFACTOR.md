# PARKED  extract GateEngine (symmetry with ClockEngine / PhaseEngine)

Status: PARKED refactor. Not scheduled. Do after the editor/Macro bugs settle. Pure refactor  golden-identical output
is the gate.

## OBSERVATION (Rodney)
src/dsp/engines/ has ClockEngine (clock-mode time base) and PhaseEngine (phase-mode mapping) but NO GateEngine. Gate-mode
logic is inline in Monsoon.cpp: the modeSelect==1 branches at ~1079, 1186-1230 (the three edge streams  main Gate1,
ratchet Gate2, ghost Gate3), and ~1401 (runGateActive). The subgate handling and the event-driven advance live there too.

## WHY IT IS DEFENSIBLE AS-IS
Gate mode has NO time base of its own  the incoming gates ARE the time base  so there is less to encapsulate than clock
(generates time) or phase (maps time). The inline branches work and are golden-pinned.

## WHY EXTRACT (the consolidation theme)
Gate logic is spread through Monsoon.cpp's process loop, interleaved with the clock and phase branches  the same
inline-per-mode shape that let the East/Macro seed computations drift. A GateEngine would:
- put the three edge streams + subgate handling in ONE place, testable headless like the other engines,
- make Monsoon.cpp's process loop a clean 3-way dispatch (ClockEngine / GateEngine / PhaseEngine),
- give gate mode the same golden-pinned, isolated-test structure the other two have.

## SCOPE WHEN DONE
- Move the modeSelect==1 logic (edge streams, ratchet/ghost subgates, event-driven advance, runGate) into GateEngine.
- Monsoon.cpp dispatches to it; no behaviour change.
- GATE: all gate-mode goldens bit-identical; run_all green. Pure refactor  if any golden moves, the extraction changed
  behaviour and is wrong.

## DO NOT (when it is eventually done)
- Change gate-mode behaviour. Golden-identical is the whole contract.
- Start it while the editor/Macro/16-triage work is in flight.
