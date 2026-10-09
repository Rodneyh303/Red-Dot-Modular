# CC BRIEF — V1 -> voice-0 unification (the keystone refactor)

## Start
Create a NEW branch off lane-expander-refactor. Do NOT work on the refactor branch directly.
    git checkout lane-expander-refactor && git pull
    git checkout -b v1-to-voice0
See also docs/design/V1_TO_VOICE0_UNIFICATION.md.

## Why (the justification — read this)
V1 is still structurally "Mono": it uses separate special-case machinery assuming the old Mono module /
Macro arbitration, instead of the poly path that works for V2-V16. This is ONE root cause with many
faces — all shipped as separate bugs this project already paid for:
- spread knob LOCK (laneOwnedByMacroTopo defaulted true for V1 w/o Macro -> knob swallowed input)
- spread application (combineSpread/applyMono didn't apply V1's own spread w/o Macro)
- QMIX mod-arc showing at rest
- the de-paramming crash
Every one was "V1 ownership/addressing defaults wrong without Macro." V2+ ALWAYS work because they use
the unified poly path (getMacroOwn, kFirstPoly loop, poly lock). Fix = make V1 behave as VOICE 0 of
that same path. This keeps generating bugs until collapsed; it is the keystone blocking effective
iteration pre-release.

## The guiding tell
For any broken V1 behaviour: find the WORKING V2+ equivalent and make V1 use the SAME logic at
voice-index 0 / the appropriate slot. e.g. getMacroOwn(v,lane) resolves correctly without Macro;
getMonoMacroOwn(lane) does not -> make V1 resolve the same way.

## Scope — collapse these V1/mono special-case sites onto the poly path (~150 refs)
- getMonoMacroOwn / setMonoMacroOwn (17/12) -> V1 ownership via poly getMacroOwn/setMacroOwn at V1 index.
- laneOwnedByMacroTopo V1 branch (8) -> V1 lock resolves like poly (false when !macroPresent).
- eastV1Owner (9) -> unify with eastPolyOwner at voice 0.
- kMonoSlot (35) -> where it is a SPECIAL-CASE BRANCH, route V1 through the normal slot path; where it is
  merely the slot-0 INDEX (correct), keep it.
- tab1MonoMirror / onMonoTab (13/25) -> treat the V1 tab as voice 0; remove mirror/special-tab logic.
- getMonoLaneDir (11) -> V1 direction via the poly direction path at voice 0.

## DO NOT blindly delete
applyMono (60) and kMonoSlot (35) include LEGITIMATE mechanism (slot 0 IS where V1's data lives — that
is correct), not just special-casing. Only collapse branches that treat V1 DIFFERENTLY from how a poly
voice at index 0 would be treated. TEST for each site: "if V1 were just voice 0, would this code still
be needed?" No (special-case) -> collapse. Yes (normal slot-0 handling) -> keep.

## Method — CRITICAL (big pervasive refactor; go carefully)
1. ONE symbol/site at a time. For each: find the working V2+ equivalent, route V1 through it, delete the
   V1-special branch, BUILD, run the test suite, confirm green.
2. BIT-COMPARE V1 against a working voice: after collapsing, V1 should be STRUCTURALLY IDENTICAL to a
   poly voice at index 0 — not just "the bug went away".
3. Keep genuine V1 semantics IF ANY (V1 as the correlation REFERENCE voice may legitimately differ in
   some contexts) — flag these, do not force-collapse; collapse only ownership/addressing/display
   special-casing.
4. Commit per symbol/milestone (SMALL commits). PUSH EACH (do not batch — pushes have been forgotten).
5. Do NOT add diagnostic logging. Diagnose from code. If you must trace, remove it before committing.
6. This is multi-session. Do NOT attempt one giant commit. Incremental is the only safe way.

## Verify throughout (the bugs this kills)
- East alone, NO Macro: V1 spread knob works, V1 bars respond, V1 NOT locked/dimmed.
- V1 behaves IDENTICALLY to V2 for ownership / spread / direction / display.
- QMIX mod-arc OFF at rest for V1.
- No regression for V2-V16, nor for Macro-attached cases.

## OUT OF SCOPE
Do NOT touch the Straits seamless-panel / VAR-LEG lanes / range-lane work. This branch is ONLY the
V1->voice-0 engine unification. Straits panel polish resumes after this merges back.
