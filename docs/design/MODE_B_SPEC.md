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

## 3. Note LENGTH: nullify internal length, use the gate  [PARTIALLY DECISION NEEDED]
The controller currently passes noteVal=2.f (1/4 = 4 steps) -> gs.holdRemain = 4 steps -> notes span
multiple gates -> the "long notes" bug, AND the internal state (which the Lantern/STEP/poly/CV read)
disagrees with the gate-following output.
The internal note length must be NULLIFIED so every read-path shows gate-width notes.
Option A (simple): controller passes noteVal = 6.f (1/16 = exactly ONE step). holdRemain expires within
  one step, before the next gate. Each gate = one fresh single-step note. All read-paths agree.
Option B (exact): set holdRemain from the MEASURED Gate 1 high-width (gate closes when Gate 1 goes low).
  True "gate width IS note width", handles gates shorter/longer than one step.
[DECISION NEEDED -- RODNEY]: A or B for NON-legato notes?
  - A is trivial and matches "one gate = one 1/16 step" (the step grid definition). Recommended default.
  - B matters only if you want the note to end when Gate 1 goes LOW (staccato short gates) rather than
    lasting the whole step. Do you want gate-WIDTH (B) or gate-triggered-fixed-1-step (A)?
  My read: A unless you specifically want staccato/varying gate widths to shorten notes. Your call.

## 4. LEGATO / TIE in Mode B -- THE REAL DESIGN PROBLEM  [DECISION NEEDED -- RODNEY]
THE CRUX (Rodney): In Mode A, when a note commits at its ONSET to start a legato/tie (gs.slurForward),
the engine KNOWS the note's length -- it will end at a known step boundary (or fractional sub-step via
gateSecRemain), so the gate can ride open to exactly there and the join to the next note is defined.
In Mode B, when a gate rise starts a note we decide to make a legato/tie source, WE DO NOT KNOW ITS
LENGTH -- the length is however long until the next external gate, which hasn't happened yet. So Mode A's
"commit at onset to hold forward for the known length" does not directly map: there is no known length
to hold for.

So the question is: what does a legato/tie MEAN when the note length is externally, unknowably timed?
Candidate models (need Rodney's decision):
  MODEL 1 -- "hold across the gap to the next gate": a legato note keeps its gate HIGH from its own
    Gate 1 fall THROUGH to the next Gate 1 rise (bridging the gap between gates), so consecutive notes
    connect with no gate drop. The tie length = until the next gate. Natural + simple; legato = "no
    re-articulation between these steps". Works without knowing length in advance (you just don't drop
    the gate at Gate 1 fall; you keep it up until the next rise). Likely the right model.
  MODEL 2 -- "sustain until Gate 1 falls, gap otherwise": legato only matters if there IS a gap between
    gates (gate width < step). If gates are contiguous (gate width = full step) there is no gap to
    bridge and legato is a no-op. Ties Model 1 to Option B (section 3).
  MODEL 3 -- "N-gate tie chains": a tie holds across a FIXED number of subsequent gates (like a tie
    length), decided at onset. But we don't know gate timing, so "N gates" is the only length unit we
    have. More complex; probably not wanted.
[DECISION NEEDED -- RODNEY]:
  (a) Which model? (My read: MODEL 1 -- legato = bridge the gate high from this note's end to the next
      gate rise, so connected notes don't re-articulate. It needs no advance length knowledge: at Gate 1
      FALL, if this note committed to slur forward, DON'T drop the gate -- hold high until the next
      Gate 1 rise, then that next step decides its own articulation.)
  (b) Does legato interact with REST? If the next step is a REST, a pending slur should resolve to gate
      LOW at that step (rest wins). Confirm.
  (c) 3-note tie chains: supported in Mode B or not? (Model 1 supports them naturally -- keep holding
      high across multiple gaps as long as each note commits slurForward.)
  (d) Interaction with Option A vs B in section 3: if A (fixed 1-step notes, gate always full step, no
      gap), MODEL 1 legato = "don't drop between these steps" (bridge the inter-step edge). If B
      (gate-width notes with gaps), legato bridges the actual gap. Decide sections 3 and 4 together.

## 5. ALL READ-PATHS MUST AGREE (the bug that shipped)
Whatever sections 3-4 decide, the STATE the Lantern reads (gs.gateHeld / gs.holdRemain / lastNoteType)
MUST match the GATE_OUTPUT the audio path produces. The shipped bug: CC's fix made GATE_OUTPUT follow
Gate 1 but left the internal hold at 1/4, so scope (correct) and Lantern (1/4) disagreed. The fix at
section 3 (nullify internal length at the STATE level, not just the output) makes them agree. KEEP CC's
GATE_OUTPUT-follow block only if it stays CONSISTENT with the internal state after section 3; if section
3 makes the internal state correct, the separate output override may become redundant -- prefer ONE
source of truth (the internal state) that all paths read, over a divergent output override.
[DECISION NEEDED -- RODNEY / Claude Code in Rack]: after section 3's fix, is CC's output-override still
needed, or does the corrected internal state drive GATE_OUTPUT correctly on its own? Prefer removing the
override if the internal state alone is right.

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

## Summary of what needs Rodney's thought
1. Section 3: note length model A (fixed 1-step) vs B (measured gate width) for non-legato notes.
2. Section 4: legato/tie model (1/2/3) -- the real design question, since Mode B doesn't know note length
   at onset. Recommended MODEL 1 (bridge gate high to next gate rise). Plus rest-interaction, tie-chains.
3. Section 5: whether CC's output-override stays or the corrected internal state suffices.
Sections 1, 2, 6 are definite and can proceed. 3-5 need your call first.
