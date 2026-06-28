# Sands Topology Resolver — consolidation plan

Status: **plan, decisions settled (section 5), not actioned.** Step 1 (the single-writer
strand ledger) is done on branch `feature/sands-strand-write-ledger`. This document plans
steps 2+: collapsing the scattered ownership / lock / editable / write-guard predicates
behind one authority. The five design questions in section 5 are now decided.

Companion docs: `SANDS_ARCHITECTURE_CONSOLIDATION.md` (East↔Macro *mixer semantics* — the
why), this doc (the *decision-routing structure* — the how). They don't overlap.

---

## 1. The problem, stated precisely

There are four questions the Sands system asks over and over, every block, at many sites:

- **OWNS**(role, voice, lane) — who owns this lane's base value here?
- **EDITABLE**(panel, voice, lane) — may *this panel's* editor edit it?
- **LOCKED**(panel, voice, lane) — should *this panel's* control be locked/dimmed? (the
  display-lock matrix the inverse of EDITABLE, plus "no-target" lock states)
- **WRITES_ENGINE**(role, voice, lane) — may *this producer* write the engine strand/poly?

Today each is answered by an independent expression re-derived from raw flags
(`hasMacro`, `polyBaseActive`, `cachedSandsVisualExpander != nullptr`, `onMonoTab`,
`ownerDispId(l) > 0.5`, …). The survey (master, this session) found these **distinct,
hand-written answers to the same questions**:

| Site (file:fn) | Question | Expression |
|---|---|---|
| `MonsoonSandsVisualExpander` `laneLockedFn` | LOCKED (Mono panel) | `hasMacro && !(ownerDispId(l)>0.5)` |
| `MonsoonSandsVisualExpander` `laneDelegated` | WRITES (Mono→param) | `macroForOwn && el<4 && !(ownerDispId(el)>0.5)` |
| `StraitsSandsMacroVisual` `laneLockedFn` | LOCKED (Macro panel) | `viewVoice==0 && mono && mono.ownerDispId(l)>0.5` |
| `StraitsEastSandsVisual` `laneLockedFn` | LOCKED (East panel) | `tab1MonoMirror() ? all-locked : laneOwnedByMacro` |
| `StraitsEastSandsVisual` `laneOwnedByMacro` | OWNS (East poly) | `macroAttached && !(ownerDispId(lane)>0.5)` |
| `StraitsEastSandsVisual` `monoLaneOwnedByMacro` | OWNS (East V1) | `macroAttached && !(ownerDispId(lane)>0.5)` (mono variant) |
| `StraitsEastSandsVisual` `v1Editable` | EDITABLE (East V1) | `onMonoTab() && !mono` |
| `StraitsEastSandsVisual` `tab1MonoMirror` | (sub-predicate) | `onMonoTab() && mono` |
| `MonsoonSandsManager` `readStrand` owner switch | OWNS+WRITES (Mono V1) | `l<4 && hasMacro && macroVis && !(ownerDispId(l)>0.5)` |
| `MonsoonSandsManager` `monoOwnsLane` | OWNS (Mono V1) | `!(l<4 && hasMacro && macroVis && !(ownerDispId(l)>0.5))` |
| `MonsoonSandsManager` `monoOwnedByMacro` (no-mono) | OWNS (East V1) | per-lane lambda |
| `MonsoonExpanderManager` `combineLOR` `ownerEast` | OWNS (poly) | `eastLOR->params[ownerId(v,lane)]>0.5` |
| `MonsoonExpanderManager` Macro-only branch guard | WRITES (Macro V1) | `cachedMacroSandsVisual && polyBaseActive` ← **the bug: didn't exclude Mono** |
| `SandsMonoVisualIds::monoMacroOwnsEngineLane` | OWNS (engine-lane) | helper (bakes ENGINE→EDITOR) |
| `StraitsMacroVisualIds::macroSpreadModulatesLane` | (arc) | helper |

