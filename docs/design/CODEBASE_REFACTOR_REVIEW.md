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

### B0. Genuinely dead code in the BUILT tree (verified)  ·  Impact: MED · Effort: TRIVIAL
(`deprecated/` excluded by design — this is dead code in *compiled* paths.)
- **`src/ui/SpreadControlWidget.hpp` (220 lines)** — the `SpreadControlWidget` class is
  referenced nowhere in src or test. Fully dead. Delete.
- **`src/dsp/spread/SpreadInterpolation.hpp` (166 lines)** — included by nothing (dead dup
  of the live `SpreadInterp.hpp`); see A3. Delete (+ the `spread/` dir if then empty).
- **`StraitsWestSandsVisual` — a half-wired ghost.** The model registration is commented
  out (`Monsoon.cpp:937 // p->addModel(modelStraitsWestSandsVisual)`) and the expander-scan
  that would populate `cachedWestSandsVisual` is commented out
  (`MonsoonExpanderManager.hpp:136-137`), so `cachedWestSandsVisual` is **always null**.
  Yet `MonsoonExpanderManager.hpp:33` still `extern`-declares the model and line 164 ANDs
  the pointer into `allTypesFound()` — a predicate that therefore **can never return true**.
  Contained because `allTypesFound()` is itself never called (also dead), but it's a
  landmine: any future code gated on it silently never runs. Either revive West (real
  feature) or strip the forward-decl/extern/cached pointer/predicate term. Decide which;
  don't leave it half-present.
- **Commented-out Deep/Dna expander members** (`MonsoonExpanderManager.hpp:44,49-50,72,
  77-78,113-131`) — abandoned-idea scaffolding in a live header. If these are "maybe
  someday" like `deprecated/`, fine; if not, strip. Low urgency.
- **9 commented-out `#include` lines** + ~37 commented-code lines in
  `MonsoonExpanderManager.cpp` — mostly old expander-scan variants. Cosmetic; clean
  opportunistically.

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

## F. Checks worth running that aren't on your list (categories, not yet exhaustive)

These are *kinds* of audit beyond consistency/dead-code. A few are spot-checked below; each
deserves its own focused pass.

### F1. Type-safety of expander casts  ·  Impact: HIGH (if it ever fires) · Effort: MED
12 `reinterpret_cast`s on expander pointers in the built tree (e.g. the scan in
`MonsoonExpanderManager`). They trust that a `module->model == modelX` check guarantees the
concrete type. That's the Rack idiom, but `reinterpret_cast` (vs `static_cast` /
`dynamic_cast`) means a single wrong model-guard is undefined behaviour, not a null. Worth
auditing that every cast is immediately preceded by the matching `model ==` guard, and
considering a one-line typed helper `as<T>(Module*, Model*)` that bundles guard+cast so the
pattern can't be written wrong.

### F2. Real-time safety (no allocation/lock/IO on the audio thread)  ·  Impact: HIGH · Effort: MED
Spot-check came back clean — no `std::string`/`vector`/`new` in the Sands `process`/`sync`
paths (good; the control-rate work is allocation-free). But this should be a *deliberate*
audit across all `process()` paths: no heap alloc, no mutex, no file/JSON, no logging in
the hot path (the debug `WARN` probes we used are fine only because they're temporary).
A known-good check to institutionalize given how much logging we added this session.

### F3. Thread-safety of UI↔audio shared state  ·  Impact: HIGH · Effort: HIGH
The whole Sands ownership story is params written on the UI thread, read on the audio
thread (and engine strands written audio-side, read UI-side for display). Rack params are
designed for this, but the *derived* shared state (engine `melodyLen` etc. read by widgets
for display) is read without synchronization. Mostly benign for display (a torn read shows
one stale frame), but worth a conscious pass: list every field written on one thread and
read on the other, confirm each tolerates a stale/torn read. The topology resolver is the
place to document these crossings.

### F4. Determinism / RNG seeding  ·  Impact: MED · Effort: LOW
There are PhiloxRng + SquaresRng + RNG tests — good. Worth confirming: is pattern output
fully reproducible from (seed, step) across save/load and sample-rate changes? A
"same seed → same pattern after reload" test would catch a class of subtle bugs and is
cheap given the RNG layer is already tested.

### F5. Sample-rate independence  ·  Impact: MED · Effort: LOW
`controlDivider.setDivision(sampleRate/1500)` — confirm all slew/timing constants are
expressed in seconds (not samples), so behaviour is identical at 44.1k/48k/96k/192k. Spot
audit of `slew`, decay, and any `*= 0.99`-style smoothing for hardcoded per-sample rates.

### F6. Denormal / NaN protection in DSP  ·  Impact: MED · Effort: LOW
Feedback/smoothing paths (spread slew, any IIR) can drift to denormals (CPU spike) or NaN
(silent stuck state). Check whether the build sets FTZ/DAZ or flushes small values; add a
NaN guard on output if not. The `-ffast-math` in the user's build flags makes NaN handling
*less* predictable, so worth a look.

### F7. Param config completeness  ·  Impact: LOW · Effort: LOW
Every `configParam`/`configSwitch` should have sane min/max/default + a name + (ideally)
unit/description for the UI. A quick audit that no param is unnamed or has a default
outside its range (the kind of thing that makes a freshly-placed module behave oddly).

### F8. Const-correctness pass  ·  Impact: LOW · Effort: MED
The build-fix `2c24ca9` had to *drop* `const` because Rack's accessors aren't const-correct
in this SDK. That's a known constraint, but it means "takes non-const Module*" has spread.
Worth confirming non-const is only where Rack forces it, not by accident — so genuinely
read-only helpers advertise it.

### F9. Header include hygiene / build time  ·  Impact: LOW · Effort: MED
`Monsoon.hpp` (1141 lines) is included widely; heavy headers included transitively inflate
build time (the user rebuilds often on MSYS2). An include-what-you-use pass + forward
declarations where possible would speed iteration. Measure first (which headers are most
transitively included) before investing.

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
