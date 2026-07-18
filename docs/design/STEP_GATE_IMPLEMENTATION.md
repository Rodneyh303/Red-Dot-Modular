# STEP GATE — implementation spec (engine, MSYS2)

Companion to LEGATO_TIE_MODEL_NOTE.md. That note establishes WHAT STEP GATE is
(GATE = legato applied / STEP GATE = legato removed; same pattern, two gate streams).
This note is the WHERE and HOW: the exact points in the engine that differ, and the
minimal change. Container can't build the Rack SDK, so this is a diff plan to execute
and test on the build, not applied code.

## The one-sentence model, in engine terms
The fused gate and the un-fused (step) gate are produced by the SAME code path and
differ at EXACTLY three call sites: the articulation methods that FUSE a continuation
instead of re-striking it. Where the engine calls `slideNote` / `extendHold` / `slideMax`
(legato / tie / legatoMax), the step gate wants a `triggerNote` instead. Nowhere else.

## Where GateState is set (the fusion happens here, nowhere else)
`GateState` (src/dsp/gates/GateState.hpp/.cpp) has four articulation methods. Fusion is
entirely the difference between the first and the other three:

| method        | retrigger?              | hold                     | NoteType | fused? |
|---------------|-------------------------|--------------------------|----------|--------|
| `triggerNote` | YES (`gatePulse.trigger`)| `holdRemain = dur` (reset)| Single   | NO — a fresh gate edge |
| `slideNote`   | no (when wasHeld)       | `holdRemain += dur`       | Legato   | YES — extends, no edge |
| `extendHold`  | no                      | `holdRemain += dur`       | Tie      | YES — extends, no edge |
| `slideMax`    | no                      | `holdRemain = dur`        | Legato   | YES — no edge          |

So "fusion" is precisely: **a continuation that calls slide/extend instead of trigger**.
It suppresses the retrigger pulse AND rolls the hold forward, so the gate never falls.
The step gate is the counterfactual: what the gate would be if every continuation were a
`triggerNote` (fresh `dur`, retrigger, gate falls+rises at the boundary carrying its own
length).

## Where the decision is made (the callers)
`src/dsp/engines/SequencerEngine.cpp`, two paths:

- **MONO** (~lines 455–498). The if/else picks the method:
  - `slideMax` → LegatoMax (line ~457)
  - `extendHold` → Tie (line ~489)
  - `slideNote(…, wasHeld=true)` → Legato (line ~492)
  - `triggerNote` → NewNote (line ~497)
- **POLY per-voice** (~lines 815–835, the Rule-2 landing), plus the follow-mono path
  (~848–858). Same four methods, chosen per voice by `connect`/`isTie`.

These are the ONLY sites that fuse. `MidNote`/hold branches don't call an articulation
method (the note simply continues), so they need no step-gate handling — the step gate,
like the main gate, just keeps counting down whatever was last armed.

## The change: a parallel "step" GateState per voice
STEP GATE needs its own gate countdown because its edges fall where the fused gate's don't.
The cleanest, lowest-risk form is a SECOND GateState that receives the SAME calls EXCEPT
continuations are re-struck. Mirror, don't reinterpret.

### 1. Add a step gate-state next to each gate-state
Wherever a voice owns `GateState gs`, add `GateState gsStep`. For mono that's `engine.gs`
→ add `engine.gsStep`. For poly that's `voices[i].gs` → add `voices[i].gsStep`.
(`reset()` both together; `tick`/`tickPulse`/`process` both together with the same args.)

### 2. At each articulation site, drive gsStep with the UN-FUSED choice
The rule is mechanical — map the fused call to its articulated twin:

    fused (gs)                          step (gsStep)
    ───────────────────────────────     ──────────────────────────────────
    gs.slideNote(pitchV, sem, nv, true) gsStep.triggerNote(pitchV, sem, nv)
    gs.extendHold(sem, nv)              gsStep.triggerNote(gs.currentPitchV, sem, nv)
    gs.slideMax(pitchV, sem, nv)        gsStep.triggerNote(pitchV, sem, nv)
    gs.triggerNote(pitchV, sem, nv)     gsStep.triggerNote(pitchV, sem, nv)   (same)
    gs.gateHeld = false (Rest)          gsStep.gateHeld = false                (same)

