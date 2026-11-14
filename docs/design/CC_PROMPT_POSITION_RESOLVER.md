# CC PROMPT — unify the two position systems: SLICE 1 (the pure-mirror resolver)

READ FIRST: DESIGN_POSITION_UNIFICATION.md (the full problem, the locked decisions, the
multiplicative direction rule, the whole slice plan). Then this prompt implements SLICE 1
ONLY. Branch off fix/macro-voicelane-shared-frame.

This is the DRY/ODR fix for the transport position: today "where are we in the phrase" is
computed TWICE — stepIndex (the played column; SequencerEngine.cpp:471-492, its own
seeding + isStepInWindow skip) and globalWalk_ (the phrase accumulator the derive folds;
:835, read in deriveHolders). They desync in reverse. Slice 1 introduces ONE authority
and PROVES it reproduces both, consuming nothing — so it changes NO behaviour and cannot
move a golden. Do not migrate any consumer in this slice.

## WHAT TO BUILD (slice 1)
A stateless position resolver (a class; propose a Singapore-geography name — e.g.
Fullerton/Esplanade — for Rodney to confirm) in src/dsp/engines/. It holds the ONE
transport-position state and exposes PURE views, each a function of (transport motion,
the window [start/end,len,offset], the composed per-level LaneDir):

- `playedColumn()` — the ring column the transport is on (today's stepIndex). Forward:
  wrapMod(startStep + phrasePos, ring); reverse: wrapMod(endStep - phrasePos, ring). The
  window is contiguous (setWindow builds windowMask from [offset,offset+length)), so the
  isStepInWindow scan is a no-op in the contiguous case — fold it in, don't special-case.
- `phrasePos()` — UNSIGNED steps since the CURRENT-DIRECTION phrase entry: 0 at the
  first-played step in BOTH directions (forward startStep, reverse endStep), counting up.
  This is the coordinate the derive will fold (slice 2) and the rotation anchor
  (phrasePos==0, both directions).
- the composed direction/sign for the holders: the MULTIPLICATIVE per-level rule — global
  (signMotion; forward-only in CLOCK, bidirectional in GATE/PHASE) × voice frame
  (voiceLorDir_/macroVoiceLorDir_) × lane (laneDir_/laneDirV_/macroLaneDir_), via the
  existing laneTickFor + laneDirSign(signMotion*dirSign) products. Compute from the ONE
  state so no two consumers can disagree.

Keep it STATELESS in the POSITION_HOLDER_MODEL sense: one transport-position state; the
three views are pure functions of it; nothing accumulates a second copy. phrasePos "since
entry" must remain correct across a mid-run direction flip — derive it from the one state
+ the current direction, not a from-reset formula (an lenG-2 from-reset constant FAILS
after a direction change — see the design doc).

## THE GOLDEN-IDENTITY HARNESS (the whole point of slice 1)
Add test/test_position_resolver_identity.cpp (headless, in run_all, $SE $GS $PE $CE):
drive the engine over a broad sweep and assert the resolver MIRRORS today's engine
bit-identically:
- `resolver.playedColumn()` == `eng.stepIndex` at EVERY step.
- `resolver.phrasePos()` reproduces the forward globalWalk_ (forward: phrasePos ==
  globalWalk_) and the direction-aware relation in reverse (document the exact relation;
  it is the fixed one, not the current drifting globalWalk_).
- Sweep: FORWARD and REVERSE; every LaneDir {Forward,Reverse,Pendulum,PingPong} at the
  voice and lane levels (the multiplicative matrix — at least a representative set incl.
  a Reverse lane under a Forward voice under a Reverse global, and permutations);
  non-divisive lengths (e.g. 7 under 16, 5 under 12); offsets {0,3,5}; from reset AND
  after a mid-run direction flip (drive forward N, then reverse, then forward again).
- CLOCK mode: the global stays forward; still exercise voice/lane direction composition.

If the resolver cannot mirror some case bit-identically, STOP and report it — that is a
real discrepancy to understand, not something to paper over.

## GATES (slice 1)
- run_all GREEN on both SDK legs; GOLDEN_A/B/BS + UA_NONDIV + the march digests
  BIT-IDENTICAL (slice 1 consumes nothing, so they MUST be untouched). anchor-audit +
  array-guard clean.
- test_position_resolver_identity GREEN (the mirror holds across the whole sweep).
- The reverse-rotation oracle test_reverse_rotation_anchor.cpp stays UNREGISTERED this
  slice (it fails on today's engine by design; it goes green in slice 2 when the derive
  migrates). Do not wire it into run_all yet.

## DO NOT (slice 1)
- Do NOT migrate any consumer (deriveHolders, output, UI, Lantern, sync) to the resolver
  yet — slice 1 is a pure mirror. Behaviour change is slice 2+.
- Do NOT touch the OUTPUT mask (maskMacro*/maskAnchored_/maskLocalEast_/outputMaskPasses)
  — windowMask/isStepInWindow is the global TRANSPORT window, a different system.
- Do NOT change stepIndex or globalWalk_ yet (they stay the live systems until the
  resolver is proven and consumers migrate in later slices).
- Do NOT re-pin any golden (there is no behaviour change to justify it).

## AFTER SLICE 1 (do NOT start without Rodney's review — outlined in the design doc)
S2: migrate deriveHolders to read phrasePos + composed direction from the resolver;
    register + green test_reverse_rotation_anchor (the reverse rotation fix lands here);
    forward/offset-0 bit-identical; confirm no golden pins the reverse mapped reads.
S3: migrate the stepIndex consumers (Intertropical output, Monsoon sync/lock/phrase tier,
    Lantern, the Sands visuals) to playedColumn, one site at a time, each verified.
S4: delete the redundant field once it has no readers.
S5: the rotation anchor is phrasePos==0, direction-agnostic — remove any globalWalk_-offset
    experiments.
Zoo: expect every slice bit-identical on the existing configs; reverse+mapped is the only
     intended behaviour change, and only from S2.
