# DESIGN — unify the TWO position systems into one robust authority (DRY/ODR)

STATUS: IMPLEMENTED (2026-11-16). The resolver unification shipped — slices 1-5 + step 3b + step
4-final all merged to `lane-expander-refactor`. `pos_` (PositionResolver) is the single position
authority: `stepIndex` is retired (now `pos_.playedColumn()`, field deleted in step 4-final) and
`globalWalk_` is retired (now `pos_.phaseClock(transportTick_)`, a view, deleted in slice 5). The
underlying global counter `totalStepsElapsed` is also dead (POSITION_HOLDER_MODEL.md — per-holder
phases). See the MIGRATION PLAN section for the per-step status. The sections below are kept as the
design record + the still-OPEN cadence/direction threads.

ADDENDUM (2026-11-15, OPEN): see "INPUT CADENCE / ACCEPTANCE MODEL" below — the RATE at
which position's inputs (global range, LOR, direction, modulation) are recognised/accepted,
and how that composes with the existing effect-timing ladder. C1 (rungs), C2 (keep-counting),
C4 (display cadence) and C3 (per-level direction) all DECIDED 2026-11-15. Design-ahead: C1/C2/C4
land as their OWN slice. C3 was GATED on the position-holder migration — that migration is now DONE
(POSITION_HOLDER_MODEL.md, the counter is killed and `test_position_holders` is green), so C3's gate
is LIFTED; the per-level-direction BEHAVIOUR is now buildable (not yet built). These are the OPEN
work left on this doc; the resolver unification itself is shipped (see top STATUS).
See also DRIVE DIRECTION MODEL below (2026-11-15) — gate direction is GLOBAL, not voice-1/mono-
sourced; event modes default forward with a REVERSE-ALL control; HOLD per-voice; and the
VARIATION-centre fix (knob 0.5 = repeat-in-place, no forward creep).

