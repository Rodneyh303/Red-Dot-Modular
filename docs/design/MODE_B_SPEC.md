# Mode B (Seq + Gate) -- SPEC

STATUS: executable spec. Sections marked [DECISION NEEDED -- RODNEY] are open design questions that
need Rodney's call before Claude Code implements. Everything else is a definite instruction.
Debug history: MODE_B_GATE_REGRESSION.md. This spec supersedes MODE_B_COHERENT_PASS.md (which was a
discussion doc).

## 1. What Mode B is
External Gate 1 drives the sequencer. One Gate 1 RISE = one step. The note's DURATION is the external
gate, NOT internal Note Length / Variation. But REST and LEGATO still apply -- Mode B keeps the same
rest and legato behaviour as Mode A, only the note LENGTH source changes (external gate instead of
nvIdx/variation).

## 2. What transfers UNCHANGED from Mode A
Mode B calls the same executeStep(...) as Mode A. Keep these identical to Mode A:
- REST: the structural rest roll (canRest + restProb + r_rest). REST punches a hole (gate low that
  step) exactly as in Mode A. Rest logic does NOT depend on note length -> maps directly. DEFINITE.
- ACCENT: unchanged. DEFINITE.
- Pitch generation (genPitchLive), accent strand, variation strand read for pitch: unchanged. DEFINITE.
- The direction-correct predecessor test (prevPlayedSounded / prevSlur leading-edge model): the
  DECISION of whether a note connects is the same. What differs is only the DURATION of the connection
  (section 4). DEFINITE that the connection DECISION transfers; the duration is the open question.

## 3. Note LENGTH -- RESOLVED (follows from MODEL 1)
DECISION (Rodney): MODEL 1 legato is chosen (section 4), which resolves this: the gate FOLLOWS GATE 1's
actual high/low width. A non-legato note's gate drops when Gate 1 drops, leaving a gap before the next
rise = a clean re-articulation. This is Option B (gate-width), NOT Option A (fixed 1-step) -- because the
GAP between gates is exactly what makes legato meaningful (legato = suppress the drop, bridge the gap).
So there is NO separate internal note-length in Mode B: the note length IS Gate 1's width.
- Internal Note Length / Variation params: fully NULLIFIED (no effect on gate/note duration).
- gs.holdRemain must reflect "gate open while Gate 1 high" -- NOT a fixed nvIdx. Implementation: drive
  the gate/hold state from Gate 1's level (high = gate open, low = gate closed) rather than from a
  note-length countdown. The controller's noteVal is irrelevant to duration now (can stay any value; it
  no longer feeds length). Confirm nvIdx isn't used for anything else in Mode B (pitch? no -- pitch is
  genPitchLive; nvIdx is length only). If nvIdx is length-only, Mode B simply bypasses the length
  countdown and ties the gate to Gate 1.
- ALL read-paths (GATE_OUTPUT, STEP, poly, CV, Lantern) read this same gate-follows-Gate1 state -> they
  agree by construction (fixes the shipped divergence).

## 4. LEGATO / TIE -- RESOLVED: MODEL 1 (bridge gate high to next gate rise)
DECISION (Rodney): MODEL 1. Legato in Mode B means: at Gate 1 FALL, if this note committed to slur
forward (gs.slurForward, the same leading-edge onset commitment as Mode A), DO NOT drop the gate -- hold
it HIGH across the gap until the next Gate 1 RISE. The next step then decides its own articulation. No
advance length knowledge needed (this is why MODEL 1 works where Mode A's known-length model doesn't).

HOW THE "CONTINUE THE SLUR?" DECISION IS MADE (Rodney's question -- answered by Mode A's existing
leading-edge model, reused verbatim):
The decision is NOT made at the gate gap. It is a LEADING-EDGE commitment made at each note's ONSET,
exactly as Mode A does it (SequencerEngine.cpp:568-569, gs.slurForward = r_legato_tie < legatoProb):
- When gate N RISES, note N rolls its OWN slurForward (a fresh r_legato_tie < legatoProb roll). This is
  N's commitment: "I intend to hold my gate forward into the next note."
