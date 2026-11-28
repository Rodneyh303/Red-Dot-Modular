# CC_PROMPT_POSITION_RESOLVER_NETSTEPS — the net-steps read law (steps 1–3a shipped; 3b open)

STATUS: steps 1–3a are in the open PR stack #367 → #368 → #369 (into master); step 3b is OPEN
with its diagnosis instrumented (branch `debug/reseed-logging`, feel-test only). This doc records
the law, the measurements that forced each choice, what was DISPROVEN, and what is still unknown —
so none of it is re-derived. The session that produced it (2026-11-25/26) burned hours re-deriving
things twice; every claim below is cited to a test, probe, or PR.

The umbrella this feeds: `POSITION_RESOLVER_CONSOLIDATION.md` (the resolver-ownership target —
one class owns position; the law stops being re-applied by hand at ~10 sites). The decision this
builds on: C3, `transportTick_` is the unit of absoluteness (PR #364). Cadence context:
`CC_PROMPT_CADENCE_C1_LOR_PHRASE_LATCH.md` (live parent-wrap latching), the C2 keep-counting
ruling in `CC_FIX_GLOBAL_BELOW_VOICE_PHASE_DESYNC.md`, and `UNIFIED_ADDRESSING_SYSTEM.md`
(the authoritative LOR nesting: voice R ← global step 1; lane R ← voice step 1).

## THE LAW (what shipped)

`readCoord()` is one signed counter of NET motion since the entry, folded into the global window:

    readCoord = netSteps_ % lenG        // C++ truncated: the sign rides the count

No `entryDir_`, no direction field in the read. `atPhraseEdge()` is the SAME counter's edge
(`netSteps_ % len_ == 0`, len_ read live per call), and the five former `phrasePos()==0` edge
sites (draw edge 506, identity row 837, voice LOR latch 939, lane-offset COMMIT gate 1030,
atPhraseEdgeC1 1227) ride it — so the reads and the global edges share ONE coordinate.

- PR #367 — the pins: nothing in the suite covered travel PAST the phrase entry
  (`test_reverse_rotation_anchor` gate 3 reverses down to `fwdCol[0]` and stops ON the entry).
  `test_position_past_entry_pins` closes it.
- PR #368 — the swap: `entryDir_` retired. Behaviour change, deliberate: past its own entry the
  read is HISTORY-INDEPENDENT — identical to a cold start in that direction (reproduce-first:
  140 checks fail on the entryDir_ resolver, all pass after). The retired thing was the TRUE-
  RETRACE guarantee ("reverse replays the forward path, but only back to one specific entry") —
  partial, bought with hidden state, and the reverse setting can only guarantee the probability
  MATERIAL anyway (selection/modulation sit downstream of position).
- PR #369 — the edge migration (step 3a): zero behavioural tests moved (only a source-shape grep
  gate) — which also means flip-edge timing had ZERO coverage; gate 4 of the pins test now pins
  the edge law at the resolver (pure-run equivalence with phrasePos()==0, the live boundary
  including the C2 trace "global 8→4 at step 8 ⇒ boundaries at 12 AND 16", flip independence).

Two writes to the counter besides counting, both deliberate:
- `snapTo` RE-SEEDS (a snap is a position authority; without it the phase-drive→clock handover
  resumed from the pre-snap count — found via `test_phase_holder_bridge`, restored unchanged).
- `setWindow` on a REAL change: parked column (window shrank from under it) → re-ARM to 0 /
  un-entered; in-window column → re-SEED to its signed phase in the new window. KEPT — see 3b.

## THE ORIGIN — measured, and an information floor

"Where is zero?" is the whole residual question. The first played step must read 0 in BOTH
directions (forward enters at the window's LEFT end, reverse at its RIGHT — a fact about the
instrument). Scoreboard, full suite each (2026-11-26):

| origin law                                   | extra state | suites failing |
|----------------------------------------------|-------------|----------------|
| SHIPPED: first motion ARRIVES, later count    | 1 bool      | 0              |
| start at −1, count every move                 | none        | 2 (both reverse) |
| stateless, sign/origin from CURRENT direction | none        | 5 (incl. both core reverse goldens) |
| count every move from 0                       | none        | 12+ (whole address lattice slides by one) |

