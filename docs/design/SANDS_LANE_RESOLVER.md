# Sands — do we need a Lane Resolver (like VoiceResolver)?

Status: **analysis + recommendation, not yet actioned.** Prompted by the
multi-day lane-reorder regression, which was entirely *bridge* bugs between
lane coordinate systems.

## FIRST, THE BETTER QUESTION: why are there four orders at all?

Traced each of the four. **Only one is a real requirement; the other three are
arbitrary historical declaration orders.**

| System | Order | Is the order meaningful? |
|--------|-------|--------------------------|
| EDITOR lane | MEL, OCT, REST, ACC, VAR, LEG | **YES** — deliberate product/visual choice (top-to-bottom on the panel). |
| MONO param bank | REST, MEL, OCT, LEG, ACC, VAR | No — just the order `enum ParamId { LEN_REST=0, … }` happened to be declared. `lenId(l)=LEN_REST+l*3` only needs each lane's 3 params contiguous; the *order* is incidental. |
| ENGINE strand | RHYTHM, VAR, LEG, ACC, MEL, OCT | No — strands are separate NAMED fields (`rhythmLen`, `melodyRandom[]`…) reached by `switch(strand)`. Nothing indexes them in order; the enum numbering is cosmetic. The sequencer assembles notes by *name*, not by lane index. |
| POLY engine lane | REST, MEL, OCT, ACC | No — East/Macro `lorId`/`polyLen` declaration order; `PROB_OUT_REST=0` etc. arbitrary. Notably a *different* arbitrary order from the mono bank (REST-first vs strand RHYTHM-first), for no reason. |

Nothing musical or structural depends on the specific numbering of the three
engine-side orders. They diverged because three enums were declared at three
times, each picking a convenient order, and permutation tables were then bolted
on to bridge them. **The domain has one lane order (what the user sees); the
other three are accidents.**

### Implication: prefer ELIMINATING orders over making them safe to convert

The typed-index shim (below) makes the four orders *safe to convert between*.
But the stronger fix is to **renumber the enums so all three engine-side orders
match the editor order** — then there is nothing to convert and no bridge to get
wrong. The bug class disappears rather than being made type-safe.

- Sands is **pre-release** (param slots free to shift — no patch-compat
  concern for these modules), so renumbering `ParamId` / poly `lorId` / the
  `EngineStrand` enum to editor order is permitted.
