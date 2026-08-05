# Mode B external-gate regression -- investigation brief (Claude Code, needs Rack + git bisect)

## Symptom (Rodney, in Rack)
Driving a Monsoon in MODE B (seq + gate) from an external gate into G1 (GATE1_INPUT) doesn't produce
gate/step events (Lantern shows static held notes, no stepping). Mode B WORKED PREVIOUSLY -> suspected
regression. A fast VCO SQUARE WAVE into G1 DOES produce events (confirmed in Rack) -- so Mode B is not
fully dead; it responds to a clean square. The original source was another Monsoon's STEP output.

## What the code shows (verified in container, structurally intact)
- executeModeB (MonsoonModeController.cpp:161) advances on gate1Rise || (gate1High && stepIndex==-1). OK.
- Mode B is correctly EXCLUDED from gate-assignment consumption (Monsoon.cpp:538 `if (modeSelect != 1)`).
  So G1 rises are NOT being eaten by handleGate1Assignment. OK.
- gate threshold = 1.0f (gateHigh_ default). STEP output is ~10V so should cross. OK.
- **PRIME SUSPECT: the whole mode-execution block is wrapped in `if (runGateActive)` (Monsoon.cpp:573).**
  So Mode B only steps when the driven Monsoon's RUN is active. IF Mode B previously advanced on external
  gates WITHOUT requiring RUN, and now requires it, that is exactly the regression: pure gate-driving
  (no RUN pressed) would have worked before and now does nothing.

## Two hypotheses to distinguish (Rack + bisect)
H1 (RUN-gating regression): Mode B now requires runGateActive; it used to free-run on G1 rises alone.
  TEST: in Rack, Mode B, external gate into G1, RUN OFF -> no events? Then press RUN -> events?
  If yes, the regression is the runGateActive wrapper gating Mode B. FIX: Mode B (and any external-gate-
  driven mode) should step on the gate regardless of RUN, OR the external gate should imply run. Decide
  intended semantics (does an external clock/gate need RUN? arguably NO -- the gate IS the transport).
H2 (STEP-source sparse, NOT a Monsoon-B regression): the source Monsoon's STEP output only pulses on
  note steps; if that source pattern is mostly rests/held/legato notes, STEP emits few rising edges, so
  the driven Monsoon barely steps. The square-wave test works because it's dense + regular.
  TEST: scope the STEP output that was driving it -- is it actually pulsing per step, or sparse/held?
  Drive G1 from a plain clock at the same rate -> if that works, the "regression" was really the STEP
  source, not Mode B.

## How to bisect (H1)
git log the mode-execution block in Monsoon.cpp around the `if (runGateActive)` wrapper and the
shouldExecute Mode B line (591). Find when Mode B got moved inside the runGateActive guard (if it did).
`git log -S "if (runGateActive)" --all -- src/Monsoon.cpp` and inspect the diff that introduced it.
The merge history obscured it in a shallow clone -- use a full clone + `git log --follow -p`.

## Likely fix (if H1 confirmed)
External-gate-driven modes (B on gate1, D on gate2) should advance on the incoming gate EDGE regardless
of runGateActive -- the external gate is the transport, so requiring the internal RUN is wrong for them.
Either: (a) move the Mode B/D gate handling OUTSIDE the `if (runGateActive)` guard, or (b) treat a gate1
rise in Mode B as implicitly running. Preserve: internal clock modes (A/C/E) still respect RUN. Verify
the six-way: {Mode B, Mode D} x {RUN on, RUN off, external gate present} behaves as intended.

## Status
Needs Rack (observe RUN on/off behaviour) + full-history bisect. Structurally the code is intact; the
regression is most likely the RUN-gating semantics, not the Mode B logic itself.

## H3 (Rodney's hypothesis): PPQN interaction (we allowed lower PPQN before)
Checked in container:
- executeModeB is a PURE GATE-EDGE path -- advances on gate1Rise, does NOT read clock.sixteenthEdge or
  any PPQN-derived step edge (unlike Mode A/C/E which are clock/phase edge driven). And processGateEdges
  (Monsoon.cpp:524) runs UNCONDITIONALLY, outside any PPQN/clock gate. So PPQN does NOT gate whether
  Mode B STEPS. "Mode B doesn't step at all" is unlikely to be pure PPQN.