- `netEntered_` is NOT the retired retrace state: it DEFERS the origin choice one move until the
  first motion reveals the direction. It never changes mid-run; behaviour stays history-independent.
- Proof the bit is a floor, not a style choice: readCoord is NOT a function of
  (netSteps, currentDir, lenG) — measured collisions, e.g. (netSteps=1, dir=+1) must read 1 in a
  pure-forward run and −4 in a rev3→fwd run (18 collision classes / 64 steps). A −1 start is the
  forward pre-start; the reverse pre-start is +1; one constant cannot be both.
- Current-direction origins fail at the FLIP itself (first reversed step moves the coordinate
  FORWARD: tick −1 but −dir +2), not merely past the entry.

## WHY NET-STEPS IS BETTER IN A DYNAMIC SETTING — the path-independence payoff

The swap looks like it only TRADES one thing (the old entryDir retrace past the entry) for
history-independence. Measured, it is a strict UPGRADE for dynamic play, and here is the reason
stated plainly (Rodney + Claude, 2026-11-26).

WITHIN A FIXED WINDOW (no re-author, no snap) THE READ IS A PURE, TOTAL FUNCTION OF netSteps:

    read = netSteps mod lenG        (every netSteps value -> the same cell, ALWAYS)

There is NO path dependence. How you arrived at a given netSteps — forward, reverse, after any
number of entry crossings — does not change the read. The entry is simply the point where
netSteps == 0; from it you can run retraceable journeys forward AND back (or back then forward),
and every leg retraces exactly.

Two cases that make it concrete (measured on the fix/readcoord-netsteps-truncated resolver, lenG=5):

  FIRST FORWARD:        netSteps  0  1  2  3      -> cells 0 1 2 3
  REVERSE past entry:   netSteps  2  1  0 -1 -2   -> cells 2 1 0 4 3
  FORWARD AGAIN (-2 ->):netSteps -1  0  1  2  3 4 -> cells 4 0 1 2 3 4

  A. Reversed past entry into the "clean reverse" zone (netSteps −2), then forward again: we
     retrace the clean-reverse zone exactly — netSteps −1 reads cell 4 whether we pass it going
     back or going forward. Forward retraces back through the entry.
  B. Continue forward through the entry and beyond: netSteps 0,1,2,3 read cells 0,1,2,3 — byte-
     identical to the FIRST forward pass. Reverse from there and it retraces again. Arbitrarily
     many times.

THE OLD MODEL had exactly ONE retrace anchor, fixed at the first entry, and it BROKE the instant
you crossed it (past-entry read the drifting col-derived forward branch). Net-steps has NO anchor
and NO cliff: because the read is purely positional, the retrace/repeat guarantees NEST and
COMPOSE instead of replacing each other. As soon as you pass the point of one guarantee you are
laying down a new one, while the first can still be repeated or taken further. The "little given
up" (one anchored retrace) buys "retraceable everywhere, in both directions, across any number of
crossings" — the strictly richer thing for building deep forward/backward gestures.

CAVEATS (the honest boundaries of the claim):
- The purity holds BETWEEN position-authority events. A window re-author (the setWindow re-seed)
  and a snapTo re-anchor netSteps, so they re-base the function at that moment — by design (an
  external authority asserting a new position), not a leak.
- There is NO natural "reset to zero" regulator, and there must not be: the read tracks UNBOUNDED
  net motion precisely so reverse can span arbitrary forward phrases. A periodic zeroing would
  destroy that guarantee (you would lose retrace across the reset). The only re-anchors are the
  position authorities above (reset / re-author / snap). (int64 is effectively unbounded at audio
  rates.)
- The guarantee is on the READ (the position the resolver outputs). Whether the AUDIO retraces
  depends on the downstream (dice / slew / probability lanes) being reversible too — reverse
  guarantees the positional MATERIAL is identical, not that every stateful modulator replays
  identically. The read is now perfectly path-independent; the sound is as reversible as the lanes
  feeding off it.

