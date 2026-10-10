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

---

## CORRECTION (Rodney) — Phase A is NOT done; the real debt is the MONO-STRAND DATA MODEL
Commit 5d170115 reconciled the index CONSTANTS (kMonoMacroOwnRow/kMonoLaneDirRow 15 -> 0) — legit but
SMALL (19 lines). The commit message "V1 = index 0 everywhere" OVERCLAIMS: the counts barely moved
(kMonoSlot 36, onMonoTab 17, tab1MonoMirror 9, eastV1Owner 9, getMonoMacroOwn 12). The actual V1-is-Mono
debt — the separate MONO-STRAND DATA MODEL — is UNTOUCHED.

### The real structural problem
The storage still separates "4 MONO strands (V1) + 15 poly", NOT 16 poly voices:
- PatternEngine.hpp:593 comment: "4 mono + 15 poly" (rhythm+variation+legato+accent as MONO + 15 poly).
- PatternEngine.hpp:51: a separate `float legato` MONO scalar.
- SequencerEngine.cpp:747: V1 legato read via `monoStrand(STRAND_LEGATO)` — a separate mono strand.
- Monsoon.hpp:174: "voice 1 (mono) variation lives on Monsoon's knob" — V1 variation on the MONO knob.
- PatternEngine.cpp:38-39: poly seeded to "match mono default" — implies mono (V1) is still the reference.
So V1's rhythm/variation/legato/accent live in SEPARATE MONO STRANDS + the Monsoon mono knob, NOT as
voice 0 of the poly arrays. THAT is the debt (legato/variation are still V1-mono-special — Rodney flagged
they should NOT be).

### What Phase A MUST actually do (the structural collapse)
VARIATION and LEGATO are now POLY (POLY_VARIATION_PARAM_1..15 exist). So V1 must be VOICE 0 of those poly
arrays, NOT a separate mono strand/knob. Collapse:
- The "4 mono + 15 poly" storage split -> 16 POLY voices, V1 = index 0. No separate mono strands for
  rhythm/variation/legato/accent.
- `monoStrand(...)` reads for V1 -> read voice 0 of the poly array (polyRandom(0, PL_*)).
- The separate `legato` mono scalar (PatternEngine.hpp:51) -> gone; V1 legato = voice 0 of the poly
  legato array.
- "V1 variation on the Monsoon knob" -> route V1 variation through POLY_VARIATION_PARAM at voice 0.
- The "matches mono default" seeding -> V1 is just voice 0; no separate mono default/reference.
Then kMonoSlot/onMonoTab/tab1MonoMirror/eastV1Owner/getMonoMacroOwn/getMonoLaneDir/laneOwnedByMacroTopo
collapse naturally (they exist to serve the mono-strand model).

### Commit-message discipline
"V1 = index 0 everywhere" must mean the DATA MODEL is unified (mono strands collapsed into poly voice 0),
not just that index constants changed. Do not mark Phase A done until the mono strands are gone and the
counts above are near zero. Bit-compare V1 to a working poly voice — same data path, not a parallel mono one.
