# CC PROMPT — make gate-mode step-advance TESTABLE: freeze-fix (local variation scan) + bipolar Variation

Build BOTH on fix/subgate-merge-onto-straits-base. These two together make step-advance usable + testable
(currently it freezes after a few steps unless p=1, and Variation isn't bipolar so backward is untestable).
Rack-test forward/stutter/backward BEFORE this branch merges.

## PART 1 — FREEZE FIX: local variation-window scan (NOT a global-clock offset)
BUG (Rack): all playhead motion ceases after a few steps unless Variation=1. CAUSE: decideStepAdvance_ reads
u = the variation draw AT THE MAIN STEP and advances iff u<p; a HELD step re-reads its OWN fixed draw every
rise → same answer forever → freeze (any step with draw≥p is a permanent wall; ~half the steps at p=0.5).

FIX — scan the variation WINDOW while held (Rodney's framing; corrected from an earlier global-clock-offset
draft that was a code smell — a gate count has no meaning in global pattern-position space):
- Keep an integer scanOfs, GATE MODE ONLY. Starts 0; reset to 0 whenever the MAIN playhead advances; +1 on
  each HELD rise.
- The advance draw u is read from the variation lane SCANNED FORWARD by scanOfs WITHIN THE VARIATION LANE'S
  OWN WINDOW: u = variationValue at (variationReadPos + scanOfs) wrapped by the VARIATION LANE'S LENGTH
  (varLen), NOT by the global clock / NOT by DNA_LCM. scanOfs is a LOCAL scan index into the variation
  window [0..varLen-1] — "how far I've scanned looking for an escape value" — bounded by varLen. It is NOT
  added into totalStepsElapsed / the global pattern clock (that mix was the smell: a held-rise count is not
  a pattern position). Do NOT touch totalStepsElapsed or DNA_LCM.
- WHY no freeze: scanOfs walks the variation window; to stay frozen EVERY value in the window must fail <|p|
  (prob ~(1-|p|)^varLen ≈ 0 for p>0). Escapes within ~1/|p| held rises. p=0 = HOLD by design.
- First visit to a step (scanOfs=0) reads the step's OWN variation value (step identity preserved); the scan
  only accrues as an ESCAPE while held. Reset on advance = each arrived step judged on its own value first.
- This is DNA_LCM-AGNOSTIC by construction (local window scan) — survives the parked DNA_LCM removal
  (CC_PROMPT_KILL_DNA_LCM_UNBOUNDED_CLOCK_PARKED.md). Do NOT implement it as an add into the global clock.

## PART 2 — BIPOLAR Variation in gate mode (so backward is testable)
Currently gate-mode Variation isn't reaching a bipolar value end-to-end (the mono CV2/Junction path has the
unclamped sum but poly/knob isn't bipolar; backward can't be tested). Make the gate-mode step-advance value
BIPOLAR, range [-1,+1]:
- Source: the knob + CV2 + Junction unclamped sum (the existing getVariationUnclamped / the "one unclamped
  sum" mono path, line ~158) clamped to [-1,+1] = variationProb (bipolar, for step-advance). Keep the
  separate variationRateProb = clamp(sum,0,1) if any rectified-rate reader remains (candidacy moved off
  variation, so verify — likely removable).
- decide(p,u): p = the bipolar variationProb, u = the scanned variation value (Part 1). p≥0 → FORWARD iff
  u<p; p<0 → BACKWARD iff u<-p; |p| small → HOLD. advancePlayhead(dir): forward=dir, backward=-dir (wraps).
- Do NOT remap the knob's 0-point in a way that breaks mode A (the knob default 0.5 is shared) — bring the
  bipolar value via the CV2/unclamped path, as already started; make sure the KNOB alone can reach negative
  in gate mode (per Rodney: "variation knob positive step forward, negative step back") — confirm the knob
  maps to the bipolar range in gate mode, not just CV2.

## PINS
- |p|=1 → crosses every variation value → always advance → BYTE-IDENTICAL to the pre-freeze p=1 identity.
  Pin it (forward at +1; the backward-at-−1 is new, pin its own digest).
- The freeze fix + bipolar MOVE mid-range gate-mode behaviour → re-pin affected gate-mode digests
  (documented). GOLDEN_A untouched (gate-mode-only guard). Zero new Philox draws (scan reads EXISTING
  variation values; assert stream position unchanged).

## DONE =
- scanOfs local variation-window scan (gate mode): +1 on held rise, reset on advance, bounded by varLen,
  NOT added into the global clock/DNA_LCM. No freeze (motion resumes within ~1/|p| rises); mid-range usable.
- Gate-mode Variation bipolar [-1,+1] via knob+CV2+Junction; knob alone reaches negative in gate mode;
  backward advance works (wraps). variationRateProb removed if no reader remains.
- p=±1 identity pins; mid re-pinned; GOLDEN_A untouched; zero new draws asserted. run_all + strict green.
- RACK-TESTABLE: forward (p>0), stutter (p~0), backward (p<0), no freeze at any |p|>0.

## AFTER THIS
Rack-test the full wandering, THEN merge fix/subgate-merge → trunk. (Unified-addressing rebases after.)

════════════════════════════════════════════════════════════════════════════════
## BIPOLAR SCOPE — two different cases (Rodney)
The bipolar requirement differs by knob, because of dual-use vs single-use:

### Existing VARIATION lane (main, Monsoon/lane expander) — bipolar ONLY in GATE MODE
Dual-use: note-length VARIATION in clock/phase, STEP-ADVANCE in gate mode. So bipolar ONLY in gate mode;
in clock/phase it keeps its existing UNIPOLAR note-length meaning (negative is meaningless there). This is
the mode-conditional case — the knob's value reinterprets to [-1,+1] in gate mode, stays unipolar
note-length in clock/phase. Mind the shared 0.5 knob default across modes (don't break mode A/clock/phase).

### Subgate ADV knobs (RATCHET ADV / GHOST ADV on Straits — GLOBAL and PER-VOICE) — ALWAYS bipolar
Single-use: these ONLY ever mean step-advance (ratchet/ghost advance). No clock/phase meaning to protect.
So they are ALWAYS BIPOLAR, centered at 0, UNCONDITIONALLY — no mode gating. Simpler than the Variation
lane: they're purpose-built bipolar advance knobs from the start (both the global subgate ADV knobs and the
per-voice subgate ADV panel knobs). Center = 0 = hold; + = forward advance prob; - = backward.

### Net
- Variation lane: mode-conditional bipolar (gate-mode only), clock/phase unchanged.
- ALL subgate ADV knobs (global + per-voice): always bipolar, center 0, no mode check.
Both feed the same decide(p,u) step-advance mechanism; only WHEN they're bipolar differs.
