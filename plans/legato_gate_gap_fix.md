# Legato gate-gap fix — implementation plan (feat/subgate-poly-width)

Source of truth: [`docs/design/LEGATO_GATE_GAP_BUG.md`](../docs/design/LEGATO_GATE_GAP_BUG.md:1).

## The bug (from the doc)
`wasHeldMono = gs.gateHeld || holdRemain>0` proxies NOTE DURATION, not gate adjacency. A long
note-value keeps `holdRemain>0`, so a much later gate still reads `wasHeld=true` and ties across an
audible gap. The `d0b8ea4` attempt cleared held-state UNCONDITIONALLY at every rise — which zeroed
`wasHeld` for EVERY gate and killed all legato (even overlap). Two acceptance conditions the failed
fix could not satisfy TOGETHER:
1. overlapping / tight (<1ms) gates → TIE candidate,
2. clearly-separated (≥1ms low) gates → FRESH note.

## Threshold (sourced): 1ms
Andrew Belt / VCV — 1ms is the minimum gate length all standard modules detect. `= sampleRate/1000`
samples, computed at RUNTIME. Overlap (gate still high at the rise) short-circuits to adjacent with
no measurement (this is what ties a 99%-duty LFO / Rampage back-to-back falls).

## Where the measurement lives — MODULE layer (per-sample), not the engine
`executeModeB` runs ONLY on a rise (dispatch gates `shouldExecute` on the edge), so it cannot
accumulate low-time itself. The per-sample accumulator lives in `Monsoon::process` (runs every
sample) and is passed into the engine as ONE bool.

### State (Monsoon.hpp)
```
float gate1LowSamples = 0.f;   // samples since Gate 1 last fell (reset on the falling edge)
```
Reverse-safety: this is EDGE-timing (reset on the FALL, accumulate while low), NOT a forward-only
step counter — direction-agnostic, matching the reverse caveat at SequencerEngine.cpp ~484. A
bitwise fwd/reverse test with the gap logic active must still pass (the accumulator is identical in
both directions because it keys off the physical gate, not the play index).

### Per-sample update (Monsoon::process, Mode B, before dispatch uses it)
Uses the Schmitt-triggered level (already added: `tc.getGate1SchmittHigh()`).
```
const bool g1 = tc.getGate1SchmittHigh();
if (g1) gate1LowSamples = 0.f; else gate1LowSamples += 1.f;
// adjacency for THIS rise: overlap OR sub-1ms low. Evaluated at the rise inside the engine call:
//   adjacent = (low-duration < sampleRate/1000)  — overlap gives low-duration 0 (reset while high).
const float oneMsSamples = args.sampleRate / 1000.f;
const bool gate1Adjacent = gate1LowSamples < oneMsSamples;   // pass into the engine
```
NOTE: because the accumulator resets while HIGH, at the first low sample after a fall it is small and
grows; a rise that arrives after <1ms low reads `gate1Adjacent=true`, after ≥1ms reads false. Overlap
never accumulates (stays 0) → always adjacent. Exactly the doc's rule.

## Engine change — executeModeB (SequencerEngine.cpp ~713)
Add a param `bool gate1Adjacent` (default true so existing callers/tests are byte-identical).
The clear that currently runs UNCONDITIONALLY after capture (`gs.holdRemain=0; gatePulseRemain=-1`,
~771) stays as-is (it only nullifies internal LENGTH, needed for the MidNote-swallow fix, and runs
AFTER capture so it does NOT touch `wasHeld`). The NEW gap clear runs BEFORE the `wasHeldMono`
capture (~743) and ONLY on a genuine gap:
```
// Gate-gap adjacency (LEGATO_GATE_GAP_BUG.md): a rise after a >=1ms low is a real separation —
// clear the held state BEFORE wasHeld capture so wasHeldMono reads false -> fresh note. Overlap /
// sub-1ms leaves it -> wasHeld stays true -> legato candidate (still subject to legatoProb/prevSlur).
// This replaces note-DURATION as the adjacency proxy with actual gate timing.
if (!gate1Adjacent) {
    gs.gateHeld = false; gs.holdRemain = 0.f;
    // slurForward is the leading-edge commitment; a real gap also breaks the intended slur.
    gs.slurForward = false;
    gsStep.gateHeld = false; gsStep.holdRemain = 0.f;
    for (int i = 0; i < numPolyVoices; ++i) {
        voices[i].gs.gateHeld = false; voices[i].gs.holdRemain = 0.f;
        voices[i].gs.slurForward = false; voices[i].participating = false;
        voices[i].gsStep.gateHeld = false; voices[i].gsStep.holdRemain = 0.f;
    }
}
```
Placed right after the `wrapped && boundaryInterrupt` block (same shape as that clear) and BEFORE
`float prevHold = gs.holdRemain;` at ~742.