- Caveat: the `EngineStrand` enum is also used by the **core Monsoon** module
  (rhythm/melody/etc are Monsoon's own strands), so renumbering it may ripple
  beyond Sands — needs a check that Monsoon doesn't assume specific values.
  The mono PARAM bank and POLY lane orders are Sands-local and safe to renumber.

**Realistic target:** collapse to **two** orders — EDITOR (everything UI/param/
poly side adopts it) and ENGINE strand (left as-is if Monsoon depends on it,
bridged by the ONE `MONO_LANE_TO_STRAND` table which is genuinely needed because
the engine is shared). That removes `MONO_PARAM_TO_EDITOR`, `EDITOR_TO_MONO_
PARAM`, `ENGINE_LANE_TO_EDITOR`, and the poly/mono bank divergence entirely —
i.e. removes the three tables that caused every regression this week, keeping
only the one (editor↔strand) that bridges to shared engine code.

## If we can't fully collapse: the shim is VoiceResolver-shaped

(Original analysis retained below — applies to whatever orders genuinely
remain after the collapse above.)



There are **four distinct lane coordinate systems** in the Sands suite:

| # | System | Order | Used by |
|---|--------|-------|---------|
| 1 | **EDITOR lane** (what the user sees/clicks) | MEL, OCT, REST, ACC, VAR, LEG | SandsVisualEditorV4, currentState.lanes[] |
| 2 | **MONO PARAM bank** | REST, MEL, OCT, LEG, ACC, VAR | lenId/offId/rotId, cvId, attenId, sprId |
| 3 | **ENGINE strand** | RHYTHM, VAR, LEG, ACC, MEL, OCT | strandLen/Off/Rot, slewedMelody/… |
| 4 | **POLY engine lane** | REST, MEL, OCT, ACC | East/Macro lorId, polyLen[v][lane], macroBase[lane], VoiceResolver lane arg |

Conversions between them happen at **~27 inline sites** plus several
hand-rolled permutation arrays (`laneMap[6]` in MonoSandsParameterManager,
historically `engToEd`/`monoLaneToEditor`/etc before consolidation). Every
regression in the reorder work was a *bridge* bug:
- `readStrand` used an editor index to read param-bank params,
- the Mono gen placed param-order rows under editor-order labels,
- the spread map `SPR_TO_EDITOR` had REST-spread on the MELODY row,
- a duplicate `EDITOR_TO_MONO_PARAM` with a wrong value slipped in via merge.

This is exactly the topology VoiceResolver was built to tame for voices:
multiple parallel addressing schemes, conversions scattered across callers,
bugs breeding in the bridges. So **yes, a shim is warranted.**

## But it should be a TYPED INDEX, not a runtime resolver

Important difference from VoiceResolver:

- **VoiceResolver resolves a *runtime* quantity** — which engine voice a UI tab
  maps to depends on how many poly voices are active. It must be an object with
  state.
- **Lane mappings are *static* permutations** — pure compile-time constants
  (already in LaneMapping.hpp). They were never wrong as *data*; the bugs came
  from callers holding a bare `int` that could mean any of the four systems and
  indexing the wrong table with it.

So the fix is not a resolver object. It's making the four systems **distinct
types** so the compiler rejects cross-system indexing. A bare `int lane` is the
disease; four interconvertible-but-distinct index types is the cure.

### Sketch (strong-typedef approach)

```cpp
namespace dotModular {
  enum class EditorLane : int {};   // 0..5, user/editor order
  enum class MonoParam  : int {};   // 0..5, lenId/cvId/attenId order
  enum class Strand     : int {};   // 0..5, engine strand order
  enum class PolyLane   : int {};   // 0..3, East/Macro/poly order

  // The ONLY conversions, total functions, all constexpr, all unit-tested:
  constexpr MonoParam  toParam (EditorLane e);   // EDITOR_TO_MONO_PARAM
  constexpr EditorLane toEditor(MonoParam  p);   // MONO_PARAM_TO_EDITOR
  constexpr Strand     toStrand(EditorLane e);   // MONO_LANE_TO_STRAND
  constexpr EditorLane toEditor(PolyLane   l);   // ENGINE_LANE_TO_EDITOR
  // … the closed set; no others exist.
}
```

Call sites then read like `params[lenId(toParam(lane))]` where `lane` is an
`EditorLane` — and `params[lenId(lane)]` (raw editor index) becomes a **compile
error**, because `lenId` takes a `MonoParam`. The class of bug we hit all week
literally cannot compile.

### Why this beats the current "named constants" state

We already centralised the *data* into LaneMapping.hpp — that was necessary but
insufficient. `MONO_PARAM_TO_EDITOR[l]` and `EDITOR_TO_MONO_PARAM[l]` are both
`int[6]`; nothing stops a caller indexing the wrong one with the wrong-system
`int`, which is exactly the duplicate-with-wrong-value and readStrand bugs. The
type wrapper turns "remember which int means which order" (human discipline,
which failed repeatedly) into "the compiler checks it" (mechanical).

## Cost / risk

- **Mechanical but wide:** ~27 conversion sites + the id-accessor signatures
  (lenId/cvId/attenId/lorId/strandLen…) would take typed args. Touches East,
  Macro, Mono, the managers, the engine accessor boundary.
- **Boundary friction:** Rack params are `int`-indexed, so the typed index
  unwraps to `int` at the param[]/engine boundary via an explicit `.raw()` or
  `static_cast` — a deliberate, greppable seam.
- **Best done when the suite is OTHERWISE stable** — not mid-regression. Doing
  it now risks compounding churn; doing it right after the current build is
  confirmed good would *lock in* that good state against future reorders.

## Recommendation

1. **First: get the current build verified working** (Mono editing restored,
   East/Macro confirmed). Do NOT start the typed-index refactor until then.
2. **Then: introduce the typed lane indices** as the lane analogue of
   VoiceResolver — but as strong typedefs + a closed set of constexpr
   conversions, not a runtime object. This is the structural fix that ends the
   bridge-bug class for good.
3. **Migrate incrementally**, one coordinate boundary at a time (e.g. editor↔
   param first, since that caused the worst regressions), keeping each step
   build-verified — the same discipline VoiceResolver used ("behaviour-inert,
   bit-identical until callers migrate").
4. **Add the unit test** the constants currently lack: assert every conversion
   pair is a proper inverse and that round-trips are identity (the duplicate
   wrong-value constant would have been caught instantly by this).

Interim, low-cost win regardless of the bigger refactor: **replace the last
hand-rolled permutation arrays** (e.g. `laneMap[6]` in
MonoSandsParameterManager::syncPatternEngineToEditor — currently a correct but
duplicate STRAND→EDITOR table) with a named `STRAND_TO_EDITOR` constant in
LaneMapping.hpp, so all four systems' conversions live in exactly one file.
