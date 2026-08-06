# Modes C & D (quantizer modes) -- NEGLECTED, need a pre-release pass

## What they are
- MODE C = Quantizer Mode 1: clock-driven. Quantizes an external CV (input.cv2) to the active scale on
  clock edges. (MonsoonModeController.cpp:195-215, SequencerEngine::executeModeC.)
- MODE D = Quantizer Mode 2: gate2-driven. Quantizes external CV on Gate 2 rises.
  (MonsoonModeController.cpp:217-230, SequencerEngine::executeModeD.)
Both are REAL (not stubs) but LONG NEGLECTED -- not re-examined as the engine, scale system, PPQN, and
tuning work all changed around them. Same risk profile as the Mode B regression: "compiles + runs" but
semantics may have drifted. A shipped-broken quantizer mode is a bad first impression.

## Why this is a PRE-RELEASE item (not post-library)
Unlike most microtonal work (post-library), C/D are EXISTING functionality that could ship broken. They
must be verified/fixed BEFORE release. Flag on MASTER_PLAN critical path.

## Doubly exposed: they're in the TUNING/SCALE path
C/D are QUANTIZER modes -> they quantize to the active scale/tuning. So they interact directly with:
- the scale mask (12-bit now, N-bit under the microtonal work),
- the tuning table (12-TET now; Monsoon Micro will define it for A/B/E -- MUST also cover C/D).
So two jobs: (1) fix any C/D neglect regressions pre-release in the CURRENT 12-TET world; (2) ensure C/D
consume the SAME tuning table the Micro defines (a quantizer must quantize to the loaded tuning, not
12-TET). Item (1) is pre-release; item (2) rides with the microtonal work but the DESIGN must not forget
C/D (a Micro that retunes A/B/E output but leaves C/D quantizing to 12-TET would be incoherent).

## Pre-release checklist (Rack + verify)
- Drive Mode C: external CV into CV2, clock running -> quantizes to active scale on clock edges? Correct
  scale degrees? Respects scale mask (disabled degrees skipped)?
- Drive Mode D: external CV into CV2, Gate 2 rises -> quantizes on gate? Same correctness.
- Do the LANTERN / outputs show correct quantized pitch (Lantern-vs-scope check applies here too)?
- Does changing the scale (Shophouse / scale mask) update C/D quantization live?
- PPQN interaction: C is clock-driven -- does the 24/48/96 PPQN change affect C's clock edges correctly?
- Regression-test candidate: like Mode B, add a standalone engine test for C/D quantization (feed CV,
  assert quantized-to-nearest-enabled-degree, assert mask respected). Encodes the semantics so future
  refactors can't silently break them.

## Meta
Same lesson as Mode B: modes that aren't exercised drift silently. C/D have been neglected longer.
Circle back BEFORE release; and when the Micro/tuning-table lands, make C/D quantize to that table.
