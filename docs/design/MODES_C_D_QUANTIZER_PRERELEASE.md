# Modes C & D (quantizer modes) -- NEGLECTED, need a pre-release pass

## CODE-VERIFIED STATUS (2026-09-03, from source on `master` -- read, not built)
A code-truth pass against the source (per the "grep the code, not the docs" meta-principle). Verifies
STRUCTURE/LOGIC only -- runtime behaviour in Rack is unverified (container can't build the SDK; that leg
is a Claude Code job). The doc body below is the original brief, kept for the checklist + Mode-F design.

- **C/D are REAL and current**, not stubs: `SequencerEngine::executeModeC` (clock / quarterEdge) and
  `executeModeD` (gate2-high) at `src/dsp/engines/SequencerEngine.cpp:993-1017`, dispatched from
  `MonsoonModeController`. NOTE: the body's cited line numbers (`MonsoonModeController.cpp:195-215` etc.)
  have DRIFTED -- symbols correct, line numbers stale.
- **Concern (2) -- microtonal tuning-table integration: DONE in code.** `quantize()` snaps via
  `pe.tuning.isDefault12TET ? s/12 : pe.tuning.degreeVolts(s)`; `degreeOf()` mirrors with
  `nearestDegree()`. => byte-identical to legacy at 12-TET; snaps to actual published degrees under a
  Micro/Sikit custom tuning. The "incoherent Micro (A/B/E retuned, C/D still 12-TET)" risk is
  structurally CLOSED. Quantiser cache pre-sized `MAXN=24` for Micro-24. Retire this concern to DONE.
- **Concern (1) -- neglect regression: logic present & coherent, RUNTIME-UNVERIFIED.** `quantize()` is
  mask-respecting (iterates `activeSemiList`/`activeSemiWeight`, skips disabled degrees), fader-weighted
  capture radius (`w * 1/12`), octave +/-1 search with register preservation, nearest-active fallback --
  matches the octave-quantise behaviour recorded as verified in CONTEXT_RECOVERY. Structurally sound;
  still needs the doc's Rack checklist driven (CV in, Lantern/scope out, live scale change, PPQN edges).
- **Regression test PARTIALLY closed (update 2026-09-03, commit d1b331c).** `test/test_modes_bcd.cpp`
  (697 lines) covers C/D step/gate/legato/rest at 12-TET. `test/test_modes_cd_microtonal.cpp` (NEW, 117
  lines) now adds **microtonal-INPUT** coverage -- off-grid CVs (1/6, 1/12, 1/24 semitone) landing on the
  nearest degree within half a semitone. STILL OPEN, though: both tests use a **12-weight mirror**
  (`quantizeToScale(vIn, weights[12])`), NOT the real `SequencerEngine::quantize`, and neither constructs
  a non-12 `TuningTable` or drives the `degreeVolts` / `isDefault12TET` branch. So the remaining gap is
  narrow + specific: a genuine non-12 tuning (e.g. 24-degree maqam) snapping via `degreeVolts`, through
  the actual function. Covered now = off-grid input vs a 12-degree scale; uncovered = non-12 tuning-table.
- **Mode F (phase-driven quantizer): genuinely UNBUILT.** No `executeModeF`; UI mode enum is exactly 5
  (`A: Sequencer ... E: Phase (CV1)`), no F slot. Future work, correctly gated behind C/D-verified +
  microtonal (see FUTURE section below).

Net: the C/D *pre-release* item is now a RUNTIME-VERIFY + (narrow) TEST-GAP item, not a broken-semantics
fix -- the microtonal integration that looked like the big risk is already implemented.

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

### Reading 1 CONFIRMED (Rodney)
Mode F = phase-TRIGGERED quantizer (phase edges fire quantization). NOT phase-as-modulator.

Rodney's reasoning for rejecting Reading 2 (phase modulating quantization behaviour):
"There's enough going on with scale, LOR mod, Interchange mod, CA, etc. for dynamically changing
quantiser." The module ALREADY has plenty of live dynamism acting on quantization -- scale mask changes,
LOR modulation, Interchange modulation, Change Alley remaps. If you want quantization to be dynamic,
those existing systems provide it -- they already change what quantization does moment-to-moment. Adding
phase-as-a-modulator would be a redundant FIFTH way to modulate an already-modulated thing. Overkill,
and the wrong kind of complexity (piling modulators rather than using the ones you have).

Reading 1 is clean precisely because it doesn't touch quantization BEHAVIOUR at all -- it just adds a
TIMING SOURCE (phase edges) for when quantization fires. The BEHAVIOUR of quantization stays governed by
the existing dynamic systems (scale mask, LOR, Interchange, CA); Mode F just says "quantize now, driven
by phase" and downstream is the same code that dynamically shapes the result. Mode F is a TRIGGER mode,
not a modulation mode.

This is the same design instinct that runs through the module's coherence:
- Mode B legato = reuse Mode A's slurForward, don't invent new "unknown-length legato".
- Shared CA offset = put on CA where its key lives, don't add a new arbitrator.
- Micro tuning table = one shared structure all modes read, don't add per-mode tuning.
- Mode F = use existing quantization behaviour, just add a phase trigger.
Compositional principle: "the machinery already does the interesting thing -- the new mode just adds a
new entry point to it." Modes are timing sources; behaviour lives in the shared systems the modes call.
This is why the module keeps gaining capabilities without turning into a tangle of interacting features.

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
