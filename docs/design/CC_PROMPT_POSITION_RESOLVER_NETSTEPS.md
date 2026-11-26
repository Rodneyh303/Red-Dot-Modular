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
