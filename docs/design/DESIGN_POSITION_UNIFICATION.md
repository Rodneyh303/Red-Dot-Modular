# DESIGN — unify the TWO position systems into one robust authority (DRY/ODR)

STATUS: scope LOCKED (decisions below). Ready to become the CC implementation prompt /
slice 1. Branch: off fix/macro-voicelane-shared-frame.

ADDENDUM (2026-11-15, OPEN): see "INPUT CADENCE / ACCEPTANCE MODEL" below — the RATE at
which position's inputs (global range, LOR, direction, modulation) are recognised/accepted,
and how that composes with the existing effect-timing ladder. C1 (rungs), C2 (keep-counting),
C4 (display cadence) and C3 (per-level direction, GATED on the position-holder migration) all
DECIDED 2026-11-15. Design-ahead: C1/C2/C4 land as their OWN slice AFTER S2; C3 rides the
counter-removal migration, not that slice. NOT slice 1.
See also DRIVE DIRECTION MODEL below (2026-11-15) — gate direction is GLOBAL, not voice-1/mono-
sourced; event modes default forward with a REVERSE-ALL control; HOLD per-voice; and the
VARIATION-centre fix (knob 0.5 = repeat-in-place, no forward creep).

## DECISIONS (Rodney, locked)
1. SCOPE: ALL modes. NUANCE — in CLOCK mode the GLOBAL playhead is forward-only
   (signMotion >= 0), but the per-level voice/lane direction modes are as flexible as in
   GATE/PHASE. So: the reverse GLOBAL traversal (first-played == endStep) occurs only in
   GATE/PHASE; the MULTIPLICATIVE per-level direction composition (voice/lane Reverse/
   Pendulum/PingPong) is active in ALL modes. The resolver takes the global direction
   from the mode and composes the per-level LaneDirs always.
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
A single resolver (a class — Singapore name TBD, e.g. **Fullerton** / **TransportPos**)
that owns the transport position and is the ONE source every consumer reads:

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
each bit-identical on the existing (forward / offset-0 / ≤16) configs:

1. **Introduce the resolver as a PURE MIRROR.** Compute playedColumn + phrasePos inside
   it, assert they reproduce the CURRENT stepIndex and globalWalk_ bit-identically for a
   broad config sweep (a golden-identity harness: forward, reverse, every LaneDir, the
   non-divisive lengths, offsets). Nothing consumes it yet. GOLDEN-IDENTICAL by
   construction.
2. **Migrate the DERIVE** to read phrasePos from the resolver instead of globalWalk_.
   The forward phrasePos == globalWalk_ (bit-identical); reverse now has a correct,
   stable entry. test_reverse_rotation_anchor goes green here; run_all + goldens stay
   green (only reverse+mapped reads change, which no golden pins — confirm).
3. **Migrate the consumers** of stepIndex (output, sync, UI, Lantern) to playedColumn,
   one site at a time, each verified no-change.
4. **Delete** the redundant field once it has no readers.
5. The rotation-anchor fix is now `phrasePos == 0`, direction-agnostic — delete the
   fragile globalWalk_-offset attempts entirely.
6. **(CADENCE slice — after S2, gated on C1–C3)** latch the resolver's inputs at their
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