- When gate N FALLS: if N's slurForward is set, DON'T drop the gate -- bridge high toward gate N+1.
  If not set, drop the gate at N's fall (gap before N+1 = re-articulation).
- When gate N+1 RISES, note N+1 does TWO things (both identical to Mode A):
  (1) reads prevSlur = N's slurForward -> "am I a legato/tie DESTINATION connected back to N?" (the join
      is confirmed here; if yes, no re-attack -- legato moves to a new pitch, tie holds same pitch);
  (2) rolls its OWN slurForward -> "do I extend the chain forward to gate N+2?"
So the chain continues note-by-note: each note at ITS onset decides whether IT reaches forward. The chain
BREAKS at the first note whose slurForward roll fails (gate drops at that note's Gate 1 fall) or at a REST.
This needs NO advance length knowledge and NO new logic -- it is Mode A's leading-edge slur cascade, with
the ONLY change being that the held gate rides to the next GATE 1 RISE instead of to a known step edge.

Resolved sub-questions:
(a) MODEL 1 confirmed.
(b) LEGATO vs REST: REST WINS. If the next step is a REST, a pending slur resolves to gate LOW at that
    step (the bridge is cancelled -- rest punches its hole even if the previous note wanted to slur into
    it). Same as Mode A: a rest is never connected into.
(c) 3-NOTE TIE CHAINS: SUPPORTED naturally. Keep holding the gate high across MULTIPLE gaps as long as
    each successive note also commits slurForward. The chain breaks at the first note that does NOT
    commit slurForward (gate drops at its Gate 1 fall) or at a REST (gate low). No special-casing --
    it falls out of "bridge each gap where slurForward is set".
(d) COUPLING with section 3: resolved together. Gate follows Gate 1 width (section 3) => non-legato
    notes leave a gap at Gate 1 fall (re-articulation); legato (MODEL 1) bridges that gap. Consistent.

The connection DECISION (whether this note slurs forward) is UNCHANGED from Mode A: gs.slurForward is set
at onset by the r_legato_tie roll + legatoProb, via the same leading-edge cascade. ONLY the DURATION
model differs (bridge-to-next-gate instead of hold-for-known-length). So Mode B reuses Mode A's legato
DECISION machinery verbatim; it changes only what happens to the GATE between the decision and the next
step: Mode A rides the gate to a known boundary; Mode B rides it to the next Gate 1 rise.

## 5. ALL READ-PATHS AGREE -- RESOLVED: one source of truth, drop the separate override
DECISION: section 3 ties the gate state to Gate 1 at the STATE level (holdRemain/gateHeld follow Gate 1,
legato bridges). Once the internal state is correct, every read-path (GATE_OUTPUT, STEP, poly, CV,
Lantern) reads that ONE correct state. So CC's separate GATE_OUTPUT-override (87eaaac) should be REMOVED
in favour of the corrected internal state -- one source of truth, no divergent override. (CC's override
was the right stopgap to prove the output could follow Gate 1, but the proper fix is at the state layer,
after which the override is redundant and risks re-introducing divergence.)
[VERIFY in Rack -- Claude Code]: after wiring the internal gate state to Gate 1 + MODEL 1 legato, confirm
GATE_OUTPUT still follows Gate 1 correctly WITHOUT the override. If some path still needs the output set
explicitly, keep it but source it from the SAME state, never a separate computation.

## 6. THE TEST (definite -- write this regardless of the decisions above)
Standalone engine-level test in test/ (container, no Rack). Once sections 3-4 are decided, encode them as
assertions. Structure:
- Feed a sequence of Gate 1 rises; assert one step advance per rise.
- Assert internal note length is nullified: holdRemain never spans multiple gates REGARDLESS of noteVal
  param (the note-value knob has no effect on length in Mode B).
- REST step -> gate low that step; rest punches holes.
- LEGATO (per the chosen model) -> assert the gate bridges per the model's rule; assert rest still wins.
- ACCENT flag set when accented.
- CRITICAL: assert the Lantern-read state (gateHeld/holdRemain/lastNoteType) equals the output-path
  state -- they cannot diverge. This is the guardrail that catches the shipped bug class.

## Summary -- ALL DECISIONS RESOLVED (Rodney chose MODEL 1)
1. Section 3: gate FOLLOWS GATE 1 WIDTH (no internal note-length); the gap between gates = re-articulation.
2. Section 4: MODEL 1 legato -- bridge gate high from Gate 1 fall to the next Gate 1 rise; rest wins;
   3-note tie chains supported naturally; reuses Mode A's slurForward DECISION machinery, changes only the
   duration model (bridge-to-next-gate, not hold-for-known-length).
3. Section 5: drop CC's separate override; drive the gate state from Gate 1 at the STATE level so all
   read-paths agree by construction (verify in Rack).
The spec is now fully executable. Claude Code: implement sections 3-5, write the section-6 test encoding
these rules. The whole thing rests on ONE idea -- in Mode B the gate follows Gate 1, and legato is the
single modifier that bridges the gap to the next gate.

## IMPLEMENTATION DECISIONS (resolved, Aug 2026)

### Decision 1: nullify note-length via nvIdx, NOT inside executeStep
Rodney's ruling (lean A): Mode B simply doesn't arm a multi-step hold. `executeStep` stays
mode-agnostic. Suppression happens upstream at the nvIdx selection in `executeModeB`.

Code change in `executeModeB` (SequencerEngine.cpp:682):
```cpp
// OLD:
int nvIdx = getNoteLenIdx(noteVal, input, r_vary);
// NEW (Mode B: note-length nullified; gate follows Gate 1, not holdRemain):
int nvIdx = getNoteLenIdx(1.f, input, r_vary);
```
`executeStep` receives a 1-step nvIdx. `triggerNote` arms a 1-step `holdRemain`. The gate-close
mechanism is then entirely driven by Gate 1's fall (Decision 2 below). One-line change, no new code
path in `executeStep` or `triggerNote`.

### Decision 2: per-sample gate driver lives in Monsoon::process
File: `src/Monsoon.cpp` -- the block CC added at ~line 708, just before
`outputGenerator->drive(engine, outputs.data(), expanderManager, args.sampleTime)`.
Gate 1 and modeSelect already live there; the driver sets engine.gs.gateHeld directly so every
read-path (GATE_OUTPUT via gs.process(), STEP via gsStep, Lantern via gs.gateHeld/holdRemain)
agrees by construction.

CC's implementation:
```cpp
if (modeSelect == 1 && runGateActive) {
    const bool gate1High = input.gate1 >= 1.0f;
    const bool isRest    = (engine.lastStepResult.decision == MonoDecision::Rest);
    engine.gs.gateHeld     = isRest ? false : (gate1High || engine.gs.slurForward);
    engine.gsStep.gateHeld = isRest ? false : gate1High;
}
```

### REMAINING BUG: holdRemain not zeroed -- Lantern still shows note through rest
Lantern.cpp:347:
```cpp
const bool sounding = gs.gateHeld || gs.holdRemain > 0.0001f;
```
The Lantern treats a note as sounding if EITHER gateHeld OR holdRemain > 0. CC's driver correctly
sets gateHeld=false on rest, but executeStep re-arms holdRemain via triggerNote AFTER the clear-
before-executeStep block. So holdRemain stays positive, sounding stays true, and Lantern shows the
note playing through the rest even though the output gate is correctly low.

Symptom in Rodney's screenshot: rest visible in scope (output correct) but "hardly in Lantern"
(holdRemain keeping sounding=true through the rest).

FIX -- extend the Mode B gate driver block in src/Monsoon.cpp:~708 to also zero holdRemain when
the gate is closed:
```cpp
if (modeSelect == 1 && runGateActive) {
    const bool gate1High = input.gate1 >= 1.0f;
    const bool isRest    = (engine.lastStepResult.decision == MonoDecision::Rest);
    const bool gateOpen  = !isRest && (gate1High || engine.gs.slurForward);

    engine.gs.gateHeld     = gateOpen;
    engine.gsStep.gateHeld = !isRest && gate1High;

    // CRITICAL: zero holdRemain when gate is closed so Lantern's
    // `sounding = gateHeld || holdRemain > 0.0001f` (Lantern.cpp:347) stays consistent.
    // executeStep re-arms holdRemain via triggerNote; this overrides it.
    if (!gateOpen) {
        engine.gs.holdRemain     = 0.f;
        engine.gsStep.holdRemain = 0.f;
    }
}
```
This is the minimum fix -- one additional condition, two zero-assignments added to the existing
block. No other files need changing for this specific symptom.

### REMAINING ISSUE: Lantern misses most rests (processing-order timing bug)
After the holdRemain fix, the scope is correct but Lantern still misses most rests.

ROOT CAUSE: Rack processes modules sequentially. If Lantern runs BEFORE Monsoon in a given audio
block, it sees the state from the PREVIOUS Monsoon block (where triggerNote ran and set gateHeld=true).
By the time Monsoon runs and the gate driver sets gateHeld=false for the rest, Lantern has already
sampled. Next block, stepIndex hasn't changed, so Lantern early-returns without sampling. The rest's
corrected gateHeld=false state is NEVER OBSERVED by the Lantern.

Lantern.cpp:237: `if (step == lastObservedStep) return;` -- samples only on stepIndex change.
On the next gate rise, stepIndex changes again but a new note fires, so gateHeld is back to true.
The rest window is invisible to the Lantern because of this processing-order race.

FIX: in recordCell (Lantern.cpp:~347), trust `dec` (MonoDecision) BEFORE checking gateHeld/holdRemain.
`dec = eng.lastStepResult.decision` is set atomically inside executeStep and persists until the next
step -- so both Lantern and the gate driver see the same value regardless of processing order.

ADD ONE LINE in `src/Lantern.cpp` at recordCell (~line 347), BEFORE the sounding check:
```cpp
// Trust the decision field over timing-sensitive gateHeld/holdRemain (processing-order race).
if (dec == MonoDecision::Rest) { c.type = lantern::NoteType::Inactive; return; }
const bool sounding = gs.gateHeld || gs.holdRemain > 0.0001f;   // existing line
```

This is the same principle as the core Mode B debugging lesson: when Lantern and scope disagree,
the internal state has diverged from output. The decision field is the authoritative state; gateHeld
and holdRemain are timing-sensitive consequences of it that the Lantern may miss.

Note: this fix is GENERAL (applies to all modes) and harmless outside Mode B -- in Mode A/E the
decision IS Rest iff the gate is truly down, so the early-return produces the same result as the
sounding check would. In Mode B it is load-bearing because gateHeld may be momentarily wrong at
sample time due to the processing-order race.

### CC's poly concern -- REJECTED (model is the opposite of what CC stated)

CC flagged that `if (dec == MonoDecision::Rest) return Inactive` would "blank a poly voice that's
correctly sounding during a mono rest." This is wrong about the model.

The actual poly rest rule (SequencerEngine.cpp:729 comment + :843-847 enforcement):
- "Rest -- mono rested; poly CANNOT INITIATE A NEW NOTE this step."
- On MonoDecision::Rest, the engine explicitly sets v.gs.gateHeld=false and v.participating=false
  for every poly voice.

The direction of the rule: poly CAN REST when mono plays (independent restProb). Poly CANNOT PLAY
when mono rests. This is the opposite of CC's claim.

Therefore when dec==MonoDecision::Rest, every voice -- mono and poly -- already has gateHeld=false
enforced by the engine. The spec's one-line fix applied to all voices is CORRECT and SAFE. CC's
proposed "safe refinement" to voice 0 only would be less correct -- if a poly voice had stale
gateHeld=true from the processing-order race, the fix wouldn't cover it.

The Lantern.cpp:341-346 comment CC cited warns about a different class of bug (poly voice that
independently rests while mono plays -- correct: that voice should render Inactive even though
dec != Rest). That comment does not contradict the spec's fix; it addresses the opposite case.

PROCEED with the original one-line fix as specified.
