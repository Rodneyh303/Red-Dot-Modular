# CC PROMPT — FIX step-advance draw source: use the VARIATION LANE draw, NOT a fresh Philox draw

CONTEXT (Rodney): step-advance was CONCEIVED as a beautiful reuse of the SPARE VARIATION LANE in gate mode.
The comparison uniform `u` for the advance decision MUST be the VARIATION LANE's step-addressed draw
(compared to the resolved Variation/Straits knob, EXACTLY like every other lane compares its draw to its
knob). The current build (feat/sands-distribution-expander, 5647e7c/b734599) instead draws a FRESH
per-rise uniform from a RESERVED Philox chunk — this is WRONG and must be removed. An additional Philox
draw is a HARD NO.

## THE CORRECT MECHANISM (pin this)
- Each step has a VARIATION LANE draw: `r_vary = pe.warpRow(monoSlot, PL_VARIATION)[getVariationStep()]`
  (already read at SequencerEngine.cpp:850/974/1142). This is the step-addressed draw.
- ADVANCE DECISION = compare that step's `r_vary` against the resolved step-advance value (Variation knob
  + CV2 + Junction, and for subgates the resolved ratchet/ghost ADVANCE threshold) — the SAME compare
  pattern as rest (`r_rest < restTh`), legato, etc. StepAdvance::decide(p, u) with u = r_vary.
- HELD STEP IS CORRECT, NOT A WALL: when the step holds, rereading the same step's r_vary gives the same
  comparison — so to advance OFF a held step you MOVE THE VARIATION KNOB/CV to a more extreme setting
  (toward ±1) until it crosses that step's r_vary. The step-addressed draw is the PER-STEP THRESHOLD
  (different steps are naturally stickier/looser); it is the intended live control, NOT a bug to escape
  with randomness. At knob=±1 you cross every step's draw → always advance (today's behaviour). At knob=0
  → always hold. This is exactly why the spare lane's step draw is the right source — the whole point.

## THE FIX — remove the fresh draw, source u from r_vary
1. DELETE the reserved-chunk machinery: PatternEngine.hpp:894 STEP_ADVANCE_CHUNK (1<<32), :895
   stepRateUniform(); SequencerEngine stepAdvanceRiseCtr_ and its ++; the :358-359 `rise =
   stepAdvanceRiseCtr_++; u = pe.stepRateUniform(rise)`.
2. decideStepAdvance_(int subLevel) takes the step's r_vary as u (pass it in, or read the level-appropriate
   variation draw inside): compare u against the resolved advance value p. MAIN level: r_vary at the main
   variation step (getVariationStep()); RATCHET/GHOST levels: the variation draw addressed at THAT subgate
   step (the same warpRow(PL_VARIATION) read, at the subgate step index — mirror how the subgate rest/
   legato reads address their level).
3. SHARED-DRAW NOTE (not a conflict): r_vary is ALSO the ghost-candidacy draw (SequencerEngine:1137-1142,
   `r_vary >= variationAmount → no ghost`). Advance and ghost-candidacy reading the SAME step draw is
   CORRECT (same-step consumers, the consumer-group model) — both are legitimate step-addressed compares
   against their own thresholds. Advance compares r_vary to the (bipolar) step-advance value; ghost
   candidacy compares r_vary to the (rectified) rate. The two-field split from 5647e7c (variationProb
   bipolar for advance vs variationRateProb rectified for candidacy) STAYS — it is the correct decoupling
   of the two THRESHOLDS; only the DRAW SOURCE (u) changes from the fresh chunk to r_vary.

## BIPOLAR / DIRECTION (unchanged by this fix)
The bipolar sourcing (CV2 → getVariationUnclamped, the ±1 range, the forward/hold/backward sign product,
backward wrap 0→15) all STAY. Only the `u` source changes. decide(p, u): p is the resolved bipolar value,
u = r_vary. p≥0 FORWARD iff u<p; p<0 BACKWARD iff u<-p; p=0 (and |p|≤0) HOLD. (At |p|=1 crosses every r_vary.)

## CHECK FOR OTHER INCONSISTENCIES WITH THIS INTERPRETATION (Rodney asked)
Audit the whole step-advance build for anything that assumed the fresh-draw model and is now wrong:
- Any claim/comment that the advance draw must be "fresh per rise" or "not step-addressed" or "would freeze
  with the held step" — those are the INVERTED reasoning; the freeze IS the intended per-step threshold.
  Remove/rewrite those comments.
- The determinism/trajectory TESTS that fed a synthetic per-rise u: re-point them to drive r_vary (the
  Variation lane draw) and the knob — the trajectory is now "given these per-step r_vary values and this
  knob, the playhead walks thus." The p=1 identity (always forward) and p=0 (always hold) still hold.
- The "one-draw-per-decision" / stepAdvanceRiseCtr determinism pins: replaced by "reads the existing
  r_vary, consumes NO new draw" — assert the Philox stream position is UNCHANGED by the advance decision
  (no new consumption), which is the stronger correctness property (the additive-domain law: advance adds
  ZERO draws).