Every session-bug was one of these disagreeing with another:
- `readOnly = tab1Mono` (Macro whole-editor lock) vs the intended per-lane lock.
- Macro-only branch WRITES guard matching Mono+Macro (the V1 clobber).
- `laneDelegated` flickering vs `laneLockedFn` for the same lane.
- spread-target push: 3 sites, 1 wrong.

**They drift because they are independent restatements, not one fact read many times.**

---

## 2. The target: one authority, built once per block

A `SandsTopology` value object, constructed once in `Monsoon::process` from the cached
expander pointers + the ownership params, then passed (by const ref) to every consumer.
No widget or manager re-derives from raw flags again.

### 2a. Inputs (everything it needs to answer the four questions)

- Presence: `monoVisual`, `eastVisual`, `macroVisual`, `polyBaseExpander` (pointers).
- `polyVoiceCount` (`engine.numPolyVoices`).
- The ownership params (read once into a normalized table):
  - Mono V1 per-lane: `Mono::ownerDispId(editorLane)` for lanes 0..3.
  - East poly per-voice per-lane: `East::ownerId(v, lane)`.
  - East V1 per-lane: `East::ownerDispId(lane)` (+ `monoOwnerId`).

### 2b. The model (the "display/lock matrix" as data)

```cpp
struct SandsTopology {
    enum class Role : uint8_t { NONE, MONO, EAST, MACRO };

    // Which named CONFIGURATION are we in? Replaces ad-hoc flag combos. The
    // Macro-only-clobber bug existed because no code named MACRO_SOLE distinctly
    // from MONO_PLUS_MACRO — a hand-built (macro && polyBase) matched both.
    enum class Config : uint8_t {
        EMPTY,            // nothing
        MONO,             // Mono only
        EAST,             // East only (+ base)
        MACRO_SOLE,       // Macro only (+ base), no East visual, no Mono   ← clobber guard belongs here
        MONO_PLUS_MACRO,  // Mono + Macro (+ base)
        EAST_PLUS_MACRO,  // East + Macro (+ base)
        // (Mono+East and the 3-way are valid too — enumerate all REACHABLE ones)
    };
    Config config;

    // Per (voice, lane) ownership — the single source. voice 0 == V1/mono slot.
    // Resolved from the ownership params at construction, with the index
    // conventions (editor/engine) baked in so callers pass ONE convention.
    Role owner(int voice, int editorLane) const;

    // Derived answers — pure functions of `config` + `owner(...)`:
    bool editableOn(Role panel, int voice, int editorLane) const;
    bool lockedOn  (Role panel, int voice, int editorLane) const;  // == display lock
    bool writesEngine(Role producer, int voice, int editorLane) const;
};
```

Key property: `editableOn`, `lockedOn`, `writesEngine` are **pure derivations** from
`config` + `owner`. The three predicates that disagreed in the bugs become *the same
function*, so they cannot drift.

### 2c. The invariant it enforces (ties to step 1)

For every (voice, lane): **exactly one Role has `writesEngine == true`.** The
already-landed strand-write ledger (`SequencerEngine::setStrand` + the per-block
single-writer assert) is the runtime check of this invariant. The topology is the
*compile-time intent*; the ledger is the *runtime proof*. Together: the resolver says who
*should* write, the ledger asserts only that role *did*.

---

## 3. Why NOT a big-bang rewrite

Refactors are where this session's regressions bred (e.g. the spread pull-refactor was
correct, but `b386d37`'s "fix" introduced the clobber). So: incremental, one predicate at
a time, **with a build between each**, behaviour-inert until proven. The goal is that at
no single commit does behaviour change unexpectedly.

Discipline rule (the thing that makes it stick): **after migration, no Sands site may read
`cachedMacroSandsVisual` / `hasMacro` / `ownerDispId(...)` directly.** They go through the
topology. A grep for those tokens outside the topology builder becomes a lint.

