# Codebase refactoring review — beyond Sands topology

Scope: a survey for inconsistency, duplicate definitions/owners, scattered logic
(manager/resolver candidates), code smell, and untested areas. Companion to
`SANDS_TOPOLOGY_RESOLVER_PLAN.md` (the ownership/lock work) — this covers everything else.

Every item is grounded in file:line evidence and rated **Impact** (bug-risk/maintenance)
× **Effort**. Ordered roughly by value. Nothing here is actioned; it's a menu to triage
after the Sands topology steps land.

General assessment: the codebase is **well structured** — a real manager/engine/ui
separation, a centralized lane-mapping home, 16 unit-test files for the engine layer, clean
delegation from `Monsoon` to 29 managers/controllers (not a god-object). The issues below
are mostly *consistency and drift-risk* in the few places where the same fact is restated
by hand, plus some dead weight and test gaps in the orchestration layer.

---

## A. Drift-risk duplication (highest value — same bug shape as the Sands clobber)

### A1. Strand→field mapping hand-written 6× in SequencerEngine.hpp  ·  Impact: HIGH · Effort: LOW
`strandLen/strandOff/strandRot` (readers, lines 98/109/120) and
`strandLenRef/strandOffRef/strandRotRef` (writers, 136/147/158) each contain a 6-case
`switch (strand)` mapping STRAND_* → the field. That's **36 hand-maps** that must stay
consistent across 6 functions. They already diverge cosmetically: the reader `default`
returns literal `16`, the writer `default` returns `rhythmLen` (SequencerEngine.hpp:107 vs
166). This is the *exact* "same fact restated by hand" pattern that made the
strandLen/strandLenRef confusion so hard to reason about during the Mono+Macro hunt.
- **Fix:** one array-of-pointers-to-members (or a single `int& field(strand, item)` that
  the len/off/rot helpers index into), so strand→field exists once. ~30 lines, removes 5
  of the 6 switches. The strand-write ledger (`setStrand`) already funnels writes — this
  finishes the job on the read side.
- **Test:** trivial round-trip test (`setStrand(role,s,a,b,c)` then `strandLen(s)==a` for
  all strands) — none exists today.

### A2. ID helpers (lenId/offId/rotId/attenId/cvId/ownerId/sendId) redefined per namespace with different strides  ·  Impact: HIGH · Effort: MED
Each visual surface defines its own `attenId/cvId/...` with a *different stride*:
- Mono (`MonsoonSandsVisualExpander.hpp`): `lane*3` (LEN/OFF/ROT triplets).
- East (`StraitsEastSandsVisual.hpp`): `lane*4`, plus a 3-arg `attenId(v,lane,col)`.
- Macro (`StraitsSandsMacroVisual.hpp`): `lane*4`.

Same names, different arithmetic, different namespaces. This is what forced the manual
build fix `2c24ca9` (bare `attenId`/`cvId` were ambiguous; had to qualify
`StraitsMacroVisualIds::`). It's a latent footgun every time a manager reaches across
surfaces.
- **Fix:** keep them per-namespace (the strides really do differ), but (a) NEVER expose
  them unqualified — require `East::cvId`, `Macro::cvId`; (b) add a short comment block in
  each namespace stating its stride and item order; (c) consider a tiny `LaneLayout`
  struct per surface that owns the stride so the arithmetic isn't copy-pasted. Lower
  urgency than A1 but high bug-history.

### A3. Dead duplicate: `spread/SpreadInterpolation.hpp` (166 lines)  ·  Impact: MED · Effort: TRIVIAL
`src/dsp/spread/SpreadInterpolation.hpp` is included by **nothing but itself**; the live
implementation is `src/dsp/SpreadInterp.hpp` (94 lines, included by VoiceResolver, both
Sands managers, SpreadManager, and the tests). Two files, similar names, one dead — a
classic "which one is real?" trap during a future spread edit.
- **Fix:** delete `src/dsp/spread/SpreadInterpolation.hpp` (and the `spread/` dir if it's
  then empty). Confirm with a final `grep -rl SpreadInterpolation` first.

