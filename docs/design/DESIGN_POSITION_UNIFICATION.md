# DESIGN — unify the TWO position systems into one robust authority (DRY/ODR)

STATUS: scope LOCKED (decisions below). Ready to become the CC implementation prompt /
slice 1. Branch: off fix/macro-voicelane-shared-frame.

ADDENDUM (2026-11-15, OPEN): see "INPUT CADENCE / ACCEPTANCE MODEL" below — the RATE at
which position's inputs (global range, LOR, direction, modulation) are recognised/accepted,
and how that composes with the existing effect-timing ladder. Decisions C1–C3 are OPEN
(Rodney to settle). Design-ahead: this lands as its OWN slice AFTER S2, NOT slice 1.

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

STATUS: DRAFT addendum (2026-11-15). Decisions C1–C3 are OPEN — Rodney to settle. This is
design-ahead; it is NOT slice 1 (slice 1 keeps today's cadence to stay golden-identical).

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
- Sands LOR off/rot → 1/16-step.
- Sands LOR len → the same question as C1, one level down.
- global off → 1/16-step (TODAY control-rate — fix).
- global len → see C1.
- per-level direction → StepEdge or Phrase via laneFlipQuant (already correct); the resolver
  reads the PROMOTED dir, never the pending one.
- modulation (CV) → NOT a rung of its own; it RIDES the target's rung (CV→LOR latches at the
  LOR rung, CV→direction at the direction rung). Nothing new to decide for modulation once
  the target rungs are set.

### OPEN DECISIONS (Rodney)
C1. GLOBAL LEN rung — STEP vs PHRASE. off/rot and global-off sit at 1/16-step. But changing
    the PHRASE LENGTH mid-phrase is arguably the "scene advance" class the ladder already
    puts at the phrase boundary. Decide: global len latches at the step edge (consistent with
    LOR) OR at the phrase boundary (structural). [Lane len is the same question — C1 governs
    both unless you split them.]

C2. phrasePos CONTINUITY across a mid-phrase window/LOR change — the subtle one for a
    POSITION authority. When len/off change, does phrasePos KEEP COUNTING (the window resizes
    UNDER a continuous counter) or RE-SEED to 0 (treat the change as a phrase re-entry)?
    Re-seeding re-fires the rotation anchor (phrasePos==0) on EVERY range change — which may
    be the wanted "re-sync rotation on resize" feel, or an unwanted retrigger. Directly
    couples to the acceptance bug THIS doc already fixes. Decide the semantics in BOTH
    directions and on wrap.

C3. COMPOSITION across levels with DIFFERENT promote edges — voice flips at StepEdge, lane at
    Phrase: the multiplicative sign is coherent only at a COMMON latch. Decide: one promote
    edge for the WHOLE composition (simpler, less flexible) OR per-level promoted values each
    latched at its own edge (flexible, but risks a one-step HALF-PROMOTED product). Direction
    today derives SIGN from position each step (sidesteps this for sign), but the DIR enum
    promotes per-level — so the question is real for the composed TRAVERSAL.

### Slice placement
NOT slice 1 (pure mirror, keeps control-rate cadence, golden-identical). Lands as its OWN
slice AFTER S2 (derive migration + reverse fix): step-latching the global range is a GENUINE
behaviour change — golden-safe only where a test HOLDS the window constant (GOLDEN_A/B/BS,
UA_NONDIV likely do; the mid-phrase MOVE is exactly what wants a NEW pin). Reproduce-first: a
test that moves len/off mid-phrase and asserts the playhead + holders accept the change on
the CHOSEN edge (not control-rate), per C1/C2.

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
