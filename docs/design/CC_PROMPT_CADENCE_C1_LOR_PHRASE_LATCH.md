# CC PROMPT — CADENCE C1: phrase-latch the nested LOR (the FIRST deliberate behaviour change)

The position/holder refactor arc is done (`pos_` is the one authority, the counter is dead, slices
1-5 + step 3b + step 4-final merged). This is the FIRST slice that CHANGES BEHAVIOUR on purpose:
nested L/O/R modulation currently takes effect PER STEP (the derive reads the live LOR stores every
advance) — which is too chaotic. C1 latches it so a nested LOR change takes effect only at the
level's ADDRESSED-AGAINST FRAME. Design: DESIGN_POSITION_UNIFICATION.md — the CADENCE addendum (C1 +
the C1 REFINEMENT "addressed-against frame", the "WHAT REACTS WHEN" table, the 8→6 trace).

## ⚠️ DISCIPLINE SHIFT — READ THIS FIRST (different from slices 1-5)
Slices 1-5 were BIT-IDENTICAL refactors; their contract was "goldens byte-identical." THIS SLICE IS
NOT. It is a DELIBERATE behaviour change, so the march goldens WILL change, and that is CORRECT — do
NOT try to keep them identical. The discipline is **REPRODUCE-FIRST + NEW PINS**:
1. Write a test that PINS the new behaviour (a mid-phrase nested-LOR change does NOT take effect
   until the level's next frame boundary) and confirm it FAILS on the current per-step engine first.
2. Implement. The new test PASSES; the old per-step behaviour is gone.
3. REGENERATE the affected march goldens deliberately, and in the PR explain WHICH changed and WHY
   (the LOR now latches) — a reviewed, intentional golden update, not a silent one.
If you find yourself fighting to keep a golden byte-identical, STOP — that instinct is wrong for this
slice.

## CARDINAL RULE
VERIFY AGAINST CODE AND TESTS. Prove the change with the reproduce-first test (FAILS before, PASSES
after). No Rack here — the display half (C4) is Rodney's leg (see NON-SCOPE).

## BRANCH
Off `lane-expander-refactor`. Branch `feat/cadence-c1-lor-phrase-latch`. PR into it.

## THE CHANGE
Introduce a LATCHED "effective LOR" per level. The derive reads the EFFECTIVE (latched) L/O/R instead
of the live store; the effective value is refreshed from the live store ONLY at the level's
addressed-against frame edge. Apply to ALL nested LOR (Rodney: "all nested LOR controls — voice
lanes AS WELL AS probability lanes").

Addressed-against frame per level (the UAS nesting — a level latches at its PARENT's wrap):
- **Voice-lane LOR** (`voiceLorStore_[v]`, `macroVoiceLor_`) — parent is the GLOBAL → latch at the
  **global phrase** edge: `pos_.phrasePos() == 0` (the `wrapped` signal, SequencerEngine.cpp ~:479).
- **Probability-lane LOR** (`polyLOR(vs, LaneId{...})` — variation/legato/etc.; and the mono strands
  via `lor(STRAND_...)`, mono = voice 1's lanes) — parent is the VOICE → latch at the **voice-frame**
  edge: `voiceFramePhase_[v] == 0` (the holder phase, ~:795; mind `voiceFrameReadsFullPhrase(v)`).

## KEY: the boundary is LIVE, edge-triggered — NOT a period counter
Key the latch on the parent holder being AT its edge (`phrasePos==0` / `voiceFramePhase_==0`), re-read
each advance — do NOT precompute a period and count to it. Why: the parent's length is a STEP-rate
control (global L+O, C1), so the boundary MOVES. The 8→6 trace: global length 8→6 mid-phrase means
the global wrap — and thus the voice-lane latch point — moves to 6, earlier. Edge-detection on the
live `phrasePos` gets this for free; a precomputed period would latch at the stale 8. (This is the
C2 keep-counting interaction: the global walk keeps counting and folds mod newLen; the LOR latch
rides that live wrap.)

## READ SITES (swap live store → effective) — ENUMERATE them yourself, do not trust this list to be complete
The live LOR reads in the derive to migrate (grep `lor(`, `polyLOR(`, `voiceLorStore_`,
`macroVoiceLor_` across SequencerEngine.cpp): the rLen/rhythm reads (~:762/:831/:1383), the macro
voice reads (~:782/:850/:1004), `voiceLorStore_[v][...]` (~:839/:908), the poly lane reads. The
POSITION-derive reads these for L/O/R; those are the ones that must see the EFFECTIVE value.

## WHAT STAYS LIVE (do NOT latch)
- **Global length + offset** — C1 says these are the real-time (STEP-rate) class; they already apply
  at the step. Do NOT phrase-latch them. (They are the DRIVE reposition, MeloDicer-native.) Confirm
  they are step-rate; change nothing if so.
- Direction (C3) — separate slice; not here.

## EXISTING SCAFFOLDING — reconcile first
There is already an `effectiveVoiceLor` reference (~:836, a comment contrasting it with the raw
`voiceLorStore_`/`macroVoiceLor_` read). CHECK whether partial effective-LOR scaffolding exists and
build on / reconcile with it rather than adding a parallel mechanism (ODR/DRY).

## NON-SCOPE
- **C4 (display cadence) — Rodney's Rack leg.** Once the engine latches, the Sands range must DRAW
  from the effective (latched) LOR so it moves at the phrase, not per step (the range IS the mod
  display). That's the widget half; this slice is the engine latch only. Note it for the feel-test.
- **C2 keep-counting** — the resolver's `phrasePos` already folds `mod len`, so this is mostly
  VERIFY-and-PIN (a golden that a mid-phrase global-length change folds, doesn't reseed), not new
  code. Add the pin; don't rebuild it.
- **C3 per-level direction** — separate slice (it reuses THIS slice's latch machinery, so it comes
  after).

## ACCEPTANCE
1. **Reproduce-first pin**: a new test asserting a mid-phrase change to a voice-lane L/O/R takes
   effect at the next GLOBAL phrase edge (not the next step), and a probability-lane L/O/R at the
   next VOICE-frame edge. Confirmed FAILING on the pre-slice engine, PASSING after.
2. **The 8→6 live-boundary pin**: shrink global length mid-phrase; assert the voice-lane latch point
   moves to the new (earlier) wrap — edge-triggered, not stale-period.
3. `bash test/run_all.sh` green. March goldens: the ones exercising nested-LOR modulation CHANGE —
   regenerate deliberately and list them in the PR with the one-line reason (LOR now phrase-latched).
   Goldens with NO nested-LOR modulation stay byte-identical (confirm — a change there is a bug).
4. `test_16_triage` (GATE 1) green; identity + reverse + phase oracles green (position math unchanged
   — only WHEN the LOR is sampled changed).

## FEEL-TEST (Rodney, in Rack)
- Sweep a voice-lane L/O/R while running: the window now STEPS to its new shape on the phrase
  boundary instead of smearing every step. Same for a probability lane at the voice cycle.
- Confirm global length/offset still react immediately (step-rate) — only the nested LOR latches.
- (C4 leg) point the Sands range at the effective LOR so the display matches the latched cadence.

## DELIVERABLE
On `feat/cadence-c1-lor-phrase-latch`, PR into `lane-expander-refactor`. The effective-LOR latch +
the reproduce-first pins + the deliberately-regenerated goldens (explained in the PR). Rodney
feel-tests + does the C4 display leg.
