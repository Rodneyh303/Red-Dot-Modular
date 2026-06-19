# Phase position→state derivation spec (the load-bearing piece for jumps/scrubs)

Status: design note, no code. Pins down the ONE hard abstraction so forward phase,
within-draw reverse, cross-boundary reverse, and jumps/scrubs all fall out of a
single consistent model instead of four ad-hoc ones.

Companion to PHASE_ENGINE_AUDIT.md / SUBSTEP_PULSE_DESIGN.md. Grounded in the
ACTUAL engine decision logic (SequencerEngine::executeStep, MergePanelWork),
not a sketch.

═══════════════════════════════════════════════════════════════════════════════
## 0. The grid (settled)

Phase 0→1 = one bar = 16 sixteenths = 16 * pulsesPer16th pulses.
  24 PPQN → 96 pulse positions   (pulsesPer16th = 6)
  48 PPQN → 192                  (12)
  96 PPQN → 384                  (24)

  pulsePos   = floor(phase * 16 * P16)        // P16 = pulsesPer16th
  step       = pulsePos / P16                 // which 1/16 (0..15), integer
  subPulse   = pulsePos % P16                 // position within the step (0..P16-1)

Because the gate refactor made ALL note lengths integer pulse counts, the phase
grid and the gate grid are the SAME quantization — no continuous-time component to
reconcile. This is the property that makes the rest of this tractable.

═══════════════════════════════════════════════════════════════════════════════
## 1. The two-layer model

LAYER A — within a draw (one phrase, one draw-counter value):
  The pattern is FIXED arrays: rhythmRandom[0..15], legatoRandom[16],
  variationRandom[16], melodyRandom[16], octaveRandom[16], accentRandom[16].
  Traversal is pure: a playhead reads steps in ± direction. NO PRNG involvement.
  "Reverse within a draw" = read the same arrays right-to-left, applying the
  start/span rules in the play direction.

LAYER B — across draw boundaries:
  The draw-counter changes; the counter-PRNG (Squares/Philox) regenerates the
  adjacent draw's arrays deterministically. This is the ONLY place the PRNG runs.
  Reversibility is free: regenerateDraw(N) is a pure fn of (N, key).

The whole engine state under phase is therefore the triple:
    (drawCounter, pulsePos, direction)
A forward tick increments pulsePos; a reverse tick decrements it; crossing
pulsePos < 0 or > (16*P16-1) changes drawCounter and regenerates. A JUMP/SCRUB is
just SETTING that triple to arbitrary values. Everything below is about deriving
the audible state (gate open? pitch? accent?) from that triple.

═══════════════════════════════════════════════════════════════════════════════
## 1b. Modulation within a draw (A/B mix, spread, slew) — the refinement

"Within a draw the arrays are fixed" is true only of the RAW dice. The EFFECTIVE
values feeding executeStep are modulated. Confirmed mechanics (PatternEngine):

  - A/B MIX: effective[i] = A[i] + mix*(B[i]-A[i]), where A=*LockedA[] (committed
    draw) and B=*CandB[] (candidate draw) are BOTH FIXED arrays within a draw. The
    mix coefficient is LIVE, sampled at control rate (latchMix runs every control
    tick — NOT phrase-latched, despite the "Latched" name; it just caches the last
    sampled value and recomputes the blend when it changes).
  - SPREAD: a continuous transform of the variation value (spread = 2*|var-0.5|,
    reach = spread*maxDist), driven by a live knob at control rate.
  - SLEW: already CONSUMED at roll/redraw time to shape B — NOT a live per-pulse
    transform (recomputeEffective uses mix, not slew). Slew matters only at
    draw/redraw boundaries, not within-draw traversal.

Honest model:
    effective[i] = F( fixedArrays[i] , liveCoeffs )
  fixedArrays = {A[i], B[i], rawDraw[i]}  — position-pure, regenerable from the
                                            draw-counter via the §6 block allocator.
  liveCoeffs  = {mix_r, mix_m, spread_lane...} — a SMALL set of live scalars,
                                            applied identically regardless of position.

