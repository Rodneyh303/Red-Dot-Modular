# CC PROMPT — step-advance CONSUMPTION (gate advancePlayhead on probability) — the behaviour

CONTEXT: the subgate build (feat/sands-distribution-expander) STORES the ADVANCE thresholds (ratchet/ghost
advance probabilities, three-way resolved, default follow-main=VARIATION) but EXPLICITLY does NOT consume
them — advancePlayhead(dir) at SequencerEngine.cpp:771 is still UNCONDITIONAL. This prompt builds the
CONSUMPTION: gate the playhead advance on a probability so a gate sometimes HOLDS the step (= re-output the
same step-addressed draw; see "repeat semantics" — NO re-draw branch, the data is step-addressed).

DEPENDENCY/BRANCH: the resolved ADVANCE threshold accessor lives on the subgate branch (SubgateKnobs.hpp,
ADVANCE=2). Build this EITHER on feat/sands-distribution-expander (where the thresholds exist) OR after it
merges to lane-expander-refactor. Do NOT build on bare trunk (no threshold source there yet).

## STAGE 1 — MAIN-GATE unipolar probabilistic advance (build first)
Site: SequencerEngine.cpp:771 (gate mode, `bool wrapped = advancePlayhead(dir);`, after the
`rm.ticked(Step) || muted` guard). ONLY this site, ONLY gate mode (Mode B), ONLY forward-or-hold.
- Gate the advance: draw u (the Step-rate uniform for this step — the SAME step-addressed draw discipline;
  NOT a new stream). Advance iff u < p_advance; else DO NOT call advancePlayhead → the playhead holds →
  the same step (same counter/strand address) is re-output. NOTHING else changes (no re-draw code — holding
  = reading the same step-addressed data by construction).
- p_advance = the RESOLVED main-lane VARIATION probability in gate mode (per-voice, through the resolver/
  effectiveProbability path — warps/correlates/CA like any lane; NOT the raw knob). In the degenerate case
  (no Straits) this is Monsoon VARIATION, per the subgate spec's mapping.
- decideAdvance(p, u) → {ADVANCE, HOLD}: keep it a PURE function (like RhythmDecisionCore) so it is
  unit-testable in isolation.
- GOLDEN PIN (mandatory): at p_advance = 1 → ALWAYS advance → BYTE-IDENTICAL to today's unconditional
  advance. The goldens MUST hold at p=1. Only p<1 changes behaviour. This is the regression gate.
- The 840/877/881/986 advance sites (other modes) are UNCHANGED — gate mode (771) only.

## STAGE 2 — BIPOLAR (forward / hold / backward) — after Stage 1 green
- Variation gate-mode value becomes BIPOLAR, mapping (concrete):
  u ∈ [0,1); p = resolved bipolar VARIATION ∈ [-1,+1].
    p ≥ 0: advance FORWARD iff u < p, else HOLD.   (p=+1 = Stage-1 identity)
    p < 0: advance BACKWARD iff u < |p|, else HOLD.
    p = 0: always HOLD.
- advancePlayhead(dir) already takes direction: forward = dir, backward = -dir. Backward past step 0 WRAPS
  to N-1 (advancePlayhead already handles wrap; verify both directions wrap).
- GOLDEN PIN at p=+1.

## STAGE 3 — RATCHET + GHOST levels — consumes the stored subgate ADVANCE thresholds
Now that the subgate build stores the per-level ADVANCE thresholds (SubgateKnobs, ratchet/ghost, three-way
resolved per voice), WIRE them into the SUBDIVIDED path:
- At each RATCHET subgate rise and each GHOST gate rise, gate that level's advance on ITS resolved ADVANCE
  threshold (same decideAdvance(p,u), same step-addressed hold semantics), reading the level-resolved value
  the subgate build already computes (VoiceState.subRes / the level-resolved threshold the subdivided path
  reads for REST/LEGATO/ACCENT — ADVANCE is the 4th decision at {RATCHET,GHOST}).
- ONE PLAYHEAD constraint: main + ratchet + ghost share the single playhead; direction is set by the LAST
  gate event that advanced. No per-level playheads.
- GOLDEN PIN: with all advance thresholds = 1, byte-identical to Stage 1/2.

## REPEAT SEMANTICS (no re-draw — already corrected in CC_PROMPT_GATE_STEP_ADVANCE_SPEC.md)
The random data is addressed BY STEP POSITION (Philox counter / strand index). HOLD = not moving the
playhead = reading the SAME address = the SAME data, by construction. There is NO held-vs-redraw choice and
NO re-draw branch to write. Stutter = identical repetition of the held step, inherently.

## RELATION TO THE RDM
Advance is a SEPARATE decision from the RDM's five predicates (which-step-visited vs what-a-step-does) — it
lives at the advance site, not as an RDM predicate, but follows RDM discipline (pure decideAdvance,
gate-mode-only, golden-pinned).

## DONE =
- Stage 1: advancePlayhead at :771 gated on resolved-VARIATION p via pure decideAdvance; gate mode only;
  p=1 byte-identical golden; other sites/modes unchanged. decideAdvance unit test (table + p=1 identity).
- Stage 2: bipolar mapping (fwd/hold/back), backward wraps, one playhead; p=+1 golden.
- Stage 3: ratchet/ghost advance gated on the stored subgate ADVANCE thresholds (subdivided path), one
  playhead, all-thresholds=1 golden.
- run_all.sh green, UNIFORMITY_STRICT green, warn-sweep clean, goldens hold at every identity pin.