i.e. **gsStep ALWAYS triggerNote on a played step**, mirroring the pitch/semitone/nv the
fused path used, and mirrors Rest by closing. It never slides or extends. Its edges
therefore fall at every sub-note boundary, each carrying that sub-note's own `dur` — which
is exactly the "legato removed" stream.

Note on `extendHold`: the tie case has no fresh pitch (same pitch held), so gsStep
re-strikes at `gs.currentPitchV` — the pitch is unchanged, only the gate re-articulates.
That is the whole point of STEP GATE on a tie: same pitch, but a gate edge for the VCF.

### 3. Tick/process gsStep in lockstep
Everywhere the engine calls `gs.tick(p16)` / `gs.tickPulse()` / `gs.process(dt)`, call the
same on `gsStep` with identical arguments. gsStep must see the same grid so its countdown
is aligned. (Search the executeStep / per-sample loop for the gs tick/process calls and
duplicate for gsStep.)

### 4. Emit it
`src/dsp/managers/MonsoonOutputGenerator.cpp`:
- **Mono** STEP_GATE_OUTPUT: replace the placeholder `float stepGateV = gateV;` with
  `float stepGateV = engine.gsStep.process(sampleTime);` (mirror how gateV is obtained;
  if gateV is read post-process from GATE_OUTPUT, instead read gsStep's processed value).
  Then `setGateWithMute_(outputs[STEP_GATE_OUTPUT], stepGateV, muted);` (already wired).
- **Poly** POLY_STEP_GATE_OUT (Straits, 16ch): in the per-voice loop, alongside
  `float vg = engine.voices[i].gs.process(sampleTime);` add
  `float vgStep = engine.voices[i].gsStep.process(sampleTime);` and
  `stepGateOut.setVoltage(vgStep, ch);`. ch0 = mono mirrors `engine.gsStep`. Set channels
  to 16 like the others; voices beyond the active count output 0 (same guard as gate).

## Why a second GateState, not a flag or a reinterpretation
- **A flag ("would-articulate") isn't enough**: the step gate needs its OWN countdown,
  because after a re-strike its gate-close pulse is armed to the sub-note's dur, which
  differs from the fused gate's summed hold. Two gates, two `gatePulseRemain`. A single
  bool can't carry a second countdown.
- **Don't reconstruct from `lastNoteType`/decision at emission**: that's the rejected
  approach (a classification, not a gate). The mirror-GateState reuses the exact, tested
  arming/tick/close machinery — the step gate is "the same engine, minus fusion", so it
  inherits all the PPQN-grid correctness (triplets, 1/32, etc.) for free.

## Cost / risk
- Memory: one extra `GateState` per voice (16) + mono. Small.
- CPU: a second `process()`/`tickPulse()` per voice per sample. Measure, but it's the same
  cheap integer countdown; likely negligible.
- Risk is localised: gsStep is WRITE-only from the articulation sites and READ-only at
  emission. It never feeds back into decisions, pitch, accent, or the Lantern. If gsStep is
  wrong, only the STEP jack is wrong; GATE/CV/ACCENT and all musical decisions are
  untouched. That containment is the reason for the mirror design.

## Test plan (on the build)
1. **Non-legato pattern**: STEP == GATE sample-for-sample (nothing to un-fuse). This is the
   degenerate-case guarantee from the model note; if they differ here, gsStep isn't being
   driven identically to gs on fresh notes.
2. **1/4 legato → 1/8 → 1/16**: GATE one continuous high across the span; STEP three
   separate gates of 1/4, 1/8, 1/16 length. Scope both.
3. **Tie (same pitch, 2 steps)**: GATE one 2-step hold; STEP two 1-step gates at the same
   pitch (CV flat, STEP re-articulates).
4. **Poly**: per-voice — voice A slurs while voice B is staccato; A's channel shows fused
   GATE + articulated STEP, B's shows identical GATE and STEP.
5. **The patch**: VCA off GATE, VCF off STEP → one amplitude swell, three filter strikes.

## Retirement note
Once STEP GATE emits correctly, the `NoteType lastNoteType` field and the Lantern's use of
it are UNAFFECTED (STEP GATE doesn't read them). But confirm nothing else still reads the
removed TIE/LEGATO/TIE_OR_LEGATO outputs (grep already clean in the widget/enum; this is a
belt-and-braces check after the engine change).
