# Mode B: stop symptom-patching, do ONE coherent pass (spec + test)

## Why this note (Rodney: "clearly Mode B had some other issues")
The gate-regression debug (MODE_B_GATE_REGRESSION.md) turned into whack-a-mole because Mode B has
SEVERAL separate problems, not one clean regression. Symptom-by-symptom patching fixed one read-path at
a time and left others diverged. Evidence:
1. Gate-slaving block was commented out (g1Trig renamed in a refactor) -- CC restored the OUTPUT side.
2. Internal note-length still 1/4 (controller noteVal=2.f) while output now follows Gate 1 -> OUTPUT and
   INTERNAL STATE diverge.
3. Lantern reads the INTERNAL state (gs.holdRemain), so scope (GATE_OUTPUT, correct 1/16) and Lantern
   (shows 1/4) DISAGREE. Confirmed in Rack: scope right, Lantern wrong.
These are the signature of a mode whose pieces drifted apart across successive refactors (g1Trig rename,
PPQN 24/48/96 rework, note-length changes) with NO end-to-end test catching the drift. Each refactor was
locally fine; Mode B fell through the cracks.

## Do this instead: define the semantics ONCE, conform every path, add a test
### Intended Mode B semantics (the single spec)
- One external Gate 1 RISE = one step (already true: executeModeB advances on gate1Rise).
- The NOTE DURATION is the external gate -- internal Note Length / Variation are NULLIFIED at the STATE
  level (not just masked at the output). So gs.holdRemain must reflect gate width / one step, NOT a
  fixed 1/4 (noteVal 2.f). Fix at source: controller passes a 1-step note (noteVal 6.f = 1/16), OR set
  the hold from measured Gate 1 width. Prefer the single-step approach unless fast-gate tests need width.
- REST punches holes (gate low that step regardless of Gate 1).
- LEGATO ties across gates (bridge the gate high across the Gate 1 gap; gs.lastNoteType Tie/Legato).
- ACCENT applies as normal.
- ALL READ-PATHS DERIVE FROM ONE COHERENT STATE: GATE_OUTPUT, STEP, poly voices, CV envelope, AND the
  Lantern must agree. No path reads a state another path has bypassed. (The current bug is exactly this:
  output bypassed the internal hold; Lantern still reads it.)

### The fix in terms of the code
- MonsoonModeController.cpp:182  noteVal 2.f -> 6.f (1-step internal hold; make note-length genuinely
  inert at the state level so Lantern/STEP/poly/CV all show gate-width notes).
- KEEP CC's GATE_OUTPUT-follow block (87eaaac) -- with a 1-step internal note it's consistent, not a
  divergent override. Re-verify rest/legato/single branches against the single spec above.
- Confirm STEP / poly / CV read the same coherent state (they read gs; with the 1-step note they align).

### THE TEST (this is the real fix -- without it, the next refactor re-breaks Mode B)
Add a standalone end-to-end Mode B test to test/ (engine-level, runs in the container -- no Rack needed):
feed a sequence of external Gate 1 rises at 1/16 and assert:
- one step advance per gate rise;
- note-length is gate-width / one step (holdRemain does NOT span multiple gates) REGARDLESS of the
  note-value param -- i.e. note length is nullified;
- REST at a step -> gate low that step;
- LEGATO -> tie across the gate gap (holdRemain bridges);
- ACCENT flag set when accented;
- the state the LANTERN reads (gs.gateHeld/holdRemain/lastNoteType) matches the state the OUTPUT path
  produces -- assert they cannot diverge (the exact bug that shipped).
This test is the guardrail. It encodes the spec, catches all three drifted issues at once, and stops the
g1Trig/PPQN/note-length class of refactor from silently re-breaking Mode B. Highest-leverage item here.

## Meta-lesson (same as DATAFLOW_DISCIPLINE_PLAN)
This is "state written for one consumer diverges from what another consumer reads" -- the mutable-state-
crossing-boundaries bug class, in the Mode B path. The output was made correct while the internal state
(which the Lantern reads) stayed wrong. The fix is ONE source of truth + a test that asserts the read-
paths agree -- not another per-path patch.

## Supersedes
The symptom trail in MODE_B_GATE_REGRESSION.md (H1-H4 + corrections) is the DEBUG HISTORY -- keep it for
context, but THIS note is the action plan. Do the coherent pass + the test, not more per-symptom fixes.
