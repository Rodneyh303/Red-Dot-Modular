# DESIGN — unify the TWO position systems into one robust authority (DRY/ODR)

STATUS: scope LOCKED (decisions below). Ready to become the CC implementation prompt /
slice 1. Branch: off fix/macro-voicelane-shared-frame.

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
