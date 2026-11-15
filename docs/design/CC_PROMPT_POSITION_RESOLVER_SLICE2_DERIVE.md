# CC PROMPT — Position Resolver SLICE 2: migrate the DERIVE + fix reverse rotation

Continuation of `CC_PROMPT_POSITION_RESOLVER.md` (slice 1). Slice 1 landed `PositionResolver`
(`src/dsp/engines/PositionResolver.hpp`, member `pos_`, advanced at `SequencerEngine.cpp:495`)
as a PURE MIRROR that consumes nothing. This slice makes the first consumer read it, and fixes
the reverse-rotation bug in the process. Reference: `DESIGN_POSITION_UNIFICATION.md` migration
steps **2 and 5**.

## CARDINAL RULE (non-negotiable)
VERIFY AGAINST CODE AND TESTS, never a plausible narrative. Prove the fix with a test that
FAILS before and PASSES after. You CANNOT verify Rack/behavioural things (no Rack here) — this
task is fully mechanical and test-pinned; Rodney feel-tests in Rack. When in doubt, read the
code.

## BRANCH
`fix/macro-voicelane-shared-frame` (where slice 1 lives). Cut a slice branch off it; PR back
into it. The design docs (cadence addendum, drive-direction model) are merged onto this branch —
read `DESIGN_POSITION_UNIFICATION.md` as it stands there.

## THE ORACLE (reproduce-first — this is the whole acceptance test)
`test/test_reverse_rotation_anchor.cpp` ALREADY EXISTS (committed `7ee07366`), is **UNREGISTERED
and FAILS today by design**. Register it in `test/run_all.sh` (`$SE $GS $PE $CE`) and make it
pass. Its three gates:
1. **Both directions**: the macro voice read (`eng.macroFrameRead()`) never leaves the window
   (`windowContains`). Already holds.
2. **Both directions**: at every phrase entry (`t % lenG == 0`) the read == `rot` (= `O + R%L`,
   ring-wrapped). FORWARD passes today; **REVERSE FAILS** — the anchor is keyed to the forward
   phrase entry (`globalWalk_ == 0` / `wG == 0`), so reverse lands mid-phrase instead of at R.
3. **Forward-then-reverse** (`driveFwdThenRev`): after running forward and then reversing, every
   reverse phrase entry — detected by `stepIndex == endStep` (the O+L upper bound) — reads `rot`.
   Guards against the from-reset anchor being a transient.

The config sweep is in the test (`{8,4,8,0,3}` = Rodney's config, plus voice-shorter, full-ring
non-dividing, odd window). Do not weaken the test; make the engine satisfy it.

## THE SLICE (scope — exactly this, nothing more)
Migrate the DERIVE that anchors the cycle voice rotation to read the resolver instead of
`globalWalk_`:
- The voice rotation must land at **`pos_.phrasePos() == 0`**, which is 0 at the first-played
  step in BOTH directions by construction (`dir>0 ? col-startStep : endStep-col`), NOT at
  `globalWalk_ == 0` (forward-only). This is migration step 2 (derive reads `phrasePos()`) folded
  with step 5 (the rotation anchor becomes `phrasePos()==0`, direction-agnostic).
- DELETE the fragile `globalWalk_`-offset reverse attempts (the `lenG-1`/`lenG-2` anchor
  patches): the correct relation falls out of `phrasePos()` because it is defined by the SAME
  traversal that produces the played column. Do not add a new reverse special-case; read the
  resolver.
- `pos_` is already advanced at `:495` with the same motion — it is live and correct; you are
  wiring a consumer to it, not changing how it advances.

## THE RULE (so it is not reinvented)
Forward enters at R and counts up; **reverse enters at R and counts down** — R at the O+L upper
bound (`endStep`). `phrasePos()==0` marks the phrase ENTRY in whichever direction it is played.
(This is the confirmed rule from `test_reverse_rotation_anchor` and the design doc.)

## INVARIANTS (the contract)
- **Goldens BIT-IDENTICAL**: `GOLDEN_A/B/BS` + `UA_NONDIV` unchanged. Forward `phrasePos()` ==
  `globalWalk_` bit-identically (the slice-1 identity harness `test_position_resolver_identity`
  already pins this — keep it green). Only the REVERSE path and the reverse-mapped reads change;
  **confirm no golden pins those** (state the confirmation in the PR), don't assume it.
- Full headless battery green (`bash test/run_all.sh`), including the newly registered
  `test_reverse_rotation_anchor` and the anchor-audit.

## NON-SCOPE (do NOT touch — these are later/separate slices)
- Migrating the other `stepIndex` consumers (output/sync/UI/Lantern) to `playedColumn()` — that
  is migration step 3, a separate slice.
- Deleting the redundant `globalWalk_`/`stepIndex` field — step 4, after it has no readers.
- The CADENCE slice (C1/C2/C4 — LOR/global-range latching) — migration step 6.
- COUNTER REMOVAL / position-holders and per-level direction composition (C3) — the
  `POSITION_HOLDER_MODEL.md` migration.
- The DRIVE DIRECTION MODEL (reverse-all, per-voice hold, variation-centre) — separate; see its
  own section/prompt.
Keep this slice to the derive migration + the reverse anchor. One landable, bit-identical step.

## DELIVERABLE
The derive wired to `pos_.phrasePos()` (+ `direction()` where the anchor needs the sign), the
fragile reverse patches deleted, `test_reverse_rotation_anchor` registered and green, the full
battery green, goldens bit-identical. In the PR: name every site touched and state the
no-golden-pins-reverse confirmation.
