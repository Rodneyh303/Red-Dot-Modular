# CC BRIEF v2 — V1 -> voice-0 unification (continue on feat/v1-to-voice0)

Steps 1-7 DONE (ownership resolution + direction unified, dead mono code removed, ~-128 lines).
Full spec + decisions: docs/design/V1_TO_VOICE0_UNIFICATION.md. This file is the working brief.

## CRITICAL — the current layout is INCONSISTENT; reconcile it
V1/mono is split across TWO indices:
- VoiceResolver.hpp: kMonoVoice=1, kFirstPoly=2, kMonoSlot=0  (mono DATA slice at slot 0)
- Monsoon.hpp:934: kMonoMacroOwnRow=15                        (V1 OWNERSHIP at row 15)
So V1 data is at slot 0 but V1 ownership is at row 15 — inconsistent, the root of the scattered
special-casing. TARGET (Rodney's decision): V1 = INDEX 0 everywhere, uniform, no special rows.
DISCARD old patches — NO backward-compat / migration code.

## PHASE A — reconcile + collapse to index 0 (DO NOW, carefully, one at a time)
1. Unify OWNERSHIP index with DATA index: kMonoMacroOwnRow=15 -> V1 ownership at the SAME index as V1
   data (voice 0). Collapse the row-15 ownership into the voice-0 row of macroOwn. Kills the slot-0 vs
   row-15 split.
2. kMonoSlot (35): collapse SPECIAL-CASE branches to the uniform voice path; where it is merely the
   index-0 slice (legit), keep but read it as "voice 0" not "special mono slot". Test each: "if V1 were
   just voice 0, would this code still be needed?" No -> collapse. Yes -> keep.
3. Collapse onto the poly path at voice 0 and DELETE the V1-special branch:
   onMonoTab (22), tab1MonoMirror (9), eastV1Owner (9), getMonoMacroOwn/setMonoMacroOwn (12/9),
   getMonoLaneDir (6), laneOwnedByMacroTopo (8).
4. Voice numbering: kMonoVoice=1 / kFirstPoly=2 (1-based, "channel-1-reserved"). Make the INTERNAL index
   0-based with V1 = index 0; keep the USER-FACING label "V1". Do not let the 1-based internal convention
   keep V1 special.

## METHOD (CRITICAL — unchanged)
- ONE symbol/area at a time. For each: find the working V2+ equivalent, route V1 through it at voice 0,
  delete the V1-special branch, BUILD, run test suite, confirm GREEN, COMMIT, PUSH. Small commits.
  PUSH EVERY ONE (do not batch; pushes have been forgotten before).
- NO diagnostic logging. Diagnose from code. Remove any trace before committing.
- BIT-COMPARE V1 against a working poly voice after each collapse: bar is "V1 structurally identical to
  voice 0", not "the bug went away".
- Keep genuine V1-as-correlation-REFERENCE semantics if any (flag, don't force-collapse).
- Multi-session; incremental only; never one giant commit.

## PHASE B — LATER (separate; only after Phase A merges clean)
Active-voices-only optimisation + CA OOB policy, per the design doc:
- Compute only N active voices (no Straits -> N=1; else N = poly count). ~16/N x saving.
- Poly-count change at STEP boundary; knob-POLLED, NEVER modulatable (not CV, not DAW). No pending UI.
- Existing channels never change on add/reduce — only the delta voices spin up/down; counter-addressed
  regen keeps determinism (activate-at-step-30 == active-since-1); pending dice irrelevant.
- CA (the SQUARE matrix over 8 CV pairs: 3 rhythm/3 melody/2 qmix in-out): any index >= N (above poly
  count) -> revert to IDENTITY map (voice<->self). Square => source & target OOB together; OOB=identity
  uniformly for BOTH consumers (random correlation structure AND CV remap). Reversible. UI: dim BOTH axes
  (not just target) when out of range.
DO NOT start Phase B until Phase A (index-0 migration) is complete and verified.

## OUT OF SCOPE
Straits seamless-panel / VAR-LEG / range-lane — not this branch.

## VERIFY THROUGHOUT
East alone, no Macro: V1 spread/owner/direction/display IDENTICAL to V2; V1 not locked; QMIX arc off at
rest; no V2-V16 or Macro-attached regression; test suite green.