### Why this satisfies "add the adjacency term to BOTH sites (~512, ~538)"
Both `slurReachesHere` (~512) and the commit branch (~538) gate on `(wasHeld || hadTail)`. By making
`wasHeld` itself carry adjacency (the pre-capture clear), BOTH sites inherit the term with NO separate
edit — `executeStep` stays mode-agnostic (the doc's consistency requirement met structurally). We do
NOT thread a flag into executeStep; the single point of truth is `wasHeldMono`.
NOTE on `hadTail`: a fractional tail (triplet) is a DIFFERENT adjacency (a real sounding tail bridging
a sub-step) and correctly still allows connect — the gap clear only zeroes `holdRemain` on a ≥1ms gate
gap, which by construction has no fractional tail (Mode B nvIdx=1 step, integer). So `hadTail` is
untouched and its legato path is preserved.

## Threading gate1Adjacent through the call chain
- `SequencerEngine::executeModeB(bool gate1Rise, bool gate1High, bool gate1Adjacent, ...)` — new param,
  default `true`.
- `SequencerEngine::executeModeBSubdivided(...)` — subGate path: the subdivision edges are the fine
  grid; adjacency there is the SUBGATE adjacency, out of scope for THIS bug (which is plain gate mode).
  Pass `gate1Adjacent=true` (unchanged) from the subdivided path — ratchet/ghost cells are intentionally
  adjacent. Only the NON-subdivided executeModeB gets the gap gate.
- `ModeController::executeModeB(const InputState&, bool useSubGate)` — read `input.gate1Adjacent`
  (new InputState field) and pass it to the non-subGate `engine.executeModeB(...)`.
- `ModeController::executeModeD(...)` — Mode D (quantiser) also calls executeModeB in the non-subGate
  path; pass `gate2Adjacent` computed the same way from Gate 2, OR pass `true` (Mode D is CV-sampled,
  legato-across-gap is not the reported symptom). LEAN: pass `true` for Mode D this pass; the bug is
  gate-mode (Mode B/gate1). Confirm no Mode D regression.
- `InputState` (Monsoon.hpp) — add `bool gate1Adjacent = true;` set in Monsoon::process from the
  accumulator.

## Tests (test_subgate.cpp or a new test_mode_b_gap.cpp — header-level, no Rack)
Drive executeModeB directly with the new bool:
1. **long gap** — play note A (adjacent=true), then note B with `gate1Adjacent=false` at high legato →
   B is NewNote (fresh), NOT Tie/Legato.
2. **overlap / 99% duty** — A then B with `gate1Adjacent=true`, legato committed → B is Tie/Legato.
3. **back-to-back <1ms** — same as overlap (adjacent=true) → Tie/Legato.
4. **normal adjacent phrasing** — a run of adjacent gates behaves as before (byte-identical to the
   pre-fix adjacent path: pass adjacent=true throughout).
5. **forward vs reverse bitwise** — run a fixed edge sequence forward and reversed with the gap logic
   active; assert identical decisions (the accumulator is edge-keyed, not index-keyed).

Also re-run the existing suites: test_subgate (ghost/ratchet unaffected — subdivided path passes
adjacent=true), test_mode_b_gate, test_legato_leading_edge, test_var_leg_rest, test_poly_voices.

## Commit note (flag the deliberate behaviour change)
"Mode B: note LENGTH no longer creates legato across gaps — adjacency is now judged by gate timing
(1ms, sourced to Belt), not note duration. This is the FIX for LEGATO_GATE_GAP_BUG.md, not a
regression: long gaps retrigger, overlap/<1ms ties. Do not 'restore' the old duration-proxy."

## Acceptance (the pair the failed fix couldn't satisfy together)
- [ ] overlapping / tight (<1ms) gates → TIE candidate (LFO 99% duty, Rampage back-to-back).
- [ ] clearly-separated (≥1ms low) gates → FRESH note.
- [ ] normal adjacent phrasing unchanged; fwd==reverse bitwise; all existing suites green.