- Golden pins: p=1 still = unconditional forward = byte-identical to pre-feature (should still hold, now
  via u=r_vary with knob=1 crossing every r_vary). Re-capture the mid-range digests if the draw-source
  change moves them (expected — different u source), GOLDEN_A (clock/phase) untouched.

## DONE =
- No reserved Philox chunk, no stepRateUniform, no rise counter — advance consumes ZERO new draws (assert
  stream position unchanged).
- decideStepAdvance_ uses r_vary (main + per-subgate-level variation draw) as u, compared to the resolved
  (bipolar main / ratchet / ghost) advance value. Held step rereads the same r_vary (intended threshold).
- Bipolar/direction/wrap unchanged; the variationProb/variationRateProb two-field split kept.
- Comments/tests that assumed fresh-draw rewritten; p=1 identity holds; mid-range digests re-pinned if
  moved; GOLDEN_A untouched. Full battery green.

════════════════════════════════════════════════════════════════════════════════
## THE COMPANION CHANGE (same piece) — ghost/ratchet CANDIDACY moves OFF Variation onto subgate REST/LEGATO

This is WHAT FREES VARIATION (Rodney). Do it IN THE SAME fix, because it is the reason r_vary becomes spare.

### Current (to change)
Ghost (and ratchet) CANDIDACY — "does this in-gap ghost cell / ratchet sub-cell produce a candidate note" —
is gated on the VARIATION strand: SequencerEngine.cpp:1137-1148 (mono) + :1382 (poly):
`r_vary >= ghostTh → no ghost`, where ghostTh = variationRateProb (the rectified variation rate). So
Variation is doing DOUBLE DUTY (candidacy gate). THEN executeStep separately rolls the REST lane which may
ALSO silence the candidate (two rest-like gates on one ghost).

### The change — candidacy from the SUBGATE GHOST/RATCHET REST (+ LEGATO) decisions
Ghost/ratchet candidacy becomes a function of the per-level SUBGATE REST (and LEGATO) thresholds — the
consumer-group knobs built in task-85 (subRes[GHOST/RATCHET][REST], [LEGATO]) — NOT of r_vary. A ghost/
ratchet cell is a candidate per its subgate REST decision (and LEGATO where it applies), exactly the tier
that now owns per-level articulation. This REMOVES the `r_vary >= variationRateProb` candidacy gate →
r_vary is then GENUINELY SPARE in gate mode → free for step-advance (the fix above). The freeing is CAUSED
by this move, not incidental.

### GET RID OF THE DOUBLE CANDIDACY — one rest decision per cell (Rodney)
There is no separate "candidacy" gate anymore. A ghost/ratchet cell plays IFF its SUBGATE REST decision
passes — that is the whole test. DELETE the old variation-candidacy gate (r_vary >= variationRateProb at
:1137-1148 / :1382) entirely; do NOT replace it with a second rest gate on top of executeStep's rest.
Candidacy and rest were always the SAME question asked twice — collapse to ONE: the cell's existence IS the
subgate rest-or-not at that level. (executeStep's rest roll for that cell becomes the single rest authority
via the subgate REST threshold; there is no additional candidacy silencing.)
- LEGATO's role in candidacy (if any) per the subgate design — a ghost tied from the prior cell vs a fresh
  ghost. Keep consistent with the main-level legato/tie semantics.

### Consequence for variationRateProb
With candidacy off Variation, variationRateProb (the rectified rate that fed ghost candidacy at :1148/1382)
may have NO remaining consumer. If so, REMOVE it (and simplify the 5647e7c two-field split to just the
bipolar variationProb for advance — the rectified field existed ONLY to feed candidacy a non-negative rate).
Verify no other reader before removing. (If a reader remains, keep it; but the candidacy reader is gone.)

### Net after both changes
- Gate-mode Variation strand (r_vary) has ONE consumer: STEP-ADVANCE (the sole reader — cleaner than the
  "shared with candidacy" note earlier in this doc, which is SUPERSEDED: candidacy no longer reads r_vary).
- No double candidacy: a ghost/ratchet cell plays iff its subgate REST passes — ONE rest decision, no separate candidacy gate.
- variationRateProb removed if unused; the advance value is the single bipolar variationProb.
- Note length in gate mode already doesn't use r_vary (fixed 1/16), so nothing else claims it.

## DONE (updated) =
- (advance draw fix as above) + ghost/ratchet candidacy sourced from subgate REST(/LEGATO), not r_vary;
  ONE rest gate per cell (no double-count); variationRateProb removed if it has no remaining reader;
  r_vary's SOLE gate-mode consumer is step-advance. Goldens: candidacy moving changes ghost behaviour →
  re-pin the affected digests deliberately (documented), GOLDEN_A untouched, p=1 advance identity holds.