- BUT PPQN plausibly affects the gate DURATION / rendering: the gate output duration is driven by the
  gs.tick() pulse (RATE_TABLE: noteVal -> length + gs.tick pulse for duration). At low PPQN (1 or 4) the
  pulse resolution for gate duration is coarse. If something in the duration/tick path changed, a low
  PPQN could produce gates that used to render and now don't (too short / malformed) -> "no events shown
  in Lantern" even though stepping occurs. Rodney's memory ("lower PPQN allowed before") is data.
- Also: computeNoteLengthIdx(idx, ppqnMask) -- ppqnSetting is a bitmask (1=PPQN1,2=PPQN4,4=PPQN24) of
  ALLOWED note values. Mode B neutralises note value (passes 2.f, noteVariationMask=0b111) so it should
  be immune to note-length restriction -- CONFIRM this neutralisation still holds (if a regression made
  Mode B respect ppqnMask, a low PPQN could zero its usable note lengths).

TEST (do this FIRST -- Rodney's memory makes it high-value): with the external gate into G1, Mode B,
cycle ppqnSetting 1 -> 4 -> 24 and watch the Lantern.
- Higher PPQN (24) makes events appear, low PPQN doesn't -> PPQN IS involved (duration/tick resolution,
  or a regression making Mode B respect ppqnMask). Real finding -- fix the gate-duration path for low
  PPQN, or restore Mode B's PPQN-independence.
- No difference across PPQN -> not PPQN; fall back to H1 (RUN-gating) / H2 (STEP source sparse).

Bisect target if H3: git log the gs.tick / gate-duration path and computeNoteLengthIdx usage; find any
change that made Mode B's gate duration depend on PPQN or made Mode B respect ppqnMask. Relates to the
PPQN-cap-at-24 discussion (RATE_TABLE) -- confirm no cap/normalisation change broke low-PPQN gate drive.

## H3 RESOLVED: the PPQN change was DELIBERATE, not the bug
Rodney observed 24/48/96 have no effect + lower PPQN no longer selectable. Confirmed in code -- and it's
BY DESIGN:
- Menu hardcodes {24,48,96} (MonsoonWidget.cpp:976). The old low values (1,4) are intentionally gone.
- SequencerEngine.cpp:317-322 comment states it explicitly: "PPQN is now always 24/48/96 -- all resolve
  every note value to an integer pulse count (24 already covers 1/32 and all triplets). So every value
  is legal; mask = the full-resolution bit (4). (The old 1/4 PPQN settings, which gated out sub-step
  values, are gone.)" -> ppqnMask hardcoded to 4 (full resolution, everything legal).
- This is the PPQN-floor-at-24 decision from RATE_TABLE, implemented. Musically correct (24 = 2^3*3
  covers 1/32 + triplets). Note-length layer is now MORE permissive, not less.
STALE-BUT-HARMLESS: the comments at Monsoon.cpp:427-428 and NoteValues.hpp:30 still DESCRIBE the old
"1=PPQN1,2=PPQN4,4=PPQN24" bitmask. Update those comments to the 24/48/96 reality (cosmetic; the code
uses mask=4 = full-resolution correctly).
CONCLUSION: PPQN change is deliberate + correct, and the new note-length layer is permissive (everything
legal). So PPQN is UNLIKELY to be what breaks Mode B gate events. Rodney's memory was accurate (low PPQN
removed) but the removal is not the regression.

## => PRIME SUSPECT is back to H1 (RUN-gating). DO THIS TEST:
Mode B, external gate into G1, watch Lantern:
  RUN OFF -> events? ... then press RUN -> events start?
If events only flow with RUN active, the regression is the `if (runGateActive)` wrapper (Monsoon.cpp:573)
gating Mode B. Fix: external-gate modes (B/D) should advance on the gate edge regardless of runGateActive
(the external gate IS the transport). Also bisect when Mode B moved inside that guard.

## ROOT CAUSE FOUND (H4, the real one): Mode B gate-slaving is COMMENTED OUT
New symptom detail (Rodney): RUN on, sequencer runs + steps on the gate, but every note is a LONG HELD
note -- REST=0 and LEGATO=0 have NO effect. So it's NOT "no events"; it's "note SHAPING broken in Mode B".