---

## 4. Incremental steps (each its own branch off integration, build-verified)

**Step 1 — DONE.** Single-writer strand ledger (`setStrand` + assert). Branch
`feature/sands-strand-write-ledger`. This is the safety net that makes the rest safe to
attempt: if a migration step accidentally creates a second writer, the assert fires on
frame one.

**Step 2 — Introduce `SandsTopology`, populate, assert-only (no consumers yet).**
- Add the struct + a `buildSandsTopology(expanderManager, engine)` in one place
  (`Monsoon::process`, right after `beginStrandWriteBlock()`).
- Populate `config` and the `owner` table from the params.
- Behaviour-inert: nobody reads it yet. In debug, cross-check it against the *existing*
  predicates (e.g. `assert(topo.owner(0,l)==MACRO) == (mono.ownerDispId(l)<=0.5)`), so we
  prove the topology agrees with current behaviour before anything depends on it.
- Build. Confirm no asserts fire in all 6 reachable configs.

**Step 3 — Migrate the WRITE guards (highest bug density) one site at a time.**
Order, safest first:
1. `MonsoonExpanderManager` Macro-only branch → `if (topo.config == MACRO_SOLE)` (this is
   literally the clobber fix re-expressed as a named state — replaces
   `cachedMacroSandsVisual && polyBaseActive && !cachedSandsVisualExpander`).
2. `MonsoonSandsManager::readStrand` owner switch → `topo.owner(0, l)`.
3. `StraitsEastSandsVisual` V1 strand writes → gated by `topo.writesEngine(EAST, 0, l)`.
   Build + ledger-check (debug, all configs) after **each** of 1/2/3.

**Step 4 — Migrate the LOCK / EDITABLE predicates one widget at a time.**
- Mono `laneLockedFn` → `topo.lockedOn(MONO, 0, l)`.
- Macro `laneLockedFn` → `topo.lockedOn(MACRO, 0, l)`.
- East `laneLockedFn` + the trimpot `lockWhen` lambdas → `topo.lockedOn(EAST, v, l)`.
- Mono `laneDelegated` (editor→param skip) → `!topo.editableOn(MONO, 0, l)`.
  This is where `laneDelegated`/`laneLockedFn` STOP being able to disagree — same call.
  Build + manual UI check per widget.

**Step 5 — Migrate the remaining OWNS readers + retire the helpers.**
- `laneOwnedByMacro`, `monoLaneOwnedByMacro`, `monoOwnedByMacro`, `monoMacroOwnsEngineLane`
  → `topo.owner(...)`. Delete the now-unused helpers.
- `combineLOR`/`combineSpread` `ownerEast` → `topo.owner(v, lane) == EAST`.

**Step 5b — Extend single-writer coverage to the poly arrays + spread (per decision 4).**
- Add a `setPoly(role, voice, lane, item, value)` mirroring `setStrand`, with the ledger
  extended to (voice, lane, item) for `polyLen`/`polyOff`/`polyRot` and the spread arrays.
- Bring `combineLOR`/`combineSpread` (the main poly writers) + East's poly writes under it.
- Same build-between-each discipline. This is where a poly-side clobber (if any latent one
  exists) would surface.

**Step 6 — Lint + lock it in.**
- Add a CI/grep check: `cachedMacroSandsVisual|hasMacro|ownerDispId\(` must not appear
  outside `SandsTopology` construction. Document the rule at the top of each Sands file.
- Remove the debug cross-check asserts from Step 2 (topology is now the source).
- Keep the strand-write ledger permanently (debug-only).

---

## 5. Design decisions — SETTLED

1. **Index convention at the API: EDITOR LANE ONLY.** DECIDED. `SandsTopology` speaks
   editor lane at every public method; all editor<->engine<->PE-buffer<->mono-param
   conversions are baked *inside* the resolver (as `monoMacroOwnsEngineLane` already does).
   Callers never convert — this kills the off-by-one family at the boundary
   (`SANDS_LANE_INDEX_AUDIT.md`).

