# CC PROMPT — fix the step-advance FREEZE: phase the variation read against the others (gate mode)

CONTEXT (Rodney): the current step-advance reads u = v[getVariationStep()] (the variation draw AT THE MAIN
PLAYHEAD'S STEP) and advances iff u < p. BUG (observed in Rack): all playhead motion CEASES after a few
steps unless Variation=1. ROOT CAUSE — CIRCULARITY: the advance decision reads from the position it
controls, so a HELD step re-reads its OWN fixed draw every rise → same answer forever → freeze. Any step
whose draw >= p is a permanent wall; at p=0.5 ~half the steps wall → motion soon ceases. Only p=1 (crosses
every draw) keeps moving. (The earlier "move the knob to escape" framing made the whole mid-range unusable —
rejected.)

## THE FIX — PHASE the variation read against the other lanes (NOT a second root playhead)
In gate mode the VARIATION read must advance on EVERY rise, while the OTHER lanes advance only when the
playhead actually moves. That phase difference = the number of rises since the playhead last advanced. So:
ONE new integer, riseSinceAdvance (gate mode only), and the variation read gets a PHASE-SHIFTED clock:
    u = <variation lane read via unified addressing> at T = totalStepsElapsed + riseSinceAdvance
(the OTHER lanes keep T = totalStepsElapsed). Same unified-addressing lane machinery (laneTickFor /
driverTickAt / the LOR chain) — the variation lane just gets a phase-offset T. NOT a separate playhead; a
phase offset on the existing read.

### The loop (gate mode, per rise)
1. riseSinceAdvance starts 0.
2. Each rise: u = the variation lane's read at (totalStepsElapsed + riseSinceAdvance), DIRECTION = the
   Variation knob SIGN (positive → variation cursor steps FORWARD through variation; negative → BACK),
   WRAP at the PHRASE BOUNDARY (the lane's length wrap in laneTickFor/driverTickAt handles it).
3. decide(p, u) → {BACKWARD, HOLD, FORWARD} for the MAIN playhead (p = resolved bipolar Variation; the
   existing decideStepAdvance_/StepAdvance::decide, unchanged — only u's SOURCE changes).
4. FORWARD/BACKWARD → advancePlayhead(dir) (moves totalStepsElapsed), then riseSinceAdvance = 0 (re-align:
   the next step is read in phase first).
5. HOLD → riseSinceAdvance += 1 (variation phases one further ahead for the next rise → a FRESH variation
   value → no freeze).

### Why no freeze (the proof)
A held step re-reads at an INCREASING phase offset, walking the variation lane through its values. To stay
frozen, EVERY variation value in the lane's window would have to fail < |p| — probability ~(1-|p|)^len,
~zero for any p>0. So motion ALWAYS resumes within ~1/|p| rises. p=0 = HOLD by design (correct). The
mid-range is now usable: escape in ~1/|p| rises (p=0.5 ≈ mostly-moving with the odd stutter; low p = more
stutter-repeats then moves — a sensible "advance probability" mapping).

### Why this integrates with unified addressing (not a bolt-on)
Unified addressing reads every lane as a function of the clock T (read_position = laneLOR(voiceLOR(
globalLOR(T)))). This fix gives the VARIATION lane T = totalStepsElapsed + riseSinceAdvance while others
use T = totalStepsElapsed — it is STILL a unified-addressing read, just phase-shifted. The circularity in
the OLD code was that it read variation at the MAIN step (coupled read to controlled position); phasing
decouples them. totalStepsElapsed stays the single content clock all other lanes read from (NOT redundant —
it is the T the whole chain maps; it now advances per the decision, as step-advance already made it).

## SCOPE
- GATE MODE ONLY. Clock/phase: riseSinceAdvance stays 0, variation reads in phase exactly as today
  (unchanged — those modes keep Variation's note-length/other meaning).
- No new Philox draw (reads the EXISTING variation lane values at a shifted index). Keep the zero-new-draws
  property (assert the Philox stream position is unchanged by the advance decision).
- Delete/replace the current frozen read (u = v[getVariationStep()] in decideStepAdvance_) with the
  phase-shifted read. The three-level hierarchy (main/ratchet/ghost) each phases its own level's variation
  read by riseSinceAdvance (one playhead → one riseSinceAdvance shared; each level reads variation at the
  shifted T for its level).
- reset-on-advance: riseSinceAdvance resets when the MAIN playhead advances (any level that advanced).

## PINS
- p=1 (|p|=1) → crosses every variation value → always advance → BYTE-IDENTICAL to the pre-freeze-fix p=1
  (the identity point). Pin it.
- The freeze fix MOVES mid-range gate-mode behaviour (it no longer walls) → re-pin the affected gate-mode
  digests DELIBERATELY (documented). GOLDEN_A untouched (gate-mode-only guard).
- Zero-new-draws assertion (stream position unchanged by the advance decision).

## DONE =
- decideStepAdvance_ reads u at the PHASE-SHIFTED variation position (totalStepsElapsed + riseSinceAdvance),
  knob-sign direction, phrase wrap, via the unified-addressing lane read. riseSinceAdvance: +1 on HOLD,
  reset on advance; gate-mode only.
- No freeze (motion resumes within ~1/|p| rises at any p>0); mid-range usable; p=1 identity byte-identical;
  zero new draws.
- Re-pinned gate-mode digests (documented); GOLDEN_A untouched. run_all + strict flags green.

## NOTE
This SUPERSEDES the current frozen single-draw mechanism (u = v[getVariationStep()]). The companion fixes on
this branch (candidacy=subgate REST, rest-slur generatedRestBeatsLegato, tie-into-rest option 2) are
separate and stand.