THE BUG: the "Mode B Gate Slaving" block in Monsoon.cpp (~line 684-698) is ENTIRELY COMMENTED OUT. Its
own comment states the intent: "In Mode B (Seq + Gate), the gate duration MUST follow the external Gate 1
input. This nullifies the impact of internal Note Length/Variation parameters."
With it disabled: Mode B's gate duration uses the INTERNAL note-length machinery instead. The controller
passes a FIXED noteVal=2.f (1/4 note). If external gates arrive faster than a 1/4 note (e.g. every 1/16),
each note's internal duration SPANS MULTIPLE gate intervals -> notes hold across steps -> continuous LONG
NOTES. Rest/legato get swamped because the gate never drops long enough for them to matter. Exactly the
symptom.

WHY IT WAS COMMENTED (likely): the block references engine.g1Trig.isHigh(). That trigger object may have
been renamed/moved in a refactor, so instead of updating the reference it was commented out -- silently
removing Mode B's defining behaviour. (MonoDecision::Rest still exists: SequencerEngine.hpp:19,30. So the
rest-check half is fine; the g1Trig reference is the suspect symbol.)

## THE FIX (Claude Code, Rack)
Restore Mode B gate-slaving so gate duration follows external Gate 1:
1. Find what engine.g1Trig became. It's the smoothed/schmitt state of the Gate 1 INPUT. Candidates: the
   tc gate-edge state, or input.gate1 >= 1.f (gate1High is already computed at Monsoon.cpp:588). The
   commented code wanted "is Gate 1 currently high" (smoothed) -> gate OUT high; low -> gate out low.
2. Re-implement the block (modeSelect == 1 only):
   - If the step's decision is NOT Rest: GATE_OUTPUT follows Gate 1 high/low (the external gate's own
     duration IS the note duration). Use the debounced gate1High (input.gate1 >= 1.f) or a smoothed
     version to avoid clicks.
   - If the step IS a Rest: GATE_OUTPUT = 0 regardless of Gate 1.
   - LEGATO still applies: a legato step should tie across gates (suppress the gate-low between steps) --
     confirm legato interaction with slaving (legato = hold through the gate gap; rest = force low).
3. This must run AFTER the mode execution set lastStepResult.decision, and override the GATE_OUTPUT that
   the internal note-length path wrote. Mirror how the commented code sat late in process().
4. Verify in Rack: external gate into G1, Mode B -> note length follows gate width; REST punches holes;
   LEGATO ties across gates; note length/variation knobs have NO effect (correctly nullified).
5. Bisect: `git log -S "g1Trig" -- src/Monsoon.cpp` to see when the block was commented + what g1Trig was
   renamed to -- that gives the exact replacement symbol.

REGRESSION CONFIRMED: this is why "Mode B worked before" -- the slaving block was live, then commented
during a refactor (likely the g1Trig rename), removing gate-follows-external behaviour. Priority: this is
a real ship-affecting regression for anyone driving Monsoon from external gates.

## STILL 1/4 NOTES after CC's fix -- the override is at the WRONG LAYER
CC's fix (87eaaac) overrides GATE_OUTPUT voltage to follow Gate 1. Well-reasoned, but STILL 1/4 notes
because it patches the OUTPUT VOLTAGE, not the internal NOTE-LENGTH STATE. Two reasons it can't work alone:
1. LANTERN READS gs.gateHeld / gs.holdRemain (the INTERNAL note-length state), NOT GATE_OUTPUT
   (Lantern.cpp:42-56). So overriding the output voltage doesn't change what Lantern shows.
2. The internal hold is still 1/4-note everywhere (STEP, poly, CV envelope all read gs, not GATE_OUTPUT).

ROOT of the 1/4: the controller passes noteVal = 2.f (MonsoonModeController.cpp:182). NOTE_VALUES index
2 = 1/4 note = 4 sixteenth-steps of hold (NoteValues.hpp:21). So gs.holdRemain = 4 steps -> every note
spans ~4 external gates -> long notes, rest/legato swamped. The 1/4 is BAKED INTO gs at triggerNote(...,
nvIdx), which Lantern + all outputs read.