2. **Per-consumer construction, must stay lightweight.** DECIDED. Each consumer builds its
   own `SandsTopology` from the shared inputs (presence pointers + ownership params). No
   shared instance marshalled across threads. The single-source guarantee lives in the
   *one build function*, not one instance. Build cost is a presence check + a small
   ownership-param read — trivial. HARD REQUIREMENT: keep it allocation-free and cheap; if
   it ever grows heavy, revisit. (See rate note below for who builds when.)

3. **`Config` enumerates ALL reachable combinations.** DECIDED. Even where two combos
   behave identically today, list them separately — behaviour may need to diverge later,
   and naming all of them is what makes "one guard matches two configs" impossible.
   Collapse only inside the derivation functions, never in the guards.

4. **`writesEngine` + the ledger EXTEND to the poly arrays.** DECIDED. Not LOR/V1 only:
   ownership governs `polyLen`/`polyOff`/`polyRot` (per voice/lane) and spread too, so the
   single-writer invariant should cover them. Practical sequencing: land LOR/V1 first
   (steps 3–5), then extend the ledger + `writesEngine` to poly arrays + spread as step 5b
   — same pattern, one writer per (voice, lane, item). The `combineLOR`/`combineSpread`
   path in `MonsoonExpanderManager` is the main poly writer to bring under it.

5. **Patch-load correctness: verify on first block.** DECIDED. Topology is rebuilt each
   control block so it reflects loaded ownership params automatically; add an explicit
   debug check that `config` + `owner(...)` are correct on the FIRST control block after
   load, before any user interaction, so a wrong default can't masquerade as correct.

### Rate / cadence — SETTLED: control rate (with UI-rate widget builds)

The topology's inputs (expander presence, ownership params) NEVER change at audio rate —
only on patch/unpatch or an owner-cell click. So it is built at **control rate**, not
audio rate. Concretely:

- **Managers** build/consume inside the existing control-rate gate in `Monsoon::process`:
  `if (controlDivider.process())` (~1500 Hz; `controlDivider.setDivision(sampleRate/1500)`,
  Monsoon.cpp:43/762). `beginStrandWriteBlock()` + `buildSandsTopology(...)` go at the TOP
  of that block, before `processDNA` + `sync` (Monsoon.cpp:784/787).
- **Widgets** (`laneLockedFn`, `laneDelegated`, lock/dim lambdas) run on the UI thread at
  frame rate (~60 Hz). With per-consumer building (decision 2), each widget builds its own
  lightweight topology in `step()`/draw from the same inputs. 60 Hz x a few param reads is
  negligible.

Same code path, two cadences, no cross-thread snapshot to marshal. Audio-rate construction
would be pure waste — explicitly rejected.

---

## 6. What this buys, concretely

- The **exact** bug that cost this session (Macro-only guard matching Mono+Macro) becomes
  impossible to write: the guard is `config == MACRO_SOLE`, a named state that cannot also
  be `MONO_PLUS_MACRO`.
- `laneDelegated` vs `laneLockedFn` drift becomes impossible: same function.
- A new combination (or a 4th expander) is added in ONE place (`Config` + the derivation
  functions), not by editing a dozen lambdas.
- Diagnosis of any future ownership bug starts at `SandsTopology` — one place to probe,
  one place that's wrong, instead of an evening of cross-file greps.

---

## 7. Estimated risk / size

- Step 2: ~120 lines new, 0 behaviour change. Low risk.
- Steps 3–5: ~15–25 edited sites, each small, each build-verified. Medium risk, mitigated
  by the ledger (step 1) + the debug cross-check (step 2).
- Step 6: lint + cleanup. Low risk.

Do NOT attempt steps 3–5 in one sitting. One predicate, one build, one UI sanity check.
