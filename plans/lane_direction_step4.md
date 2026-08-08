# Step 4 — Per-lane gates (Scope 1: rhythm + legato, rhythm wins precedence)

Decision: articulation is classified per-lane (rhythm owns rest/sounding, legato owns slur),
each against its OWN temporal predecessor (advanced on its laneTick_). Rhythm/rest wins
precedence (rest cancels slur). Melody/octave/accent/variation stay property-reads (no gate).

## Why (the problem this solves)
Today the connect test uses the GLOBAL predecessor (`lastStepResult.decision` / `gs.slurForward`).
Reverse a lane and its temporal predecessor diverges from mono's global one → a reversed-lane
note can "connect" to a cell that isn't its lane-temporal predecessor (the doc's invariant #2
violation). Scope 1 makes the two articulation-owning lanes track their own predecessor.

## Safety property
All-forward: each lane's predecessor == mono's global predecessor, so behaviour is bit-identical.
The per-lane state only diverges when a lane is reversed. No change to the default/forward case.

## New per-lane gate state (SequencerEngine)
- `bool rhythmSoundedPrev_ = false` — did the RHYTHM lane's last tick position produce a
  sounding note (not a rest)? Replaces the global `prevPlayedSounded` in the connect test.
- `bool legatoSlurForward_ = false` — the LEGATO lane's own slur commitment (lead intent).
  Replaces `gs.slurForward` as `prevSlur` in the connect test.
- (Both already read their probability cells via `laneTick_[STRAND_RHYTHM/LEGATO]`, so the
  reversed read is already in place — only the PREDECESSOR test changes.)

## Composed decision in executeStep (rhythm wins)
1. **Rhythm gate**: read `r_rest` (already via `getRhythmStep()` → `laneTick_[RHYTHM]`).
   If rest and `canRest` → **Rest**. Update `rhythmSoundedPrev_ = false`. (Rhythm wins → gate closes.)
2. Else **sounding**: update `rhythmSoundedPrev_ = true`.
3. **Legato gate** (only if sounding): the connect test becomes
   `legatoConnects && (wasHeld || hadTail) && rhythmSoundedPrev_`
   where `prevSlur = legatoSlurForward_` (not `gs.slurForward`).
   - Connect + held → Legato/Tie (gate stays open, slide/extend).
   - Else → NewNote (fresh retrigger).
4. At the onset, roll the slur from `pe.legatoRandom[getLegatoStep()]` (already via
   `laneTick_[LEGATO]`) and commit to `legatoSlurForward_`.

## The mono gate envelope stays ONE state
`gs` (gateHeld/holdRemain/gatePulseRemain) is still the single audio envelope — it reflects
the COMPOSED decision. The per-lane gates are the DECISION INPUTS (predecessor + slur intent),
not separate audio gates. So the output is still one note per step with one envelope; only the
CLASSIFICATION (rest/legato/tie/new) now keys off the per-lane predecessors.

## Sync points
- `gs.slurForward` is still set (poly voices inherit it as `monoSlur`; Lantern reads it for the
  lead marker). It must mirror `legatoSlurForward_` so downstream stays consistent — simplest
  is to set `gs.slurForward = legatoSlurForward_` after the legato roll (one assignment, one
  source of truth). Or route all readers to `legatoSlurForward_`; the mirror is lower-risk.
- `lastStepResult.decision` stays global (it records the composed decision, which IS mono's
  gate). Poly voices read it for their gate-follow behaviour — unchanged.
- `rhythmSoundedPrev_` is updated at the END of executeStep (after the decision), so the NEXT
  step's connect test reads this step's rhythm outcome. Same timing as `lastStepResult` today.

## What does NOT change
- Melody/octave/accent/variation lanes: still property-reads, no gate, no predecessor. Their
  direction (via laneTick_) only changes which cell is read, not articulation.
- Poly per-voice: step 4 is mono-only for now. The poly analogue (per-voice-per-lane gates)
  is a later step — the per-voice tick storage (steps 1-2) is already in place for it.
- The `wasHeld`/`hadTail` gate read stays (audio-gate-based, direction-correct per the doc).

## Files
- `SequencerEngine.hpp`: add `rhythmSoundedPrev_`, `legatoSlurForward_` members.
- `SequencerEngine.cpp` `executeStep`: swap `prevSlur` source → `legatoSlurForward_`; swap
  `prevPlayedSounded` source → `rhythmSoundedPrev_`; update both at end of step; mirror
  `gs.slurForward = legatoSlurForward_`.
- `reset()`: zero the new state.
- (No persistence change needed unless we want the lane gate state to survive reload — it's
  runtime-derived from the lane ticks + decisions, so it rebuilds on first step.)

## Test
All-forward: identical behaviour (per-lane predecessor == global). Reverse the rhythm lane:
rest pattern reads backward, and a note after a reversed-rest correctly classifies as NewNote
(not a false legato). Reverse the legato lane: the slur roll reads backward, and a connect
correctly keys off the legato lane's own predecessor.
