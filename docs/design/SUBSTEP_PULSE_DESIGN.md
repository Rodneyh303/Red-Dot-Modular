# Replace gateSecRemain with PPQN sub-pulse counting — design

Goal: delete the seconds-based sub-step gate timer (gateSecRemain) and close ALL
note lengths — including 1/32 and triplets — by counting integer sub-16th pulses.
Removes the tempo-freeze artifact and the float-drift race, unifies the gate model
on one mechanism (counting), and is phase-friendly.

## Resolution math — CONFIRMED SUFFICIENT ✓
24 PPQN = 24 pulses/quarter; a 1/16 step = 6 pulses. Every note value lands on an
integer pulse count:
  1/1 =96  1/2 =48  1/4 =24  1/4T =16  1/8 =12  1/8T =8  1/16 =6  1/32 =3
Smallest unit = 3 pulses (1/32). No fractional remainder anywhere → the seconds
timer is genuinely unnecessary. (If a 1/16T or 1/32T were ever added: 1/16T = 4
pulses ✓, 1/32T = 2.667 ✗ — NOT integer at 24 PPQN. So 24 covers everything in the
current table with headroom, but a future 1/32-triplet would need 48 or 96 PPQN.
Worth noting before anyone adds one.)

## THE PREREQUISITE (the real work): there is no sub-16th tick to count yet
Today the engine only ever sees sixteenthEdge. The 24 sub-pulses are NOT surfaced:
- External clock: ClockEngine counts incoming pulses in ppqnCount but only emits
  sixteenthEdge when ppqnCount==0 (every 6th). The other 5 are swallowed.
- Internal clock: generates ONLY sixteenthSec-spaced edges — there are no
  sub-pulses at all; timeAcc jumps straight to 16th boundaries.
So "count pulses between 16th steps" first requires PRODUCING a pulse to count, and
the source differs:

### External clock — easy
Surface the raw pulse: add `bool pulseEdge` to ClockEngine, set true on every
accepted clkTrig pulse (before the ppqnCount==0 gate). The gate model counts
pulseEdge. 6 pulseEdges per 16th. NOTE: relies on the incoming clock actually being
24 PPQN — if a user patches a 1 PPQN or 4 PPQN clock (allowedPPQN supports 1/4/24),
there are only 1 or 4 pulses per quarter = 0.25 or 1 per 16th → CANNOT resolve
1/32 (needs 3 per 16th). So sub-step lengths are only honoured at 24 PPQN; at lower
PPQN they must fall back (round 1/32→1/16, collapse triplets) — the NoteValues
allowedPPQN bitmask ALREADY encodes which values are legal per PPQN (1/32 is mask
4 = PPQN24 only), so the fallback rule already exists in the table. Good — the
design is consistent with existing PPQN gating.

### Internal clock — needs a sub-divider
The internal accumulator must tick at the PULSE rate, not the 16th rate. Change:
  pulseSec = sixteenthSec / 6      (24 PPQN → 6 pulses per 16th)
  accumulate timeAcc against pulseSec; emit pulseEdge each crossing; every 6th
  pulseEdge also emits sixteenthEdge.
This makes internal + external symmetric: both produce pulseEdge, and sixteenthEdge
is derived as every-6th-pulse. Bonus: this naturally fixes the BPM-modulation
artifact too, because pulse spacing tracks the LIVE sixteenthSec each crossing
(same self-correcting property the 16th counter already has).

## The gate-model change (the easy part once pulses exist)
GateState currently mixes two timers. Replace with ONE pulse counter:
- holdRemainPulses (int) = noteSteps * 6, set at trigger (e.g. 1/32 → 0.5*6 = 3).
- tick() becomes tickPulse(): called per pulseEdge, holdRemainPulses -= 1; close
  gate at 0.
- DELETE gateSecRemain, armGate's seconds path, curStepSec-for-gating. (curStepSec
  may still be wanted for LED flash timing — check semiPlayRemain, which is also
  step-based and could move to pulses too for consistency.)
- The whole-step edge mechanism and the sub-step mechanism become THE SAME thing
  (counting pulses) — no more special-casing fractional tails, no float drift, no
  race with the edge.

Note this multiplies the counter granularity ×6 but it's just an int decrement per
pulse; negligible cost. Poly voices (voices[i].gs) get the same treatment.

## Migration / regression watch
- Note-spanning "secret sauce" (legato/tie extend holdRemain += dur): becomes
  += dur*6 in pulses. Same logic, finer unit. Verify the multi-step phrase feel is
  bit-identical at whole-step values (it must be: 6N pulses = N steps exactly).
- The existing 347-test suite + GateState tests: update expected hold values to
  pulses (×6), or add a steps↔pulses helper so tests read in steps. Recommend a
  constant PULSES_PER_16TH = 6 and express everything through it.
- gs.tick() is called from executeModeA/B/C on sixteenthEdge today; it must now be
  called on pulseEdge (more often). Audit every gs.tick / voices[i].gs.tick call
  site (5 found) — they move from the 16th path to the pulse path.
- prevGate1High / gatePulse (1ms retrigger) are sample-based, unaffected.

## Effort
Moderate, well-contained. The gate-model swap is small and clean. The real work is
producing pulseEdge symmetrically (internal sub-divider + external pulse surfacing)
and re-pointing the 5 tick() call sites + tests. Low conceptual risk (the math is
exact), main risk is test churn and making sure internal/external pulse production
is truly symmetric.

