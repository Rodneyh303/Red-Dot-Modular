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