---

## B. Dead weight / build hygiene

### B1. `src/deprecated/` — ~1,400 LOC of unbuilt code  ·  Impact: MED · Effort: LOW
Includes a 897-line `SandsVisualEditorV4_with_percell_editing.hpp` (a near-duplicate of the
live 1021-line editor), plus old Deep/Straits Sands, West visual, Peranakan panel, Surge
test. The Makefile globs only `src/*.cpp` + `src/dsp/{engines,managers,gates}/*.cpp`, so
`deprecated/` is **not compiled** — but it's indexed by greps/IDE and invites copy-paste
from stale code (the percell editor especially looks authoritative).
- **Fix:** move out of the tree (a `archive/` branch or tag, or delete — it's in git
  history regardless). At minimum add a top-of-file `// DEPRECATED — not built, do not
  copy` banner to each. The near-duplicate editor is the real hazard.

### B2. Mixed CRLF/LF line endings (7 files)  ·  Impact: LOW · Effort: TRIVIAL
CRLF in: `MonsoonInterchangeExpander.cpp`, `MonsoonWidget.hpp`,
`managers/MonsoonPersistenceManager.hpp`, `managers/MonsoonConfigurator.hpp`,
`managers/MonsoonScaleManager.{hpp,cpp}`, `deprecated/MonsoonSandsExpander.cpp`. Rest are
LF. Causes noisy diffs and the occasional tooling hiccup.
- **Fix:** add `.gitattributes` (`*.cpp text eol=lf`, `*.hpp text eol=lf`) and normalize
  once (`git add --renormalize .`). One commit, then it stays consistent.

### B3. Temp-debug commits still in history  ·  Impact: LOW · Effort: LOW
`c6b5f35`, `c7e2074`, `8410506` (and the probe commits from the Mono+Macro hunt, now
removed in content but present as history). Squash candidates before any release tag. Not
urgent; cosmetic.

---

## C. Resolver / manager candidates (scattered logic)

### C1. Spread logic spread across engine + managers + widgets  ·  Impact: MED · Effort: MED-HIGH
~56 call sites touch `spreadValue/getInterpolatedValue/effectiveTarget/spreadEffective/
monoContext` across `SpreadManager.hpp` (437 lines), both Sands managers, the widgets, and
`PatternEngine`. The pull-refactor this session (engine owns `spreadInterpMono`, managers
read it) was a step toward consolidation but only covered the *target mode*. The
per-lane/per-voice spread *value* resolution (owner → which spread knob + CV + send blend)
is still computed inline at several sites — and it's the same ownership question the
SandsTopology answers.
- **Fix:** once SandsTopology lands, fold the spread-value owner resolution into it
  (`topo.spreadSource(voice, lane) → {role, knobId, sendApplies}`), so the spread blend is
  computed one way. Natural **step 5b** of the topology plan (already noted there).
- This is the second-biggest "scattered logic" after ownership; do it *after* topology so
  it reuses the same authority rather than inventing a parallel one.

### C2. `polyBaseActive` defined twice, differently  ·  Impact: MED · Effort: TRIVIAL
- `MonsoonExpanderManager.cpp:28` → `(straitEast != nullptr) && (numPolyVoices >= 1)`
- `MonsoonSandsManager.cpp:45` → `(cachedPolyVoiceExpander != nullptr) && ...`
Same intent, two expressions (one via the raw `straitEast` alias, one via the cached
pointer). They happen to agree today, but it's the textbook "two definitions drift" setup.
- **Fix:** one accessor on `MonsoonExpanderManager` (`bool polyBaseActive(engine) const`)
  both call. Trivial, and it's exactly the input SandsTopology already consumes — so this
  can be absorbed when the topology is built once and passed down.

---

## D. Test gaps (orchestration layer is untested — where the bugs live)

