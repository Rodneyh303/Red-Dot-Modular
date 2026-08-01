# Step 3: recomputeEffective* -> counter-window scrub blend (the behavioural pivot)

## Current (to replace)
recomputeEffectiveRhythm/Melody blend two STORED arrays by the latched mix:
  slewed[i] = LockedA[i] + mix*(CandB[i]-LockedA[i])
LockedA/CandB are whole drawn patterns (16 floats each, + poly 15x16). A roll promotes B->A and
draws a fresh B (redrawRhythm). MIX is the live A<->B blend.

## Target (scrub)
The effective pattern is the blend across the 6-draw counter window at the SCRUB position:
  s = scrub position in [0..6], 0 = current draw N, 6 = N-6.  (MIX knob repurposed, continuous.)
  f = floor(s), frac = s - f.
  effective[i] = blend( patternAt(N-f)[i], patternAt(N-f-1)[i], frac )      (adjacent-draw blend)
where patternAt(M)[i] = the slot-i value drawn at counter position M (pure Philox re-derivation).
Detent: applied in the KNOB WIDGET drag only (step 5); the value + scrub math read raw continuous s.

## Core capability needed: draw a pattern at an ARBITRARY counter position
Current draw primitive: base = DrawCtr * DRAW_CHUNK + cursor, atUniform(base) -- uses the LIVE
member counter. Step 3 needs "draw slot i at position M" without disturbing the live counter.
Since Philox is stateless-addressable, add a position-parameterised draw:
  float philoxRhythmAt(int64_t pos, int cursor) { return rhythmPhilox.atUniform(pos*DRAW_CHUNK+cursor); }
Then patternAt(M) re-runs the per-slot draw logic reading philox*At(M, cursor++) instead of the
member-counter form. The draw logic (rest/variation/legato/accent/poly) must be refactored so it can
run against an explicit position -- extract the per-slot draw into a helper taking pos.

## Approach (incremental within step 3)
3a. Add philox*At(pos, cursor) primitives (position-explicit). No behaviour change yet.
3b. Extract the per-slot pattern draw (currently inline in redrawRhythm's loop) into
    drawRhythmPatternAt(int64_t pos, out arrays) that uses philox*At. redrawRhythm becomes: draw at
    the current counter via the helper (behaviour-identical -- verify).
3c. recomputeEffectiveRhythm: replace LockedA/CandB blend with
    blend(drawRhythmPatternAt(N-f), drawRhythmPatternAt(N-f-1), frac). Cache the <=7 window draws
    per recompute if profiling needs (fallback). N = current rhythmDrawCtr.
3d. Same for melody.
3e. Verify in Rack: MIX/scrub knob now sweeps the last-6 window with adjacent-draw blend; a roll
    advances N (window slides); reverse inverts.

## Slew interaction (check)
Currently slew is consumed at roll time to shape B (B = A + slew*(T-A)). Under the scrub model each
patternAt(M) is a FULL independent draw (no A-relative slew shaping). DECISION NEEDED: does slew
survive as a per-draw shaping, or fold away (scrub replaces the A-relative morph slew provided)?
Likely slew's role (bounded random walk near A) is now the SCRUB's job (blend across adjacent
draws), so slew may retire or repurpose. FLAG -- resolve at 3c. For first cut: draw full independent
patterns at each position, mix knob blends adjacent (ignore slew), see how it plays.

## Undo (step 7 later)
Once patterns are re-derived from the counter, a roll's undo = (before_ctr, after_ctr) scalar. The
LockedA/CandB arrays (step 4) are then removable.

## Order
3a primitives -> 3b extract draw-at-pos (verify identical) -> 3c/3d recompute swap -> 3e Rack verify.
Slew decision at 3c. Stored arrays removed in step 4 after 3 proves out.