## THE ACTUAL FIX: shorten the internal note length in Mode B (state layer), THEN CC's output-follow works
Change the controller's Mode B noteVal from 2.f (1/4 = 4 steps) to 6.f (1/16 = ONE step):
- NoteValues.hpp index 6 = 1/16 = 0.0625 whole = 16*0.0625 = 1 sixteenth-step hold.
- gs.holdRemain then expires within ONE step, before the next external gate -> each gate rise produces a
  FRESH single-step note, no welding across gates. This is what "gate follows external Gate 1" needs at
  the STATE level (not just the output voltage).
- MonsoonModeController.cpp:182: change `2.f,` -> `6.f,` (and update the comment: not "1/4 neutral" but
  "shortest single-step note so the internal hold never spans gates; the external gate is the duration").
- KEEP CC's GATE_OUTPUT-follow block -- with a 1-step internal note it now behaves correctly (rest=low,
  legato/tie=bridge high, single=follow Gate 1). The two fixes are complementary: short internal note
  (so Lantern/STEP/poly/CV see gate-width notes) + output-follow (so the mono GATE jack tracks Gate 1).

VERIFY (Rack): external gate into G1, Mode B. Lantern shows notes at GATE WIDTH (not 1/4). REST punches
holes. LEGATO ties across gates. Note-length/variation knobs have NO effect. Fast gates -> fast short
notes (not one long note).

NOTE if 1/16 still too long for very fast gates: the truly correct version sets gs.holdRemain from the
EXTERNAL GATE WIDTH (measure Gate 1 high-duration) rather than any fixed nvIdx -- but 1/16 single-step is
the simple fix that matches "gate drives the step" (one gate = one step = one 1/16 note by definition of
the step grid). Try 6.f first; only go to gate-width-measured hold if fast-gate tests need it.

## CORRECTION (Rodney: scope shows a gate at GATE_OUTPUT) -- CC's fix DID work for the output
Rodney patched a VCV scope on Monsoon GATE out and SEES a gate. So CC's GATE_OUTPUT override IS taking
effect at the jack. My earlier "wrong layer / does nothing" was too strong -- correction:
- GATE_OUTPUT (audio path) and the Lantern read DIFFERENT state. CC's fix made GATE_OUTPUT follow Gate 1
  (scope confirms). The Lantern reads gs.holdRemain / gs.gateHeld (internal note-length state), which
  CC's fix did NOT touch. So they legitimately DIVERGE now.
- Therefore the remaining "1/4 notes" is (at least largely) a LANTERN DISPLAY mismatch: the audio gate is
  correct, but the Lantern faithfully draws the un-shortened internal hold (holdRemain = 4 steps from
  noteVal=2.f). The Lantern logic isn't wrong -- it's showing a state CC's fix bypassed for the output.

### Which scope picture? (decides severity)
- Scope = SHORT gate following Gate 1, rest makes gaps -> AUDIO IS CORRECT. Remaining issue is Lantern
  (and STEP/poly/CV, which also read gs) showing 1/4. Display/secondary-path mismatch, NOT blocking play.
- Scope = LONG 1/4-width gate not following Gate 1 -> CC's override isn't reaching the jack; real signal
  bug (something re-drives GATE_OUTPUT downstream, or the override condition misses).

### Right fix given the divergence: make internal state AGREE with the output (still noteVal 2.f -> 6.f)
The scope test shows the real issue is TWO DIVERGENT REPRESENTATIONS (output follows Gate 1; internal
hold still 4 steps). Best fix = ONE SOURCE OF TRUTH: shorten the internal note so holdRemain = 1 step
(noteVal 2.f -> 6.f = 1/16, MonsoonModeController.cpp:182). Then GATE_OUTPUT, Lantern, STEP, poly, CV ALL
agree -- gate-width single-step notes everywhere -- and note-length is genuinely nullified at the STATE
level, not just masked at the output. KEEP CC's output-follow (it correctly handles rest=low / legato=tie
/ single=follow at the jack; with a 1-step internal note the two are consistent).
Corrected rationale: not "CC's fix does nothing" (scope proves it works at the jack) but "CC's fix fixes
the OUTPUT while leaving the INTERNAL STATE at 1/4, and the Lantern honestly shows that internal state --
so shorten the internal note to make output and state agree."
