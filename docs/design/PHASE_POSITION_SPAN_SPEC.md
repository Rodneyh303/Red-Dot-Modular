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

DERIVE(drawCounter, pulsePos):
  1. step    = pulsePos / P16 ;  subPulse = pulsePos % P16
  2. Find the OWNING note-start: walk backwards from `step` to the most recent step
     OS where a note actually STARTED (decision ∈ {NewNote, LegatoMax, or the head
     of a legato/tie chain}). "Started" means: not MidNote, not Rest. Because the
     arrays are deterministic, re-evaluate each step's decision from OS..step by
     replaying the precedence rules forward from the last known clean boundary.
       - Bound: at most one phrase (16 steps) within a draw. If the chain extends
         back across the draw boundary (a note/tie held over from draw N-1), walk
         into regenerateDraw(drawCounter-1) and continue — still bounded, still
         deterministic (this is the cross-boundary held-note case).
  3. Accumulate the chain span: starting at OS, sum spanPulses over the chain of
     legato/tie-linked steps until the chain ends (a step that neither extends nor
     is MidNote). Call the chain end pulse CE = OS_startPulse + Σ spanPulses.
  4. SOUNDING? = (pulsePos < CE).  pulsesRemaining = CE - pulsePos.
     pitch/accent = the chain's current segment values (genPitchLive at the active
     step; accent from the owning NewNote per existing "accent sticks across
     MidNote" rule — result.accented = lastStepResult.accented).
  5. Respect R1–R3 when deciding whether a fractional step extends the chain.

So the carried gatePulseRemain becomes a DERIVED quantity: pulsesRemaining from
step 4. Forward play: it decrements naturally (pulsePos++). Jump: recompute via
DERIVE at the destination. Same number, two ways to get it.

COST: O(chain length), bounded by one phrase (≤16 steps) within a draw, plus one
extra draw regen if the chain straddles the boundary. NOT O(jump distance). This is
the whole point — jumps are cheap because spans are re-derivable, not replayed.

WALK-BACK is the one non-O(1) part: to know the chain head you re-evaluate decisions
from a clean boundary. A "clean boundary" = any step that is a Rest or a NewNote
that itself isn't a legato destination — i.e. a guaranteed chain start. In practice
rests/new-notes are frequent, so the walk-back is short; worst case is a fully-tied
phrase (16 steps). Acceptable.

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
