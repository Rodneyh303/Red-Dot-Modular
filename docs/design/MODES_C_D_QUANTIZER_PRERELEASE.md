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

## FUTURE: add a PHASE-based quantizer mode (Rodney -- note to come back to)
The current quantizer side has C (clock-driven) + D (gate-driven). The sequencer side has A (clock) + B
(gate) + E (phase). The quantizer side is MISSING the phase-driven equivalent. Add it -- completes the
timing symmetry.

### Proposed shape (Reading 1 -- likely what Rodney means)
A phase-driven quantizer mode -- provisional name Mode F: quantizes external CV on PHASE EDGES from an
external phase ramp, the same way Mode E generates on phase edges. Same relationship to C/D as Mode E has
to A/B. Timing sources across the module become:
             CLOCK       GATE        PHASE
  Generate:  A           B           E
  Quantize:  C           D           F (new)
Fits the crab-canon / external-phase-drive work: phase drives quantization the same way it drives
generation, keeping the phase paradigm coherent across all modes. A single external phase source could
drive BOTH a generative Monsoon (Mode E) and a quantizing Monsoon (Mode F) in lockstep.

### Reading 2 alternative (flag for Rodney to disambiguate)
If instead Rodney meant "phase MODULATES the quantization behaviour" (phase position within a cycle
interpolates between scales, or shifts which degrees are active as phase advances) -- that's a different
mode: phase as a modulator of quantization, not as its trigger. Structurally more novel but less
symmetrical with the existing mode grid.
=> Reading 1 lean (completes the symmetry, matches Mode E's role on the sequencer side).

### Scope + timing
- Not pre-release (unlike the C/D neglect pass). This is a NEW mode -- add after C/D are verified working,
  and ideally after the microtonal work so it consumes the shared tuning table from the start.
- Same tuning-table integration as C/D: quantize to the active tuning (Micro-defined when attached, else
  12-TET) -- MUST NOT hardcode 12-TET, per MICRO_TUNING_INTEGRATION_PLAN.
- Implementation likely mirrors Mode E's phase-edge machinery + C's quantize call: on each phase-edge
  event, quantize inCV to the nearest enabled degree in the tuning table, output on the same paths C/D
  use. Should be a small mode by lines of code -- most of the machinery is already there (phase edges
  from Mode E, quantize from C/D).

### What to check when building
- Phase-edge source: same input as Mode E's phase drive? Or a separate phase input for the quantizer?
  Likely reuse (one phase ramp, both modes tap it) -- decide at build.
- Interaction with the crab canon's phase-drive scenarios: a phase-quantized Monsoon fed by the same
  ramp as a phase-generative Monsoon should stay in lockstep.
- Test: standalone quantizer test with a phase-edge sequence, assert quantize-on-edge behaviour + tuning-
  table respected + mask respected (same guardrails as C/D).