## DECISIONS (Rodney, locked)
1. SCOPE: ALL modes. GLOBAL direction: event modes (CLOCK + GATE) are default-forward,
   flipped by the REVERSE-ALL control — IDENTICAL for both (DRIVE DIRECTION MODEL); PHASE
   takes its direction from the input. So the reverse GLOBAL traversal (first-played ==
   endStep) is reachable in ALL drive modes. The MULTIPLICATIVE per-level direction
   composition (voice/lane Reverse/Pendulum/PingPong) is active in ALL modes. The resolver
   takes the global direction from the mode (+ reverse-all) and composes the per-level
   LaneDirs always.
   UPDATED 2026-11-16 (Rodney — SUPERSEDES the original "CLOCK global is forward-only
   (signMotion >= 0)" nuance): that held when direction was an inferred behaviour and
   reversing a clock seemed odd; now direction is an EXPLICIT control (reverse-all, default
   OFF → clock is still forward by default), so clock gets global-reverse PARITY with gate —
   capability parity, mechanism matched to the drive (switch for event modes, input for
   phase). Rationale: times changed since that was written; and in mono (1-channel) gate
   mode a global reverse is already reachable via Variation anyway, so this just makes the
   capability explicit and uniform. "One more switch" — cheap, since per-voice/lane
   directions already diverge under the composition.
2. The resolver OWNS the window traversal: the start/end bounds, the windowMask
   membership (isStepInWindow) and the seeding/skip. (windowMask is the global transport
   window — NOT the output mask; confirmed, no entanglement.)
3. STATELESS derivation: one transport-position state in the resolver; playedColumn,
   phrasePos and the composed direction are PURE views of it; deriveHolders re-derives
   from those (no second accumulator). (POSITION_HOLDER_MODEL style.)
4. INCREMENTAL slices, goldens bit-identical at each.

## THE PROBLEM (why this exists)
Two independent computations of "where are we in the phrase" that must agree but DRIFT:

1. **stepIndex** — the PLAYED/displayed ring column. Advanced in advancePlayhead by a
   window traversal with its OWN seeding + skip logic (SequencerEngine.cpp:471-492):
   reverse seeds `stepIndex = endStep+1` then steps onto endStep; forward seeds
   `startStep-1`; `isStepInWindow` scans skip out-of-window columns. Read by the OUTPUT
   and the UI and the sync: Intertropical (the result trigger), Monsoon (phaseStepFor
   sync :1354, lockManager :1814, the PHRASE tier :1815), Lantern, and every Sands
   visual (as the ">=0 started" guard and the playhead feed).

2. **globalWalk_** — the PHRASE-POSITION accumulator, `≡ (t−1) mod cachedLength`,
   advanced in tickHolders (:835). Read ONLY by deriveHolders to derive every holder
   position (the voice frame reads, the lane reads, the strip read).

They are the SAME quantity computed two ways. In REVERSE they desync by a direction-
and-history-dependent ±1 (stepIndex's seeding/skip vs globalWalk_'s pure accumulation):
from reset the reverse phrase entry (stepIndex==endStep) sits at globalWalk_==lenG-2;
after a forward run then reverse it differs again. deriveHolders also runs BEFORE
stepIndex is updated for the step, so the derive cannot even see the played column.

## THE SYMPTOM THAT EXPOSED IT (the acceptance bug)
The voice rotation must sync at the FIRST-PLAYED step of the phrase — forward: startStep
(lower bound); reverse: endStep (the O+L upper bound). It anchors to globalWalk_==0
(the forward entry) only, so in reverse the rotation lands mid-phrase. This is NOT
fixable robustly by any globalWalk_ formula, because the reverse entry has no stable
globalWalk_ value (proved: an anchor of lenG-2 passes from reset but fails after a
direction change, off by one the other way). Reproduce-first oracle already written:
test/test_reverse_rotation_anchor.cpp — gates FORWARD, REVERSE, and FORWARD→REVERSE
(the last keyed off stepIndex==endStep, which is the direction-stable truth).

## THE MULTIPLICATIVE DIRECTION RULE (must be preserved)
Direction is PER-LEVEL and composes as a SIGN PRODUCT, not a single global flag:
- Levels: global transport (signMotion), voice frame (voiceLorDir_ / macroVoiceLorDir_),
  lane (laneDir_ / laneDirV_ / macroLaneDir_). Each is a LaneDir
  {Forward, Reverse, Pendulum, PingPong}.
- laneTickFor(LaneDir, t, len) applies the per-level transform (Forward t0, Reverse
  −(t0+1), Pendulum/PingPong bounce). The effective step is the PRODUCT:
  e.g. `dstep = signMotion * mdSign` (:1049), `motionV` composing the voice sign, the
  lane composing again. A probability lane can run Reverse under a Forward voice under a
  Reverse global — all three compose. The unified authority MUST compute position by
  composing these, from ONE source, so no two consumers disagree.

## THE PROPOSAL — one position authority
A single resolver — **PositionResolver** (the settled name: descriptive, not thematic —
it is an internal engine class, not a user-facing module) — that owns the transport
position and is the ONE source every consumer reads:

- It computes, once per advance, from the transport motion + the window (start/end,
  len, offset) + the per-level LaneDir composition:
  - `playedColumn` — the ring column (replaces stepIndex; drives output/UI/sync).
  - `phrasePos` — steps since the CURRENT-DIRECTION entry (0 at the first-played step,
    forward or reverse), the coordinate deriveHolders folds (replaces the globalWalk_
    role). Because phrasePos is defined by the SAME traversal that produces
    playedColumn, "first-played step" is ONE definition — the rotation anchor becomes
    `phrasePos == 0` in both directions and the reverse bug cannot recur.
  - the composed per-level direction/sign the holders consume.
- stepIndex and globalWalk_ become thin VIEWS of the resolver (or are deleted once all
  consumers move over). The window-skip (isStepInWindow) and seeding live in the
  resolver ONCE, not split between the stepIndex traversal and the globalWalk_ accumulate.
- deriveHolders is fed the resolver's phrasePos + composed direction, so it no longer
  needs globalWalk_ and no longer runs "blind" to the played column.

## INPUT CADENCE / ACCEPTANCE MODEL — the resolver as the single owner of position-input acceptance

STATUS: addendum (2026-11-15). C1 (rungs) + C2 (keep-counting) + C4 (display cadence) + C3
(per-level direction) DECIDED by Rodney 2026-11-15 (recorded below; revisitable). C1 REFINED
(2026-11-15 eve): nested LOR latches at the ADDRESSED-AGAINST FRAME, uniform across voice AND
probability lanes — cycle → parent cycle (lanes at the voice boundary), abs → global phrase. C3 is
per-level but GATED on the position-holder migration (POSITION_HOLDER_MODEL.md) — it rides the
counter removal, not the cadence slice. Design-ahead; still NOT slice 1 (slice 1 keeps today's
cadence to stay golden-identical).

C1 CADENCE FORK — RESOLVED (Rodney, 2026-11-17). During Sands display feel-testing the
voice-frame-edge cadence was briefly questioned (should the probability lane re-latch its LOR
mod on the voice-frame edge, as C1 says, or on the lane's OWN cycle?). RESOLVED: **voice-frame
edge, as C1 already specifies — and not as a compromise but as the feature.** The voice-frame
edge makes the lane LENGTH a polymeter control: the short lane runs free against the voice
meter and the voice edge is the downbeat that re-anchors it. 8-step voice / 3-step lane → the
lane subdivides the frame as `3 3 2`; lane→4 → `4 4`; lane→2 → `2 2 2 2`. And the subdivisions
ARE genres: `3 3 2` is the tresillo grouping behind DnB/jungle breaks (garage, dembow, Afro
rhythms too); `4 4` is straight four-on-the-floor house/techno — SAME line, same notes, only the
lane length differs. So length mod on the lane is a live FEEL-MORPH macro: a slow LFO/envelope on
the length glides the groove from syncopated breakbeat to driving four-on-floor without touching
the pattern. The "drift" IS the instrument. Re-latching on the
lane's own cycle would make the lane merely restate itself every N — the cross-rhythm is gone —
so "lane's own cycle" is the musically dead option. NOTE for whoever reads the feel-test logs:
the "range jumping at step 4/5, drifting across the lane" that read as a bug 2026-11-16 WAS this
polymeter (the short lane against the voice frame); it only looked like jitter because two real
faults masked it — the double-commit (lane latching twice per voice-frame cycle, fixed in
`c8ac2cf2`) and the dishonest display (frozen-range / disconnected-grab, fixed by the FINAL
window model in CC_PROMPT_LANE_RANGE_DISPLAY_UNIFY.md). Cadence was correct throughout; do not
"fix" it.

### Why this belongs in the resolver
THE PROPOSAL computes position "once per advance, from the transport motion + the window +
the per-level LaneDir composition" — but it ASSUMES those inputs are already the right
values at advance time. They are not a given: the global range, the per-level LOR, and the
per-level direction each change on their OWN acceptance edge, and today those edges are
INCONSISTENT. A resolver that recomputes from the LIVE controls reintroduces exactly the
jitter/drift EFFECT_TIMING_HIERARCHY.md + RATE_DISCIPLINE_UNIFICATION.md name. So the
resolver must consume a LATCHED snapshot of its inputs, taken at the step-advance edge —
never the live controls.

This keeps decision #3 (stateless derivation) intact: the resolver stays a PURE function of
(tick, latched global range, latched LOR, promoted direction). The LATCHING is the stateful
layer OUTSIDE the resolver — the existing pending→promote machinery, GENERALIZED from
direction to every position input. Per RATE_DISCIPLINE: TIMING picks each input's rung;
DATAFLOW makes the hand-off single-writer-at-the-edge so engine and playhead PULL the same
working copy and cannot disagree.

### What the code does today (verified, not narrated)
- DIRECTION — already disciplined, and the MODEL to copy. advancePlayhead holds
  laneDirPending_ / laneSignVPending_ and promotes the DIR enum to laneDir_ at the step edge
  (LaneFlipQuant::StepEdge, ~SequencerEngine.cpp:431) or on `wrapped` only
  (LaneFlipQuant::Phrase, ~:508), with a LOCK_SCOPE layer gating whether a strand may
  promote. The SIGN is DERIVED from position each step — only the enum promotes — so it can
  never half-apply.
- LOR (len/off/rot) — EFFECT_TIMING_HIERARCHY classifies it at the 1/16-step rung (it feeds
  the pattern READ). NOTE: that doc reads as principle-stated, implementation-for-when-built.
  CONFIRM whether the step-boundary latch is actually in place at the store→engine read, or
  whether the Sands playhead jitter it describes is still latent.
- GLOBAL RANGE (cachedLength, window bounds) — the UNDISCIPLINED one. updateWindow
  (Monsoon.cpp:1870) runs EVERY process() block (the control-divider was deleted, :1733),
  and setWindow (:270) writes cachedLength IMMEDIATELY. So the phrase length + window bounds
  change mid-step at CONTROL rate — yet globalWalk_ ≡ (t−1) mod cachedLength and the
  traversal bounds both feed the pattern read. The input most structural to position is the
  LEAST quantized. A latent rate-mismatch the resolver should close.
- The TimingController class governs TRANSPORT GATES (run/reset/gate1-2/edges) — NOT
  parameter acceptance. There is NO single owner of "when does a change take effect" today;
  this section proposes the resolver (or a sibling latch it reads) become it.

### The acceptance ladder, mapped to position's inputs
Per EFFECT_TIMING_HIERARCHY's rungs (control / 1-16-step / phrase / unlock):
- nested LOR len/off/rot (voice AND probability lanes) → the ADDRESSED-AGAINST FRAME boundary
  (C1 + its refinement): CYCLE mode → the PARENT level's cycle (lanes at the VOICE boundary,
  voice at the GLOBAL phrase); ABSOLUTE mode → the GLOBAL phrase. TODAY step — this COARSENS it
  to the relationship's OWN frame, deliberately (not to the global phrase, which was too coarse).
- global len + off → STEP, together (C1 — the MeloDicer-native "reposition/resize the whole
  pattern now" class). TODAY control-rate — this is a (small) coarsening/fix.
- per-level direction → each level at its OWN edge (C3). TODAY laneFlipQuant is a SINGLE enum
  (one edge for the whole engine, lock-scope picks which strands) — per-level EDGES are the
  extension, coherent only on the holder model (see C3). The resolver reads each level's
  PROMOTED dir, never the pending one.
- modulation (CV) → NOT a rung of its own; it RIDES the target's rung (CV→LOR latches at the
  LOR rung, CV→direction at the direction rung). Nothing new to decide for modulation once
  the target rungs are set.

### DECISIONS (Rodney, 2026-11-15 — recorded; revisitable)

THE ORGANIZING PRINCIPLE (replaces "consistency"): RUNG = WHAT THE CONTROL IS FOR, not
uniformity across inputs.
  - A control whose value is a PERCEPTIBLE RELATIONSHIP over time (nested LOR — each level
    phasing against the frame it is ADDRESSED AGAINST: the lanes against the VOICE, the voice
    against the GLOBAL) → that FRAME's boundary (see the C1 refinement for the per-level/per-mode
    mapping). The relationship is only HEARD if it holds still for its own frame; per-step it is
    hash, not phasing, and in UAS it makes everything jump. LOR is the reason the instrument
    exists (the observation that small probability-offset changes between lanes make a random
    pattern sound better), so its legibility is primary.
  - A control whose value is an IMMEDIATE REPOSITION / RESIZE of the whole pattern (global
    length + offset) → STEP. No inter-element relationship to hold still for: offset slides the
    whole window against the clock (a translation — it preserves every relationship), length
    just moves the loop end. This is the MeloDicer-native class.

