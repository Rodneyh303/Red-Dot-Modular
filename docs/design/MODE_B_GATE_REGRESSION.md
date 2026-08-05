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