## Phase reassessment after this change
This change makes BOTH phase options EASIER and is arguably a prerequisite:
- It removes gateSecRemain — which was Obstacle 2 in the Option B audit (sub-step
  timing as wall-clock seconds). Gone. Sub-step gates become pulse-counted =
  position-derivable.
- It unifies the gate model on COUNTING, which is exactly what a phase engine wants
  (phase position → pulse index → gate state). A phase ramp maps to a pulse index
  (phase * pulsesPerCycle); the gate model already counts pulses. Much cleaner
  splice than counting 16ths + a seconds side-timer.
- Option A (forward phase): now a near-pure "phase → pulse index → existing pulse
  counter" — the seconds special-case that complicated armGate under warp is gone,
  so the warping-ramp gate-breathing refinement (BPM section of the phase audit) is
  AUTOMATIC: pulse spacing tracks live rate, so gates breathe for free.
- Option B (scrub): Obstacle 2 eliminated; Obstacles 1 (accumulative hold),
  3 (slew state), 4 (phrase reseed), 5 (poly) remain. But hold becoming a clean
  pulse count makes the positional-span reconstruction (Obstacle 1) more tractable
  — "does note-start S's span (Sstart_pulse .. Sstart_pulse + dur*6) cover pulse P"
  is a clean integer interval test.

RECOMMENDATION: do this gateSecRemain→pulse-counting refactor FIRST (it's good
hygiene regardless of phase, fixes the BPM artifact, and removes a model you never
liked), THEN Option A phase becomes a small addition on top of a counting-based
gate model. Reassess Option B only after living with A.

════════════════════════════════════════════════════════════════════════════════
# REFINED DESIGN (after reading the engine's holdRemain reads) + 48/96 PPQN

## Critical finding: holdRemain is DECISION state, not just a gate timer
The engine reads holdRemain in STEP units to make musical decisions:
  - holdRemain >= 1.f        → MidNote (still inside a note this step)
  - holdRemain <= 0.0001f    → canRest
  - 0.0001 < prevHold < 0.999 → hadTail (a sub-step note ended mid-step) ← the very
    reason sub-step timing exists
So holdRemain's FRACTIONAL value carries meaning. Changing its units to pulses
would break MidNote/canRest/hadTail across SequencerEngine + poly. DO NOT re-unit
holdRemain.

## Therefore: keep holdRemain (steps, fractional) — replace ONLY the gate CLOSE
Minimal, low-risk change:
  - holdRemain, tick() decision logic, MidNote/canRest/hadTail: UNCHANGED (step
    units, bit-identical decisions, secret-sauce intact).
  - DELETE gateSecRemain (seconds) + curStepSec-for-gating + armGate's seconds path.
  - ADD gatePulseRemain (int pulses) governing the gate VOLTAGE close. Set at
    trigger = round(durSteps * pulsesPer16th). Decrement per PULSE edge. Gate
    closes when it hits 0. Whole AND fractional lengths both close on a pulse
    boundary — unifies the two old mechanisms, no seconds, no float-drift race.
  - pulsesPer16th = ppqn/4 (24→6, 48→12, 96→24). All note values are integer
    pulses at all three (verified incl. future 1/16T = 4/8/16 pulses).

## Prerequisite: produce a pulse edge to count (unchanged from above)
  - ClockEngine: add bool pulseEdge. External: set on every accepted clk pulse.
    Internal: sub-divide (accumulate against sixteenthSec/pulsesPer16th), emit
    pulseEdge, derive sixteenthEdge as every-pulsesPer16th-th pulse.
  - GateState gains processPulse() (or tickPulse) called per pulseEdge to
    decrement gatePulseRemain + close gate. process(sampleTime) keeps only the
    1ms gatePulse retrigger (no more seconds countdown).
  - The 16th-edge tick() (decision/holdRemain decrement) stays on sixteenthEdge.
    So two cadences: decisions on 16th edges (unchanged), gate-close on pulse edges
    (new). They're consistent because 16th edge == every Nth pulse edge.

## 48 / 96 PPQN
ppqn becomes a user setting (context menu: 24 / 48 / 96). pulsesPer16th derives
from it. Higher PPQN = finer gate-close granularity (and future-proofs triplet
subdivisions). NoteValues.allowedPPQN already gates which note values are legal per
PPQN — extend the masks so 1/32 etc. are allowed at 48/96 too (currently mask bit
"4" = PPQN24; add 48/96 bits or widen the semantics). The integer-pulse property
holds at all three.

## Counter-PRNG (squares) for reverse — SEPARATE, LATER (not this branch)
Rodney's idea: replace stored precomputed draw arrays with a counter-based PRNG
(squares: value = squares(key, stepIndex)). Stateless + reversible: step N's draw
is recomputable forwards/backwards within one key. Hitting dice = new key = new
sequence, so exact-reverse holds for the LIFETIME OF A DRAW (not across re-rolls) —
which is plenty for scrub-within-current-pattern (the recommended Option B v1
scope, and it dissolves Obstacle 4's phrase-history-storage problem: store the key,
not the array). This is a PatternEngine draw-model change, larger and separate from
the gate refactor. Note it as the enabling step for Option B reverse; do it on its
own branch after the gate work + Option A.