## TWO COORDINATES, BY ROLE (not duplication)

`transportTick_` is a CLOCK — Σ motions, never repositioned. `netSteps_` is a POSITION —
repositioned by window edits and snaps. The identity `netSteps == tick − entrySign` holds ONLY
between re-seeds (measured: 0 violations in pure traversal; hundreds in the dip/bug6/holder
tests, seeded at setWindow/snapTo and persisting through later advances). They are NOT a DRY
violation; deriving one from the other would need the entry sign PLUS no re-seeds. A clock
dragged by window edits would be a broken clock. `voiceFrameEdgeLive_` spans both BY DESIGN:
identity row → `atPhraseEdge()` (counter); ABS1 → `phaseClock(transportTick_)` (clock);
CYCLE → the walk value the read folds (counter-derived).

## 3B — OPEN. What is disproven, what is unknown

The goal: decide the `setWindow` re-seed's fate. C2 favours keep-counting, BUT removing the
re-seed fails `test_global_dip_differential` on 173 checks ("no commit off the live anchor",
16→4→16 dip). DISPROVEN diagnoses (do not re-derive these):
- "The voice holders re-anchor on window change; make them keep-counting" — they ALREADY do:
  `voiceFramePhase_[v]` ≡ `wrapMod(tick−1+advanceOffset_[v], walkPeriod(vd,L))`, period read
  live (holder-vs-derivation: 0 diffs / 25,634 steps / 690 key changes at write site 1119;
  site 987 is that derivation by construction, `voiceFrameTickAt`).
- "voiceFramePhase_ is the third face blocking the re-seed" — it is READ on exactly one path,
  `abs1Read && passG` (SequencerEngine.cpp ~1399), and that path has ZERO suite coverage
  (site reached 112–2216×/test incl. `test_uas_abs1_mask_ring`; abs1Read true 0×). A holder
  maintained every tick for an untested path — BOTH a coverage gap to close and a provable
  cache-removal candidate ONCE covered.
- "Both laws differ on the C2 trace" — they don't; 8→4@8 gives boundaries 12 AND 16 either way.

UNKNOWN: the actual mechanism of the 173 failures. NEXT MEASUREMENT, in order:
1. `debug/reseed-logging` (pushed, stacked on #369; grep Rack log for `[reseed]`): one INFO line
   per counter re-seed with `delta = after − before` — the before value IS what free-running
   would have kept, so delta is the per-gesture divergence, live. Feel-test pending (Rodney).
2. Instrument the COMMIT gate (1030) + `voiceFrameEdgeLive_` branches with the re-seed removed,
   and diff the edge/read/anchor triple at the first failing commit of the dip. Identify the
   coordinate that moves. THEN decide the re-seed's fate (and only then revisit `netSteps_ ←
   derived` consolidation).

## THE REFACTOR THIS FEEDS (standing target — do not lose)

Per `POSITION_RESOLVER_CONSOLIDATION.md`: PositionResolver becomes the one owner of position
views so a law-change is one edit, not an `anchorBias`-style fan-out. Concrete remaining moves:
1. Resolve 3b (above) — the last edge/read split the resolver does not own.
2. Write the missing `abs1Read && passG` coverage; then replace the `voiceFramePhase_` /
   `frameKey_` holder pair with the derivation (state deleted, behaviour-identical by the
   measurements above — but ONLY once covered).
3. Fold the per-(voice,lane) tick family (`laneTickV_` / `laneSignV_` / `getStrandIdx` inputs)
   into resolver-owned views — `getStrandIdx(tick,L,O,R)` IS the net-steps read law; the lanes
   already speak it. UAS nesting (voice R ← global 1; lane R ← voice 1; `walkEdge` carries the
   reverse compensation) is the spec to preserve, pinned by `test_problane_phase_coupling` and
   `test_reverse_rotation_anchor`.

Feel-tests pending (the gate on all of it): #368 past-entry read delta; #369 flip cadence delta
(dice/slew redraw, LOR latch, commit after a mid-run reversal); `[reseed]` deltas vs the ears.