C1 (DECIDED). Sands LOR (len/off/rot) → PHRASE. Global length + offset → STEP, TOGETHER.
    "Together" because MeloDicer has global length AND offset in real time but has NO
    per-voice/lane LOR — direct evidence that global L+O are one class (real-time) and LOR is a
    class MeloDicer never had. The MeloDicer premise "patterns react instantly to the controls"
    is honoured for the class it was coined for (global L+O); it does not oblige LOR, which was
    never in that class. LOCK MODE stays ORTHOGONAL — the deliberate defer-to-cue (DJ cueing)
    that overrides every base rung wholesale; it SUSPENDS a rung, it doesn't set one.

C1 REFINEMENT (Rodney, 2026-11-15 eve) — THE RUNG IS THE ADDRESSED-AGAINST FRAME, uniform across
    EVERY nested LOR level (voice LOR and probability-lane LOR alike). "LOR → the GLOBAL phrase"
    was too restrictive: the probability lanes do not phase against the global phrase — they
    phase against the VOICE (lane rotation = where the lanes land when the VOICE hits its first
    step). Their relationship's frame is the voice CYCLE, not the global phrase; latching them
    at the global phrase lets them re-phase only once per global phrase even when the voice
    cycle — their real period — is far shorter. So, applied to every nested LOR level uniformly:
      - a level's LOR change promotes at the boundary of the FRAME IT IS ADDRESSED AGAINST;
      - CYCLE mode → the PARENT level's cycle boundary (its phrasePos==0): probability lanes at
        the VOICE-cycle (LO) boundary; the voice at the GLOBAL phrase (its parent is the global);
      - ABSOLUTE mode → the GLOBAL phrase (abs addresses the level against global step 1, so the
        global is its frame);
      - global length + offset stay at STEP — the drive-level reposition, not a nested LOR.
    So the addressing MODE is the frame, and the acceptance cadence follows it.
    EXAMPLE (the parent frame can be SHORTER than the level's OWN length): a voice lane of
    length 16 under a GLOBAL pattern of 8 accepts its LOR change after step 8 — the global
    phrase, its parent frame — NOT after its own 16. This is intended: the voice re-anchors to
    global step 1 every global phrase (every 8), so the change lands exactly at that re-anchor,
    mid-way through the voice's own 16-step traversal. The acceptance rate is the RELATIONSHIP's
    (voice-against-global = the global phrase), not the level's internal length.
    WHY better, not just looser: (1) SELF-CALIBRATING — the latch holds exactly one relationship
    period (one parent cycle); a short cycle is a short relationship, so one full instance still
    plays before it can change. (2) LANDS AT THE NATURAL RE-ENTRY — the parent boundary is where
    the level re-anchors anyway, so a pending LOR promotes at the level's OWN frame boundary,
    cleaner against C2 (no re-seed mid-cycle). (3) ONLY BITES UNDER ACTIVE MODULATION — the rung
    quantises WHEN a change lands, not how often you change it; hold the knob and the phasing
    holds indefinitely.
    IMPLEMENTATION: a natural extension of the direction pending→promote machinery (laneFlipQuant-
    style) — key the promote boundary PER LEVEL by addressing mode: parent's phrasePos==0 (cycle)
    or global phrasePos==0 (abs). The engine already knows every level's cycle boundary and mode.

    THE BOUNDARY IS EVALUATED LIVE (not a precomputed period). The promote check is "is my parent
    at phrasePos==0 THIS step?", tested each step against the parent's CURRENT length — never a
    fixed period or a countdown. Consequences, which fall out for free:
      - the parent's length is itself a STEP-rate control (C1), so the acceptance boundary MOVES
        when the parent length changes mid-run: global 8→4 at step 8 → the voice LOR accepts at
        12 AND 16 (two global-4 phrases), not at 16 only;
      - a parent length change MID-parent-phrase routes through C2 (keep-counting) to decide where
        the next parent phrasePos==0 — and thus the acceptance — lands (the "shrink below the
        current pos" case).
    One rule covers every combination (parent shrinks, grows, longer/shorter child, once or
    repeatedly); a CC implementation that precomputes a period WILL get the step-12 case wrong.

    ROTATION DOES THE ALIGNING (Rodney). In cycle mode rotation re-anchors the voice to the global
    START (forward) / END (reverse) at EVERY global re-entry — that IS the cycle semantic, applied
    each global phrase whether or not R is being changed. So the LOR acceptance boundary COINCIDES
    with the rotation re-anchor: a ROTATION change lands exactly where rotation already re-aligns
    the voice, at the next global step 1. The voice-16/global-8 picture is therefore really about
    a ROTATION change — the voice re-aligns to the global start each global phrase regardless; a
    change to L or O re-parameterises the voice's OWN window rather than this global alignment.
    (Same one level down: a probability lane's rotation re-aligns it to the VOICE's first step
    each voice cycle, and its acceptance boundary coincides with that re-anchor.)

C2 (DECIDED — and SELECTED BY C1, not independent). phrasePos KEEPS COUNTING across a
    mid-phrase global change; it does NOT re-seed. Why it is forced by C1: LOR is phrase-
    latched and anchored to global step 1 (= phrasePos==0). RE-SEED would re-fire phrasePos==0
    on EVERY global length/offset tweak → the lanes you phased RE-ANCHOR and snap mid-flow,
    destroying the exact phase-stability C1 bought. KEEP-COUNTING leaves this phrase's anchor
    alone: the global change bites the WRAP immediately (MeloDicer-instant), but the lane
    anchoring set at this phrase's start holds until the next REAL phrase boundary. It is also
    the MeloDicer feel — turning length plays THROUGH with a moved loop-end, it does not
    restart. col = (phrasePos + rot) mod len + off, evaluated against the step-latched len/off
    and the free-running phrasePos.
    COST (accepted; small + deterministic): shrinking length BELOW the current phrasePos folds
    `phrasePos mod newLen` → lands a step or two into the new short window rather than at its
    start, until the next wrap tidies it. Re-seed's only virtue (always-clean-from-anchor) is
    precisely the anchor-re-fire we do NOT want, so the cost is worth paying.

C3 (DECIDED — PER-LEVEL; GATED on the position-holder migration). Each level's direction
    promotes at its OWN edge (voice StepEdge, lane Phrase, …); there is NO single common latch.
    The "half-promoted product" worry is an ARTIFACT of the global-counter model
    (dstep = signMotion × mdSign — a STORED product that can be half-updated). POSITION_HOLDER_
    MODEL.md removes it: each (voice,lane) holder OWNS its position and updates LOCALLY
    pos = wrap(pos ± dir), dir applied FRESH each advance — no shared composed sign exists to
    half-promote, so every holder is always self-consistent with its own promoted dir.
    COMPOSITION survives as NESTING, not multiplication: a child walk's phase driver IS its
    parent's walk, so a parent flip drags its whole SUBTREE immediately (the driver reversed),
    while each level's OWN flip lands at its OWN edge.
    WORKED: voice (StepEdge) flips mid-phrase → lane's DRIVER reverses NOW → lane nets backward
    (parent-back × own-forward) until the phrase → lane's own dir promotes at the phrase → nets
    forward again (back × back). Coherent at EVERY step; it is the correct meaning of per-level
    — a parent reversal is immediate and applies to everything under it; each level's own
    reversal is its own choice at its own edge.
    CONDITION (why GATED): this coherence DEPENDS on the holder model. On the CURRENT global
    counter, per-level edges force the stored product and the half-promoted state is REAL —
    there you would need the common latch as a stopgap. So C3 RIDES the counter-removal
    migration (POSITION_HOLDER_MODEL.md sequencing: poly advance + divergence tests → counter
    removal → 32 steps), NOT the C1/C2/C4 cadence slice. Gated on that migration, not on S2.
    VERIFY (don't assume — the holder model is partly TARGET, not fully built): when a PARENT
    flips mid-phrase the child must re-anchor to the LIVE parent wrap, not a stored stale one
    (consistent with C2 keep-counting). Reproduce-first, a DIRECTION-divergence case of the
    holder doc's per-voice divergence suite: voice reverses at a step edge while lane holds →
    assert the lane reads the parent-dragged positions, then re-nets at the phrase. This is what
    proves per-level composition is wired to the LIVE reference and not a cached product
    creeping back in.

C4 (DECIDED). DISPLAY cadence follows the INPUT rung — the Sands range IS the LOR mod display.
    There are NO knobs on the LOR, hence NO mod arcs (mod arcs belong on knobs, not on
    modulators); LOR modulation is shown ONLY by the Sands lane range MOVING. So phrase-
    latching LOR in the engine (C1) OBLIGES the Sands range to update at the PHRASE boundary
    too — it must NOT keep sliding every step while the engine holds still, or the display and
    the engine disagree (the exact two-representations-drift this whole doc exists to kill).
    The display PULLS the SAME latched LOR the engine reads (one authority), so it moves when
    the engine's value moves — at the phrase. No mod-arc work; the range movement IS the
    modulation display, and it quantizes with the rung.

### WHAT REACTS WHEN — from the voice lane's view (summary of C1/C2/C3; detection ≠ propagation)
Pulls C1/C2/C3 into one place. KEY DISTINCTION (Rodney, 2026-11-15): "immediate" = PROPAGATION
latency (once the change is KNOWN, how fast the nested subtree follows). It is NOT the same as
DETECTION latency (how soon the engine can KNOW the change happened). Phase-mode direction has
a detection latency the event modes do not.

| change (source)                     | voice reacts        | why |
|-------------------------------------|---------------------|-----|
| GLOBAL length / offset              | next STEP           | C1: global L+O are STEP-rate (MeloDicer-native). "Instant" in a step sequencer == the next step edge; nothing finer. |
| ...its effect on voice ANCHORING    | next PHRASE         | C2 keep-counting: the step-change bites the WRAP at the step, but the voice does NOT re-seed — its LOR anchoring holds to the next real phrase boundary. (Two-tier: window moves now, anchor holds.) |
| voice's OWN nested LOR (L/O/R)      | next PHRASE         | C1 refinement: a level's LOR latches at its ADDRESSED-AGAINST frame; the voice's parent is the global, so its frame is the global phrase. |
| GLOBAL direction — reverse-all (CLOCK/GATE) | immediate   | Discrete control; KNOWN at the toggle. Nesting drags the voice's driver on that same event (C3). No detection latency. |
| GLOBAL direction — PHASE input      | next QUANTISED STEP, THEN immediate | DETECTION latency: phase direction is the sign of `absStep − prevPhaseAbsStep_` across two consecutive PPQN snaps (snapToPhaseStep ~:1299; `d==0` holds the last dir; first snap after a mode switch holds the prior traversal's dir — "one possibly-off snap, corrected at the next"). So a phase reversal is UNKNOWABLE until the next quantised crossing shows `d<0`; ONCE known, nesting drags the subtree immediately. Net: up to one quantised step to SEE it, then immediate. |
| voice's OWN direction flip          | voice StepEdge      | C3: each level's own reversal lands at its own edge; the voice's edge is the StepEdge. |

The two "global direction" rows are why "immediate" needs the qualifier: under the DRIVE
DIRECTION MODEL the event modes (clock/gate) are default-forward + reverse-all (a discrete
control, zero detection latency), whereas PHASE reads direction from the input and can only
INFER a flip one quantised crossing late. Propagation is immediate in both; detection is not.

> FOOTNOTE — the phase-mode detection lag is a FLOOR, not a cadence choice. It sits BELOW the
> whole C1–C4 ladder: the rungs decide when to ACCEPT a change that is already known; this lag
> is about when a direction change becomes KNOWABLE at all. Direction is the derivative of
> position, and you cannot differentiate a single sample — so within the PPQN grid the only lever
> is PPQN resolution (finer quantisation = shorter floor), never the acceptance rungs.
>
> IDEA TO EXPLORE (Rodney, 2026-11-16) — DECOUPLE direction sensing from the PPQN grid. The floor
> only exists because direction is read AT the PPQN crossings. Keep note DECISIONS quantised to
> PPQN (the musical grid is unchanged), but sense the direction SIGN at SAMPLE rate — `sign(Δphase)`
> per sample — so a reversal is caught within a sample, not a whole quantised step. This collapses
> the detection lag without touching note timing. CAVEAT: audio-rate phase can jitter, so a raw
> per-sample sign would flicker — needs a small hysteresis / delta threshold (or an N-sample
> confirm) rather than a bare sign test. Weigh against the current bounded-and-self-correcting
> behaviour: the sample-level check is worth it only if the one-step boundary blip feel-tests as
> audibly wrong.
>
> PRIOR ART (ZZC Phaseque, zezic/ZZC, GPL — technique noted, no code copied): Phaseque senses
> direction at SAMPLE rate in `processTransport()`, as `sign(phaseShifted − lastPhaseShifted)`,
> guarded by: (1) delta != 0 → hold last direction (== our `d==0` hold); (2) `|delta| < 0.95` →
> REJECT wrap-sized deltas (a 0.99→0.01 phrase wrap would false-read as reverse — this is our
> "wrap is a beat, not a bounce" law applied to direction); (3) skip on reset/jump. Notably NO
> small-signal hysteresis — raw sign each sample, tolerating a one-sample flicker at a genuine
> turnaround. Note TRIGGERS are computed separately (the near-0/near-1 crossing), so direction and
> note-decisions are decoupled — the same split proposed here. Takeaway: the sample-rate approach
> is shipping prior art; the jitter guard that matters is the WRAP-magnitude reject (which our
> sign law already encodes), not a turnaround hysteresis — add the latter only if our phase source
> proves noisier than a clean CV.
>
> CONCLUSION (Rodney, 2026-11-16) — the detection-lag floor DISSOLVES in every mode once direction
> is read from the drive's own nature rather than inferred from step history: PHASE via sample-rate
> `sign(Δphase)` (instant bar the one ambiguous turnaround sample), CLOCK/GATE via the reverse-all
> SWITCH (nothing to detect — known the instant it flips). Detection is then instant everywhere;
> the change TAKES EFFECT at the next event/crossing (no position moves between events) — the
> musical grid, not a lag. The switch and the sample-rate sign are the same principle: direction
> comes from the drive, never from a counter.
> It is
> BOUNDED and SELF-CORRECTING: at most one quantised step reads the stale direction (a genuine
> reversal, or the first snap after a mode switch), corrected at the very next crossing; `d==0`
> holds the last direction rather than guessing. It is not a desync that accumulates. The only
> live risk is a single transient wrong-direction step at a reversal / mode-switch boundary —
> a Rack feel-test item (does that one step read audibly wrong?), not a correctness bug.

### Slice placement
NOT slice 1 (pure mirror, keeps control-rate cadence, golden-identical). Lands as its OWN
slice AFTER S2 (derive migration + reverse fix): step-latching the global range is a GENUINE
behaviour change — golden-safe only where a test HOLDS the window constant (GOLDEN_A/B/BS,
UA_NONDIV likely do; the mid-phrase MOVE is exactly what wants a NEW pin). Reproduce-first: a
test that moves len/off mid-phrase and asserts the playhead + holders accept the change on
the CHOSEN edge (not control-rate), per C1/C2. Engine-side is headless-testable; the Sands
range quantization (C4) ships in the SAME slice but is WIDGET — Rodney's Rack leg verifies the
range steps at the phrase boundary, not every step, and never disagrees with the engine.
The engine↔display agreement is pinned structurally (both read the one latched LOR) the way
test_variation_scan_display pins the VAR chevron.

C3 (per-level direction) is NOT in this slice. It is coherent only on the position-holder
model, so it rides the COUNTER-REMOVAL migration (POSITION_HOLDER_MODEL.md: poly advance +
divergence tests → counter removal → 32 steps), with its own direction-divergence test (parent
flips mid-phrase, child tracks the live parent wrap). Gated on that migration, not on S2.

## DRIVE DIRECTION MODEL — direction comes from the drive, not from a voice (Rodney, 2026-11-15)

STATUS: DECIDED 2026-11-15. Companion to the cadence addendum. Amends the mode-direction rule
in CC_PROMPT_POSITION_RESOLVER.md (the "signMotion" line).

### The bug being removed (verified in code)
In gate mode the GLOBAL advancePlayhead direction is taken from decideStepAdvance_(mono()) —
the MONO slot's signed variationProb (SequencerEngine.cpp ~:1818) — and the per-voice motions
layer on top via foldPerVoiceAdvance_. So mono's Variation does DOUBLE DUTY: voice-1's own
motion AND the whole transport's direction. "Voice 1 steers the ship" was never decided — it is
the single-decision-era incumbent (before per-voice advance there was ONE decision driving ONE
timeline; per-voice motion was layered on later, the global kept using mono's). The only thing
actually decided at mode level was "gate/phase bidirectional" (CC_PROMPT_POSITION_RESOLVER:29).
This is the mono-is-special leak — same root as POSITION_HOLDER_MODEL.md.

### The principle — direction comes from the DRIVE's own nature
An EVENT drive (clock tick, gate rise) carries NO direction — a tick says "advance", not which
way. A POSITION drive (phase/position input) carries direction — it can run backward on its
own. Therefore:
- CLOCK (event) → a GLOBAL direction, default forward.
- GATE (event) → a GLOBAL direction, default forward. Borrowing a voice's sign to fake one is
  the arbitrary part; removed.
- PHASE (position) → bidirectional, direction taken from the phase signal itself.
This is why gate ≠ phase is NOT arbitrary: phase HAS a direction to read; gate does not, so a
gate global sourced from voice 1 was the one true inconsistency of the three.

### Forward is itself a convention — so it is globally flippable (the "reverse all" gesture)
Once mono is de-specialized, "forward" is an arbitrary default for BOTH event modes. So the
event-mode global direction is DEFAULT forward, flipped by a single dedicated REVERSE-ALL
control that applies to CLOCK and GATE identically — a GLOBAL control, not sourced from any
voice. PHASE has no reverse-all (reversing the phase input IS the gesture). This recovers the
one-gesture "reverse everything" properly, instead of smuggling it through voice 1's Variation.

### The three tiers (one owner each)
1. GLOBAL DRIVE DIRECTION — event modes: default-forward, flipped by reverse-all (clock + gate
   uniform); phase: from the input. Mono plays no part.
2. PER-(VOICE,LANE) DIRECTION — LaneDir / voiceLorDir_ (deterministic) AND Variation
   (stochastic, bipolar — below), each per holder; composes on top of tier 1 per the
   MULTIPLICATIVE/nesting rule (C3).
No tier is sourced from another's incidental state.

### HOLD is per-voice too — the (2) decision
The global frame advances on EVERY event rise (forward, or flipped by reverse-all); HOLD is
PURELY per-voice — a voice freezes ITSELF (falls behind the moving frame); the frame never
freezes because one voice (mono) chose to. This fully de-specializes mono: its Variation drives
ONLY its own voice, like every other voice. (The smaller alternative — frame holds when mono's
p≈0 — was rejected: it keeps mono special for hold.)

### VARIATION — keep it coupled to direction; fix the CENTRE (Rodney)
Variation is NOT decoupled from direction — it stays a per-voice stochastic advance+direction
control (tier 2). Only the CENTRE semantics change:
- knob 0.5 → the variation lane HOLDS: repeats the current step. NO advance, NO scan, NO
  direction. TODAY p=0 is lumped with forward (decideStepAdvance_: `signedDelta = (p<0) ?
  -delta : +delta`), and since |p|=0 the freeze-fix escape never comes, scanOfsV_ grows and the
  lane/chevron CREEPS FORWARD indefinitely — the arbitrary forward at the centre.
- knob >0.5 → FORWARD; advance probability = (knob−0.5) scaled; scan forward.
- knob <0.5 → BACKWARD; advance probability = (0.5−knob) scaled; scan backward.
- It STILL drives voice direction (the side of 0.5 is that voice's advance direction) — intent
  unchanged; only the centre is fixed.
FIX: make the sign a THREE-WAY — p>0 forward, p<0 backward, p==0 repeat-in-place with NO scan
(scanOfsV_ stays 0, no chevron creep). This also makes the restored VAR scan chevron
(fix/variation-scan-display) sit STILL at the centre instead of creeping — the two are
consistent — and it is the correct meaning of the "by-design stutter" (knob 0.5 → p=0,
MonsoonModeController.cpp:160).

### Slice / where it lands
The DRIVE layer, separate from the C1/C2/C4 cadence slice and from C3. The VARIATION-CENTRE fix
is a small, self-contained engine change (three-way p sign at the scan site + scanOfsV_ gated
on |p|>0), reproduce-first: at p=0 assert NO advance AND scanOfsV_==0 over many held rises; at
p>0 forward scan, p<0 backward scan (the existing freeze-fix behaviour, now excluding the
centre). REVERSE-ALL + per-voice HOLD are bigger (gate-mode advance plumbing + a new global
control) and ride the counter-removal migration alongside C3 (POSITION_HOLDER_MODEL.md). The
mono de-specialization (gate direction global, not mono-sourced) is noted there.

## MIGRATION PLAN (incremental; goldens green at EVERY step — this is the whole risk)
This touches transport timing; the goldens are the contract. Do it in landable slices,
each bit-identical on the existing (forward / offset-0 / ≤16) configs.
STATUS (2026-11-16): steps 1-5 DONE (the resolver unification shipped); step 6 (the cadence
behaviour slice) is the OPEN work.

1. **[DONE — slice 1]** Introduce the resolver as a PURE MIRROR. Compute playedColumn + phrasePos
   inside it, assert they reproduce the CURRENT stepIndex and globalWalk_ bit-identically for a
   broad config sweep (the `test_position_resolver_identity` harness: forward, reverse, every
   LaneDir, the non-divisive lengths, offsets). GOLDEN-IDENTICAL by construction.
2. **[DONE — slice 2 + 2b]** Migrate the DERIVE to read phrasePos from the resolver instead of
   globalWalk_; the reverse/phase entry is now correct and stable. `test_reverse_rotation_anchor` +
   `test_phase_rotation_anchor` green.
3. **[DONE — slice 3 + step 3b]** Migrate the consumers of stepIndex (output, sync, UI, Lantern) to
   playedColumn — slice 3 did the headless managers, step 3b the widget/visual readers via the
   public `playedColumn()`/`started()` accessors.
4. **[DONE — slice 4 + step 4-final]** Slice 4 unified the advance (stepIndex a derived mirror of
   `pos_`); step 4-final DELETED the `stepIndex`/`lastStepIndex` fields and the vestigial mode-switch
   position cache. ALSO (slice 5): `globalWalk_` retired as `pos_.phaseClock(transportTick_)`, a
   resolver view, field deleted — see the globalWalk_ resolution section (option (a) shipped).
5. **[DONE — slice 4]** The rotation-anchor fix is `phrasePos == 0`, direction-agnostic; the fragile
   globalWalk_-offset attempts are gone.
6. **[OPEN — the cadence slice] (after S2, gated on C1–C3; C3's holder-model gate is now LIFTED)**
   latch the resolver's inputs at their
   chosen rungs (see INPUT CADENCE / ACCEPTANCE MODEL): generalize the direction
   pending→promote machinery to the global range (and LOR, if not already step-latched).
   This is the one slice that INTENTIONALLY moves behaviour for a mid-phrase window move —
   it gets its own reproduce-first test + a new golden pin; forward/window-held stays
   bit-identical.

## TESTS
- test_reverse_rotation_anchor.cpp (written): the acceptance oracle — rotation at the
  first-played step forward, reverse, and after a mid-run direction change.
- A golden-identity harness for step 1 (resolver mirrors the current two systems on the
  existing config sweep) — the safety net for the refactor.
- run_all + the march goldens (GOLDEN_A/B/BS, UA_NONDIV) unchanged on forward/offset-0.
- The multiplicative-direction matrix: a lane Reverse under a voice Forward under a
  global Reverse (and permutations) read the composed position — pin a few by hand.

## REMAINING QUESTION (minor — can be settled in slice 1)
- Class home + name (Singapore geography per CLAUDE.md). Candidate: a new
  src/dsp/engines file. Proposed phrasePos semantics: UNSIGNED "steps since the
  current-direction phrase entry" (0 at the first-played step BOTH directions, counting
  up), so the rotation anchor is phrasePos==0 everywhere and playedColumn is the
  direction-aware placement (fwd: startStep+phrasePos; rev: endStep-phrasePos, mod ring).
  Note: phrasePos "since entry" must survive a mid-run direction flip — the resolver
  holds the one transport state that makes this a pure view (the current globalWalk_
  already holds equivalent state; we keep ONE, not two).

## globalWalk_ — WHY TWO QUANTITIES, AND HOW TO RETIRE THE FIELD (Rodney + CC, 2026-11-15)

STATUS (2026-11-16): direction (a) is DONE — shipped as SLICE 5. `globalWalk_` the field is
DELETED; `pos_.phaseClock(transportTick_)` is its closed-form view (`≡ (t−1) mod cachedLength`),
behaviour-preserving, with the identity harness's `tickPhase` guard pinning it stays the TICK phase
(not the column). Direction (b) — collapse abs onto the column (reset abs to the window top at a
re-author) — is now RETIRED. The "unit of absoluteness" is DECIDED: **TRANSPORT** (Rodney,
2026-11-16). So (a) is not just the shipped move, it is the END STATE — (b) is not to be picked.

WHY TRANSPORT (the decision): the span observation collapses the choice. Abs walks only
`cachedLength` positions regardless (`ft0 = wG + offG`, `wG = (t−1) mod cachedLength`) — the FULL
ring only when the phrase is the ring. So "absolute" is an ANCHORING semantic (not re-anchored to
cycle/rotation), NOT a span; the span is the phrase in BOTH (a) and (b). That leaves exactly ONE
thing distinguishing transport from phrase: the mid-phrase RE-AUTHOR. And that question is already
answered — C2 KEEP-COUNTING: a mid-phrase length change keeps `phrasePos` counting (folds mod
newLen), never re-seeds, because reseeding snaps a phased lane. (a)/transport applies that same rule
to the abs read; (b)/phrase is the reseed C2 rejected, applied to abs alone. Plus HOLDER UNIFORMITY:
every holder is `≡ raw-coord mod own-period`, transport-anchored; `phaseClock` is just the top one —
(b) would make it the single special holder that carries window-edit history. (b) has standalone
appeal ("abs = step N of the current pattern, re-established on edit"), but adopting it here means
reversing keep-counting to stay consistent — a bigger move, against the "don't snap phased lanes"
line held throughout. So transport, by consistency, not taste. (Rodney's note: the earlier pull
toward (b) was a mental shortcut — defaulting "absolute/transport" to mean the full 16/32-step span;
it never did, it means the phrase, transport-continuous.)

SEPARATE, ORTHOGONAL axis (PARKED — "if ever wanted"): the SPAN limit (phrase, not ring) is
independent of transport-vs-phrase — both are phrase-bounded. To make abs actually cover ALL 32
regardless of phrase length, change the abs PERIOD from the phrase (`mod cachedLength`) to the RING
(`mod kMaskRing`). Cost: abs then reads the parked steps beyond the active phrase (usually empty), so
only wanted if "a lane that sweeps the whole ring while a shorter phrase loops underneath" is a
feature. Not part of the transport decision; noted so it doesn't hide inside it.

The analysis below is the record of why (a)/transport is the safe, correct move.

### The two quantities (one per addressing MODE)
Both feed the SAME read law `wnow = (laneTickFor(md, ft0+1, L) + rho) % L`, with a different
`ft0`/`rho`/`L` per mode (SequencerEngine.cpp derive):
- CYCLE: `ft0 = sPos` where `sPos = (dir>0) ? +phrasePos : -phrasePos`; `rho = R`. The read
  coordinate is DISTANCE FROM THIS CYCLE'S ENTRY (pos_.phrasePos()), because cycle re-anchors
  to rotation every cycle — the entry IS the origin (Rodney: rotation re-aligns the voice
  against the start forward / end reverse of the global pattern).
- ABS (MASK_DYNAMIC): `ft0 = wG + offG`; `rho = (R − O)`; `L = kMaskRing` (full ring). The read
  coordinate is the ABSOLUTE PHRASE POSITION (globalWalk_), anchored at the fixed phrase origin
  (global step 1), because abs does NOT re-anchor.

So the two quantities are: pos_.phrasePos() (entry-relative, for cycle) and globalWalk_
(phrase-absolute, for abs). Two modes → two origins.

### Why they DIVERGE in reverse (and coincide forward)
Forward the two origins COINCIDE (forward entry == phrase origin == step 1): phrasePos and
globalWalk_ both run 0,1,2,3,… — identical (the identity harness relies on exactly this).
Reverse the origins sit at OPPOSITE ends of the window: phrasePos is pinned to the reverse
entry (endStep) and counts UP; globalWalk_ is pinned to the phrase origin and counts DOWN.
They are reflections across the window, summing to lenG−1. The cycle branch takes the sign
(sPos = −phrasePos) so its coordinate runs down from 0; abs keeps the absolute phase running
down from the top. Same motion, two origins — they only collapse onto each other going forward.

### Why globalWalk_ is NOT a pure function of the column (the real reason it's a 2nd field)
In the contiguous case globalWalk_ tracks `playedColumn − offset` in BOTH directions — looks
redundant. A mid-run setWindow re-author BREAKS that equivalence: globalWalk_ re-binds from the
TICK (SequencerEngine.cpp:292 `globalWalk_ = wrapMod(transportTick_−1, newLen)`) while the
column parks to the window (PositionResolver::setWindow / engine :284). globalWalk_ is tied to
transportTick_ (the one absolute clock, :843) — it carries phase the column does not. This is
the "third desync class" the identity harness documents.

### TRACE — length 8→4 shrink mid-phrase (offset 0, forward, rot 0). The behaviour at stake.
Walk forward to col 5 (coherent: globalWalk_ = col − offset = 5), then setWindow(4,0) fires
(new window cols 0–3). At the re-author: column parks to 15; globalWalk_ = wrapMod(6−1,4) = 1.

| event | tick | globalWalk_ | col | phrasePos | CYCLE ft0 | ABS ft0 (wG) |
|-------|------|-------------|-----|-----------|-----------|--------------|
| 6 pre | 6    | 5           | 5   | 5         | 5         | 5            |
| 7     | 7    | 2           | 0   | 0         | 0         | 2            |
| 8     | 8    | 3           | 1   | 1         | 1         | 3            |
| 9     | 9    | 0           | 2   | 2         | 2         | 0            |
| 10    | 10   | 1           | 3   | 3         | 3         | 1            |
| 11    | 11   | 2           | 0   | 0         | 0         | 2            |

- CYCLE runs 0,1,2,3,… — the new window plays FROM ITS TOP (re-author resets the cycle).
- ABS runs 2,3,0,1,… — keeps counting from where the TRANSPORT clock was; constant offset
  = (tick−1) mod newLen at the re-author instant. It preserves phase continuity with the drive.

That constant offset (2 here) is the ENTIRE difference between (a) and (b).

### (a) DECIDED — resolver coordinates both; behaviour-preserving
The defect today is not two quantities, it is TWO SITES applying the re-author law from two
inputs (engine :292 re-binds globalWalk_ from the tick; PositionResolver::setWindow parks col_
from the window) — same event, no shared authority, free to drift. (a): the resolver owns col_
AND a phaseClock_ (the globalWalk_ concept), and its setWindow applies the one re-author law to
both. Reproduces the trace above EXACTLY — zero played frames change. The globalWalk_ FIELD is
deleted; the globalWalk_ CONCEPT lives on as resolver phaseClock_. This is safe before C3.

### (b) PARKED — collapse abs onto the column; a behaviour change, gated on C3
(b) makes abs read `col − offset`, so ABS would run 0,1,2,3,… too — RESET to the window top at
every live edit, discarding transport phase. Under the CURRENT semantics (absolute = locked to
the global transport, survives window edits) this ERODES the abs↔cycle distinction: the two
modes would converge in anchoring at exactly the re-authors that a live-mod sequencer spends
its time doing. (b) is principled ONLY if C3's holder model redefines the unit of absoluteness
from the TRANSPORT to the PHRASE: if re-authors latch at phrase boundaries (C1) and abs resets
each phrase BY DESIGN, then col and tick-phase coincide at every re-author by construction, (b)
is the correct reading, and the phaseClock_ becomes redundant.

### Decision tree
1. **[DONE]** (a) shipped (SLICE 5): `globalWalk_` the field retired to `pos_.phaseClock`, one
   coherent re-author law, no behaviour change.
2. **[DECIDED — 2026-11-16: TRANSPORT]** the unit of absoluteness is the transport, so (a) is the END
   STATE and (b) is RETIRED — settled by C2-keep-counting consistency + holder uniformity (see STATUS
   above), with the span observation showing both options are phrase-bounded so only the re-author
   distinguishes them. `phaseClock` stays. Nothing further on this axis.
3. **[PARKED — orthogonal]** the SPAN axis (abs period = phrase vs ring). Independent of 1-2; pick up
   only if a full-ring abs sweep under a shorter phrase is ever wanted (see STATUS).