This split is BETTER than feared: ALL position-dependence lives in the fixed arrays;
modulation is just a few live scalars. Two regimes:

  REGIME 1 — continuous play (forward OR reverse, no jump):
    liveCoeffs stay LIVE — correct and desirable: you hear A/B and spread move as you
    turn them while phase drives position. Each pulse decides with current coeffs. No
    freezing. (User mental-model #1: normal fwd/rev with live modulation is fine.)

  REGIME 2 — jump / scrub (discontinuity):
    No defined coeff trajectory between source and destination ⇒ FREEZE liveCoeffs at
    the jump instant and integrate forward (§3) with that frozen snapshot. The jump
    then reproduces EXACTLY what continuous play would have produced had coeffs been
    constant across the traversed region. (User #3 precompute, #4 freeze, #5 integrate
    forward — all three are this one rule.)

Note-LENGTH under a jump: variation→note value→span. A note that STARTS during the
integration uses the frozen variation coeff. Freeze once at jump start, hold for the
whole integration ⇒ spans self-consistent. A note already SOUNDING when the jump
arrives (user #2: half-note, jump 3-4 steps) keeps its span — it started pre-jump
with pre-jump coeffs; the integration only re-decides notes starting at/after the
jump origin, and the half-note's span (8 steps) simply still covers the destination.

PHRASE-BOUNDARY OPEN QUESTION (deferred — user considering): A and B are committed/
promoted at redraw (phrase) boundaries (redrawRhythm promoteToA), while mix is live.
Going BACKWARD across a boundary, do A/B re-commit to the prior phrase's draw? This
is the modulation analogue of the draw-counter question, NOT yet traced. Resolve
before building cross-boundary reverse (step 3).

═══════════════════════════════════════════════════════════════════════════════
## 2. What decides a note TODAY (ground truth from executeStep)

At a 1/16 edge for step S, with the fixed arrays + carried hold state:

  nvIdx(S)   = getNoteLenIdx(noteVal, input, variationRandom[S])   // note length
  spanPulses = round( noteValueSteps(nvIdx) * P16 )                // integer pulses

  Decision precedence (mono), reading r_rest=rhythmRandom[S],
  r_legato=legatoRandom[S]:
    if (still inside previous note: holdRemain>=1 || gatePulseRemain>0) → MidNote
    else if legatoProb>=0.999                → LegatoMax (forced slide)
    else if r_rest < restProb:
        if canRest (prev ended exactly on edge, no fractional tail) → Rest
        else → MidNote (note tail outranks structural rest)
    else if r_legato < legatoProb            → Legato/Tie (extends a HELD note)
    else                                     → NewNote

  canRest = (holdRemain <= ~0 && !hadTail)

DOCUMENTED fractional-note rules (by design, must be preserved by any derivation):
  R1. A fractional note (1/32=0.5, 1/8T=1.333, 1/4T=2.667 steps) CANNOT be the
      SOURCE of a legato/tie — it closes mid-step, so wasHeld is false at the next
      edge.
  R2. A fractional note CAN be the DESTINATION of a legato/tie (rides the already-
      open gate to its exact sub-step end).
  R3. A fractional note CANNOT be a MIDDLE note of a 3-note tie (would need to be
      both source and destination).

Key observation: a note "owns" a span of pulses [startPulse, startPulse+spanPulses).
Legato/tie EXTENDS that span by adding the next note's pulses (holdRemain += dur).
So a sounding region is a CHAIN of steps linked by legato/tie, starting at some
NewNote/LegatoMax step.

═══════════════════════════════════════════════════════════════════════════════
## 3. The derivation: (drawCounter, pulsePos) → (sounding?, pitch, accent, pulsesRemaining)

This replaces the CARRIED countdown (gatePulseRemain) with a POSITION-DERIVED one.
Forward/within-draw reverse can keep the carried countdown (they move 1 pulse at a
time); JUMPS/SCRUBS need this. Build it once, use it for the jump case.

NORMAL PLAY DOES NOT BACKTRACK (correcting an overstatement in an earlier draft).
In continuous play — forward OR reverse — we are ALREADY at the adjacent pulse one
tick ago, so we just CARRY two integers:
    noteStartPulse   — pulse at which the current sounding note/chain began
    chainEndPulse    — noteStartPulse + Σ spanPulses over the legato/tie chain
Each tick: pulsePos ±= 1; sounding = (pulsePos < chainEndPulse);
pulsesRemaining = chainEndPulse - pulsePos. When a new note starts (or the chain
ends), update the two integers from that step's decision. No walk-back, ever. This
is just the carried countdown expressed as integers — the integer counters the user
asked about (#6). Reverse is the same with the sign flipped and the start/span rules
run leftward.

WALK-BACK IS A JUMP-ONLY FALLBACK. It is needed ONLY when you land at a pulse with no
carried state because you teleported there (a discontinuous jump/scrub). Then, and
only then, you must reconstruct (noteStartPulse, chainEndPulse) at the destination:

DERIVE(drawCounter, pulsePos):   // jump/scrub only
  1. step    = pulsePos / P16 ;  subPulse = pulsePos % P16
  2. Find the OWNING note-start: from the jump ORIGIN (where we DO have carried
     state) integrate FORWARD to the destination (user #5). NOTE this is a full
     REPLAY of the event chain, not span arithmetic: each crossed step boundary can
     START a new note, EXTEND a legato/tie chain, or REST. So the destination state
     is determined by the LAST note-start before the landing — which may be a note
     that began DURING the jumped-over region, not the note sounding at the origin.
     e.g. origin half-note ends at step 4; a tied note starts at step 5 running to
     step 10; a jump landing at step 7 is inside THAT note, not the half-note. Only
     a per-boundary replay catches it. Mechanically: call executeStep at each crossed
     boundary with frozen liveCoeffs — the SAME code path as real-time play, just
     fast-forwarded with no clock wait. No new decision logic. Bounded by jump
     distance (and see §9 one-phrase cap).
       Alternative when there is no usable origin (e.g. load, or a teleport with no
       prior state): walk backwards from `step` to the nearest CLEAN BOUNDARY (a
       Rest, or a NewNote that is not a legato destination — a guaranteed chain
       start), then replay forward to `step`. Bounded by one phrase (≤16 steps)
       within a draw; if the chain straddles the draw boundary, continue into
       regenerateDraw(drawCounter-1) — still bounded, still deterministic.
       Take whichever is shorter: integrate-forward-from-origin or walk-back-to-
       clean-boundary.
  3. Accumulate the chain span from the owning start: sum spanPulses over the
     legato/tie-linked chain until it ends. chainEndPulse = start + Σ spanPulses.
  4. SOUNDING? = (pulsePos < chainEndPulse). pulsesRemaining = chainEndPulse-pulsePos.
     pitch/accent from the chain's active segment (genPitchLive — pure, see §7;
     accent = owning NewNote's accent per the "sticks across MidNote" rule).
  5. Respect R1-R3 when deciding whether a fractional step extends the chain.
  6. Modulation: use the FROZEN liveCoeffs snapshot (§1b Regime 2) throughout.

So the carried gatePulseRemain becomes a DERIVED quantity ONLY at jumps; in normal
play it is the carried integer pair. Same number, two ways to get it — and the
expensive way is reached only on discontinuity.

COST: normal play O(1) (carry integers). Jump: O(min(jumpDistance, chainLength)),
chainLength bounded by one phrase (+1 draw regen if the chain straddles a boundary).
NOT O(jump distance) in the worst case, and often much less. This is the whole point
— jumps are cheap because state is either carried (play) or re-derivable over a
bounded window (jump), never replayed from phrase 0.

═══════════════════════════════════════════════════════════════════════════════
## 4. Slew across boundaries (the one genuinely directional piece)

Within a draw: slew doesn't enter the span derivation (it shapes VALUES, not gate
timing) — ignore for sounding/span; apply to pitch/CV as today.
Across a boundary under reverse/jump: slewed* buffers carry from the PREVIOUS draw
toward the current. Going back, "previous" becomes drawCounter+1.
DECISION (recommended v1): FREEZE slew under scrub/jump — when not advancing one
pulse at a time forward, use the un-slewed (latched target) values. Pure, cheap,
visually/audibly defensible. Revisit with "recompute slew trajectory from draw
start" only if freeze feels wrong in use. (Slew recompute IS possible because both
adjacent draws are regenerable — it's a cost choice, not a feasibility one.)

═══════════════════════════════════════════════════════════════════════════════
## 5. Staging (what needs what)

1. FORWARD PHASE: phase ramp → pulsePos (increasing). Reuse existing execute path;
   carried gatePulseRemain works unchanged. NO PRNG/counter API, NO derivation.
   Validates phase-as-position.
2. WITHIN-DRAW REVERSE: add direction sign; run span/start rules leftward. Carried
   countdown still valid (1 pulse at a time). Still no derivation, no PRNG.
3. CROSS-BOUNDARY REVERSE: wire draw-counter + regenerateDraw(N±1) (the block-
   allocator API). Still 1 pulse at a time, so carried countdown still works;
   derivation only needed if a held note straddles the boundary (use §3 walk-back
   into the adjacent draw for that case).
4. JUMPS / SCRUBS: full §3 DERIVE at the destination triple. This is the only step
   that REQUIRES the position-derived span model end-to-end.

Ship 1–3 on the existing gate model. Take on §3 fully only for step 4.

═══════════════════════════════════════════════════════════════════════════════
## 6. The block-allocator API (needed at step 3, pin it now)

Draw N's lane blocks live at deterministic stream offsets so regenerateDraw(N) is a
pure fn of (N, key):

  constexpr int P16_STEPS   = 16;                  // steps per phrase
  // mono lanes drawn per phrase:
  enum Lane { RHYTHM, LEGATO, VARIATION, MELODY, OCTAVE, ACCENT, LANE_COUNT };
  constexpr uint64_t LANE_BLOCK = 16;              // one value per step
  constexpr uint64_t DRAW_STRIDE = LANE_COUNT * LANE_BLOCK;   // floats per draw (mono)
  // (poly adds 15 voices × {rhythm,melody,octave} × 16 — extend stride; keep mono
  //  lanes at FIXED low offsets so adding poly later doesn't shift them.)

  base(drawCounter, lane) = drawCounter * DRAW_STRIDE + lane * LANE_BLOCK

  regenerateDraw(N):
      for lane in 0..LANE_COUNT-1:
          rng.fillBlock(base(N, lane), targetArray[lane], 16)   // addressable, counter untouched
  advanceDraw(±1): drawCounter += ±1; regenerateDraw(drawCounter)

CRITICAL API SEPARATION: the DRAW-COUNTER (musical phrase index you ++/--) is NOT
the PRNG stream position. The stream position is DERIVED via base(). Keep them
distinct so reverse/jump only ever touches the draw-counter integer.

Reserve the mono lane offsets permanently; append poly after, so a future poly
wiring doesn't renumber existing draws (would break reproducibility of saved seeds).

fillBlock() already exists on Squares/Philox/Philox4x64 (addressable, counter
untouched) — this API is a thin layer over it.

═══════════════════════════════════════════════════════════════════════════════
## 7. Open checks before coding step 4
- Confirm the accent "sticks across MidNote" rule re-derives correctly (accent =
  owning NewNote's accent, not the pulse's own roll).
- Confirm genPitchLive is a pure fn of (melodyRandom[S], octaveRandom[S], input) —
  CONFIRMED (MergePanelWork): genPitchLive(outSem, in, r_semi, r_oct) reads only its
  args, writes only the out-param semitone; no member/latch state. So pitch
  re-derivation at an arbitrary jumped position is exact — no hidden state to
  reconstruct. (Note: the SLEWED pitch path genPitchLive(slewedMelody[i],
  slewedOctave[i]) does depend on slew state — see §4; the RAW path used for span
  derivation is pure.)
- Decide LegatoMax (legatoProb>=0.999, forced) behaviour at a chain head reached by
  walk-back — it always slides, so the chain never has a clean NewNote head while
  legatoProb is maxed; the "clean boundary" then is only a Rest. Bound still ≤16.

═══════════════════════════════════════════════════════════════════════════════
## 8. Forward vs backward, and which PRNG (Squares vs Philox)

### How it works FORWARD (the reference behaviour)
The draw-counter increments at each phrase boundary. Draw N's lane blocks come from
base(N, lane) over the PRNG stream. The playhead reads steps 0→15; legato/tie chains
extend rightward; holds count down. At the boundary, draw-counter++ and the next
phrase's blocks are generated (or, today, a fresh roll). Modulation is live (§1b
Regime 1). This is what the engine does now, reframed onto an explicit draw-counter.

### The BACKWARD analog
Decrement the draw-counter at the boundary; regenerate the PREVIOUS draw's blocks
from base(N-1, lane). Within the phrase, traverse steps 15→0, applying start/span
rules leftward. Because base(N-1,·) is a pure function of (N-1, key), the prior
phrase regenerates IDENTICALLY — no stored history. Eventually decrementing reaches
the initial draw-counter (0 or 1) and the original deterministic settings, exactly
as the user envisioned. The counter IS the phrase history, compressed to one integer.

The asymmetry to remember: backward is NOT time-reversed audio. It is the SAME
generative rules run with the playhead moving leftward — notes still start on
boundaries, dice still map via the counter. A phrase played backward won't be the
mirror of the forward audio, and that's correct (and musically interesting).

### Why a counter-based PRNG is REQUIRED for the backward analog
A conventional sequential PRNG (e.g. Xoroshiro, today's engine) only goes forward:
to get draw N-1 you'd have to store it, because you can't run the generator
backward. A COUNTER-BASED PRNG makes draw N a pure function value(base(N,·), key),
so N-1 is just another evaluation — reverse, jump, and scrub all become "evaluate at
a different counter," needing zero stored history. This is the entire reason we added
Squares/Philox: addressability == reversibility == jumpability, all one property.

### Squares vs Philox — the choice
Both are counter-based, both verified here (KATs + reversibility + uniformity), both
expose the identical addressable API (at()/atUniform()/fillBlock(), get/setCounter).
For THIS use either works; the decision is about pedigree and future-proofing, not
capability.

  SquaresRng
    + Cheapest: one keyed counter → 64-bit output, ~5 rounds of integer ops.
    + Native 64-bit value; fewer ops per draw than Philox.
    + Verified (Widynski KAT, reversibility, chi-sq).
    - Less ubiquitous; not a standard-library or framework default.
    - Single 64-bit stream per (counter,key); we slice it by offset.

  PhiloxRng (4x32)  /  Philox4x64Rng
    + Pedigree: Random123 standard; the algorithm C++26 adds as std::philox_engine;
      the default counter-based RNG in TensorFlow and NumPy (Generator(Philox)).
    + Bit-exact verified against BOTH the canonical Random123 KAT AND the C++26/
      libstdc++ anchors (philox4x32 10000th=1955073260; philox4x64=3409172418970261260).
      ⇒ a draw stream could be reproduced byte-for-byte in NumPy/TF/std for offline
      analysis, tooling, or cross-checking — a real practical asset.
    + 4 words per block (4x32) or 4x64 — natural fit for block-drawing 16-value lanes
      (4 blocks per lane).
    + When the build moves to a C++26 toolchain, our class can be swapped for
      std::philox4x32/4x64 with no behavioural change (we match the standard), or
      kept for the addressable extensions the standard doesn't expose.
    - Slightly heavier than Squares (10 rounds, 128-bit counter space). Negligible at
      our draw volume (~800 floats/regeneration, only at phrase boundaries).

  RECOMMENDATION (matches user lean): use PHILOX. Specifically Philox4x32 unless 64-bit
  draws are wanted (Philox4x64). Rationale: it is the de-facto standard counter PRNG
  (C++26 std, TF, NumPy), it's bit-exact reproducible against those references (so a
  pattern's seed+counter is portable and analysable outside the plugin), and the
  performance cost is irrelevant here because the PRNG runs only at phrase boundaries,
  not per sample. Squares stays in the tree as a lighter alternative and a cross-check
  (two independent verified PRNGs agreeing on reversibility is a nice safety net), but
  Philox is the one to wire into PatternEngine.

  Width: Philox4x32 is sufficient (draws are uniform floats; 24-bit mantissa from a
  32-bit word is ample for probability lanes). Choose Philox4x64 only if you later want
  64-bit-clean doubles or to match a NumPy float64 Philox stream exactly. Either way the
  block-allocator API (§6) is identical; only result_type/uniform precision differ.

### Open question carried forward (user, when time permits)
Phrase-boundary A/B commit under reverse (§1b): A/B promote at redraw boundaries while
mix is live. Decrementing across a boundary — do A/B re-commit to the prior phrase's
draw, and is that draw itself counter-addressable (so it regenerates), or is the A/B
promotion a separate stateful step that needs its own counter discipline? Likely the
cleanest answer is: make A and B BOTH come from the counter stream (A = draw N's
committed block, B = draw N's candidate block at distinct offsets), so the entire
A/B/mix state is regenerable from (drawCounter, key) and reverse "just works". To
confirm against the redrawRhythm/promoteToA logic before step 3.

═══════════════════════════════════════════════════════════════════════════════
## 8b. Why freezing modulation across a jump is FORCED, not chosen (time-scale separation)

A jump is computed entirely within ONE process() call — it consumes ZERO sample-
time. Modulation (mix, spread) has exactly ONE value in scope at that instant: the
current sample's. There is no other modulation value in existence to interpolate
toward, because the jump's support is a single instant on the sample clock.

So freezing the coefficients is not an approximation we impose — it is forced by
the jump being instantaneous in sample-time. This holds REGARDLESS of modulation
rate: even at audio-rate modulation, the jump occupies a single sample instant, so
the coefficient is constant over its support by construction. Control-rate
modulation just makes it extra obvious (the coeff is constant over the whole block).

This is the jump-diffusion / regime-switch structure: the CONTINUOUS part
(modulation) evolves on its own clock, unaffected by the jump; the JUMP part
(playhead position) is an instantaneous mark applied to a frozen background. The two
live on different time scales (sample-instant jump vs ≥sample modulation drift), so
freezing the slow variable across the fast jump is the only consistent
discretization. Trying to interpolate modulation "across" the jump would invent a
path that does not physically exist — the spurious-path error one rejects in a jump
process. (User's PhD domain: regime switches/jumps in stochastic processes — this is
that argument applied here.)

Consequence: a jump is fully computable in-place within process() — freeze coeffs
(already constant), replay executeStep over crossed boundaries reading the fixed
draws, land. No history, no interpolation, no second modulation sample needed. The
time-scale separation is what makes the jump closed-form within one block.

═══════════════════════════════════════════════════════════════════════════════
## 9. Jump-size cap: at most one phrase boundary per jump (simplifying invariant)

A single JUMP event may cross AT MOST ONE phrase boundary (land in the current
phrase or an immediately adjacent one). So the replay integration touches at most
TWO draws (current + one neighbour) and at most ~one phrase of steps. This bounds
the worst-case in-block work and the regeneration to a single neighbour draw.

A continuous SCRUB is then a sequence of small jumps across successive process()
calls, each ≤ one phrase, carrying (noteStartPulse, chainEndPulse, drawCounter)
between calls. A long scrub is therefore fine — bounded per block, amortized — it is
only a SINGLE instantaneous teleport of arbitrary distance that is forbidden (it
would need multi-phrase replay in one block). Draw the line there: no multi-phrase
replay within one process() call.

═══════════════════════════════════════════════════════════════════════════════
## 10. Boundary-crossing under jump/scrub: dice-event state, not position (user constraint)

CRITICAL (user): when a jump/scrub crosses a phrase boundary in EITHER direction,
the rhythm/melody draw used on the far side depends on WHETHER A DICE ROLL HAD BEEN
SELECTED (or live mode activated) for that boundary — not merely on position.

Ground truth (applyPendingSeedsAndRedraw, MergePPQNWork): a phrase boundary REDRAWS
only if, at that boundary, one of these held: rhythm/melodySeedPending,
RollPending, TrialPending, ReseedRollPending, or the lane is in Realtime mode
(mode==1). Otherwise the phrase REUSES the existing draw (A unchanged). Promote
(A walks to B) vs audition (A anchored) further depends on trial / live-trial flags.

Therefore the RNG stream advances ONLY on a redraw-firing boundary. The "draw index"
increments IRREGULARLY — only at boundaries where a roll/reseed/live-mode fired, NOT
at every phrase. Reuse boundaries do not advance it.

Implication for the counter model (this REFINES §6, doesn't break it):
  - The DRAW-COUNTER advances on DICE EVENTS, not per phrase. It is still monotonic
    and well-defined; phrase K's draw = the value of the draw-counter AT phrase K,
    which equals (number of redraw-firing boundaries up to K).
  - So to cross a boundary during a jump/scrub we must know, for that boundary,
    WHETHER it was a redraw boundary:
      * crossing FORWARD over a boundary that fired a roll → drawCounter += 1,
        regenerate draw at the new counter.
      * crossing FORWARD over a REUSE boundary → drawCounter unchanged, same draw.
      * crossing BACKWARD: the inverse — step the counter DOWN only if the boundary
        being un-crossed had fired a roll; otherwise leave it.
  - This requires a per-boundary record of "did a dice event fire here" — a small
    bitmap / list indexed by phrase boundary (NOT the draws themselves; just one bit
    per boundary: redrew or reused, plus the kind: seed/roll/reseed/trial/live).
    This is the ONE piece of genuine history the counter alone cannot reconstruct,
    because it reflects user dice actions / live-mode toggles over time, which are
    not a function of position or seed.

Design options for that per-boundary record:
  A. EVENT LOG (bounded): record, per crossed boundary, {redrew?, kind, counterAfter}.
     Forward play appends; reverse reads it to know how to step the counter back.
     Bounded by the scrub range you allow (with the §9 one-phrase cap, you only ever
     need the current and adjacent boundary's record in a given block).
  B. SNAPSHOT-AT-BOUNDARY: at each boundary store (drawCounter, dice-mode-flags).
     Reverse restores from the snapshot. Heavier; equivalent information.
  RECOMMENDATION: A, minimal — one entry per boundary: did it redraw, what kind, and
  the resulting drawCounter. With the §9 cap, cross-boundary logic in any block only
  consults the immediately adjacent boundary's entry, so the live working set is O(1)
  even though the log grows with phrases played. For a pure deterministic patch (no
  live dice action — fixed seed, no rolls), the log is trivial (every boundary either
  always-reuse or always-reseed per mode) and the counter↔phrase map is regular, so
  reverse needs no log at all — the log only earns its keep when the user has been
  rolling dice / toggling live mode mid-performance.

  Note this PRESERVES the "reach original settings at counter 0" property: decrement
  past all the recorded dice events and you arrive back at the initial draw — the log
  tells you exactly how many counter steps that is.

Open: LIVE mode (mode==1) redraws EVERY boundary (continuous fresh entropy). Under
reverse that means every boundary was a "roll" — the counter decrements each
boundary, and regenerating requires the entropy that was used. If live mode used
seedRngFull (internal 64-bit entropy, not counter-addressable), those draws are NOT
reproducible by counter alone — they'd need the actual entropy logged, or live mode
is declared NON-REVERSIBLE past its activation point (freeze/anchor on reverse).
This is the one mode where reverse may be fundamentally limited; flag for the user's
phrase-boundary deliberation. Counter-addressable reverse works cleanly for
seed/roll/reseed modes that draw from the counter stream; live-full-entropy mode is
the exception.