### D1. No tests for any manager or the Sands orchestration  ·  Impact: HIGH · Effort: MED
16 test files exist, all on the **engine/pattern layer** (PatternEngine, GateState,
SpreadInterp, RNGs, voice_resolver, note-length, multistep, lock-behaviour, modes). There
is **zero** coverage of: `MonsoonExpanderManager::sync`, `MonsoonSandsManager::processDNA`,
ownership/owner-switch logic, or expander-combination behaviour. Every bug this session
lived in that untested orchestration layer.
- **Fix:** the SandsTopology unit test (`test_SandsTopology.cpp`, 26 checks, just added) is
  the first crack at this and the right model — it tests the *decision logic* in isolation
  with plain-data inputs. Extend that pattern: a `setStrand` ledger round-trip test (A1),
  and (harder, needs light fixtures) a combination-matrix test that asserts "exactly one
  writer per (voice,lane)" across all 8 configs. The topology makes this finally testable
  without a full Rack harness.

### D2. Editor edit-state not persisted (no dataToJson on the 3 Sands visuals)  ·  Impact: MED · Effort: MED
None of `MonsoonSandsVisualExpander`, `StraitsEastSandsVisual`, `StraitsSandsMacroVisual`
define `dataToJson`/`dataFromJson`. Param values persist via Rack's automatic param save,
but any **non-param editor state** does not. The V1 LOR edit-vs-display divergence we hit
suggests the editor carries state (edit window) distinct from the params; confirm whether
anything user-visible is lost on save/load (e.g. East V1 LOR in combo-3, flagged earlier as
a known gap). If edit-state == params everywhere, this is a non-issue; if not, it's silent
data loss on reload.
- **Fix:** audit what each editor holds beyond params; if anything, add JSON round-trip +
  a load-time topology check (plan §5, decision 5).

---

## E. Lower-priority smells

### E1. `% 16` / literal-16 step-count magic (~114 sites in engine+managers)  · Impact: LOW · Effort: LOW
A `STEP_COUNT` constant exists but `% 16`, `, 16)`, `16.f`, `< 16` are written raw ~114×
in `dsp/engines` + `dsp/managers`. Mostly harmless (16 is unlikely to change) but it's the
kind of thing that makes a future "variable pattern length" feature a large diff.
- **Fix:** opportunistic — replace with `STEP_COUNT` when touching a line anyway; not a
  dedicated pass.

### E2. Namespace split redDot:: (UI, 153 refs) vs dotModular:: (DSP, 137 refs)  · Impact: LOW · Effort: — 
Not a bug — it's a reasonable UI/DSP split (redDot = widgets/look, dotModular = lane maps /
voice resolver / topology). Noting it only so a future reader knows the split is
intentional. **No action**; if anything, document the convention in a top-level README.

### E3. `Monsoon.hpp` is 1,141 lines  · Impact: LOW · Effort: —
Large, but it delegates to 29 managers/controllers — the size is mostly declarations +
wiring, not logic concentration. **Not a god-object.** No action beyond watching that new
logic goes into a manager, not into `Monsoon` directly.

---

## Suggested order (after Sands topology steps 3–6)

1. **A3** (delete dead spread dup) + **B2** (.gitattributes) — minutes, pure hygiene.
2. **A1** (strand→field once) + its round-trip test — small, removes real drift risk,
   complements the strand-write ledger.
3. **C2** (single `polyBaseActive`) — absorb into the topology build.
4. **D1** (manager/combination tests) — build on `test_SandsTopology.cpp`; this is the
   highest-value durable investment.
5. **C1** (spread-value resolution into topology) — as topology step 5b.
6. **B1** (deprecated dir), **A2** (ID-helper hygiene), **D2** (persistence audit) — as
   convenient.
7. **E*** — opportunistic only.

None of these block the Sands topology work; several (C1, C2, D1) actively *reuse* it, so
they're best done after, not before.
