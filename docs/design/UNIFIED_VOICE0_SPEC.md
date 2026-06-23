# Unified Voice Model — Mono as Voice 0 (Idea C)

**Status:** design spec only. No engine code written. This is step 1 of the staged
migration (spec on paper before touching the note-producing path).

**Context:** captured at the end of the control-inversion arc (branches
`feat/control-inversion` → `-panels` → `feat/voice1-mixin-blend`). Idea C is the
principled cleanup those changes point toward but deliberately did **not** depend on.

---

## 1. The problem C solves

The sequencer currently holds two parallel representations of the same conceptual
thing — "a voice's L/O/R/spread":

- **Mono master strand** — `engine.strandLen/Off/Rot(strand)` + the shared `*Random[]`
  arrays. Computed in `MonsoonSandsManager::processDNA`. Lane-permuted through
  `MONO_LANE_TO_STRAND`. No voice index.
- **Poly voices** — `engine.polyLen/Off/Rot[15][3]`, indexed by voice. Computed in
  `MonsoonExpanderManager` (`combineLOR`/`combineSpread`). Straight lane indexing.

Two producers, two arrays, two managers running in sequence. Almost every special case
in the Sands system traces back to this split:

| Symptom | Root cause |
|---|---|
| Interp. Y needs manual mix-in wiring in `readStrand` | mono path is separate from `combineLOR`, so it doesn't get the mix-ins for free |
| One-block `macroCVDelta` lag for voice 1 | two managers run in sequence; mono reads before Macro publishes |
| Tab-1 mono-mirror special-casing (display + lock on East & Macro) | voice 0 isn't really voice 0, so it needs bespoke display rules |
| Channel 1 reserved on the poly prob-outs | the seam showing through — ch1 *is* voice 0 conceptually |

C collapses the two into one: **the mono voice becomes voice 0 of a single unified
per-voice production pipeline.** "Mono" then means "voice 0 whose base is sourced from
the Mono module."

---

## 2. The hard truth: voice 0 is *mostly* but not *entirely* uniform

The trap in C is assuming voice 0 can be identical to voice N. It can't. The real
structural differences, confirmed against the code:

1. **Lane count.** Poly voices have **3 lanes** (REST/MEL/OCT — `polyLen[15][3]`). The
   mono strand has **6** (REST/MEL/OCT/LEG/ACC/VAR). Voice 0 carries LEG/ACC/VAR; voices
   1+ do not.
2. **Lane permutation.** Mono lanes map to engine strands through a **non-identity**
   permutation (`MONO_LANE_TO_STRAND`, `src/dsp/LaneMapping.hpp`):
   REST→RHYTHM, MELODY→VARIATION, OCTAVE→LEGATO, LEGATO→ACCENT, ACCENT→MELODY,
   VARIATION→OCTAVE. Poly voices index lanes straight. Unification must reconcile these —
   either the unified array is permuted for voice 0 only, or the permutation moves to the
   read site.
3. **Spread mode.** Voice 0 uses the ensemble-average `SpreadInterp` (converge toward the
   mono-inclusive average over active voices). Poly voices use per-voice spread. Voice 0's
   spread is genuinely a different computation, not just different inputs.
4. **Lock semantics.** Lock freezes the mono final arrays (`if (!engine.locked)` guards the
   spread rewrite). This is a voice-0/master behaviour.
5. **Base source.** Voice 0's base comes from the **Mono module** params; voices 1+ from
   East-local-or-Macro-global via the owner/opt-in selector.

**Design principle (the make-or-break):** voice 0's specialness must live in *what feeds
the pipeline* — a base-source selector, a lane-count, a spread-mode flag — **not** in
`if (v == 0)` branches scattered through the producer. If C ends up as `if (v==0)`
sprinkled through `combineLOR`, it has failed: it would relocate the special-casing into
the hot note-producing path rather than eliminate it. The shared ~80% (the L/O/R + CV-mix
+ blend pipeline) is genuinely shared; the mono-specific ~20% is clean parameterisation at
the edges.

---

## 3. The unified model

A single per-voice production pipeline producing `voice[i].{len,off,rot}[lane]` and
`voice[i].spread[lane]`, for `i` in `0..N`:

```
voice[i].value[lane][item] =
      base(i, lane, item)                         // source depends on i
    + eastCV(i, lane, item)  × eastDepth(i,…)      // uniform across all i (incl. 0)
    + macroDelta(lane, item) × macroSend(i, …)     // uniform across all i (incl. 0)
```

Where the **only** per-voice-0 divergence is in three parameterised inputs:

| Input | Voice 0 | Voices 1+ |
|---|---|---|
| `base(i, lane, item)` | Mono module params, via `MONO_LANE_TO_STRAND` | East-local OR Macro-global (opt-in latch) |
| lane count | 6 (REST/MEL/OCT/LEG/ACC/VAR) | 3 (REST/MEL/OCT) |
| spread mode | ensemble-average `SpreadInterp` | per-voice spread |
| lock | freezes voice-0 output | n/a |

The mix-in terms (`eastCV`, `macroDelta × send`) are **identical machinery** for every
voice — which is exactly what makes interp. Y fall out for free: voice 0 gets the mix-ins
because it runs the same pipeline, not because of bespoke code in `readStrand`.

---

## 4. What falls away when C lands

- **Interp. Y's manual mix-in** in `MonsoonSandsManager::readStrand` + the mono spread loop
  (the `eastMix`/`macroMix` lambdas) — subsumed by the unified pipeline.
- **The one-block `macroCVDelta` lag** — one producer, one ordering, no cross-manager
  sequencing.
- **The tab-1 mono-mirror display/lock special-cases** on East and Macro
  (`tab1MonoMirror()`, `readOnly`, hidden Macro attenuverters) — tab 1 becomes "the voice
  whose base is Mono," displayed by the same code as any voice. (Some display nuance may
  remain — e.g. base controls live on the Mono module — but the bespoke branches shrink.)
- The **two-array bookkeeping** (`strand*` vs `polyLen`) and the second manager pass.

---

## 5. Staged migration (when C is taken on)

C touches the **core note-producing read path** — the highest-stakes code in the plugin,
and the one place a subtle error shows as *wrong notes*, not a misplaced knob. So it is a
deliberate, isolated project with ear-verification, never a rider on other work.

**Stage 0 — this spec.** Lock the uniform-vs-voice-0-specific boundary (§2, §3) on paper.
Decide the permutation strategy (permute voice 0 in the unified array, vs permute at the
read site). Decide where the 6-vs-3 lane difference lives (voice 0 has 3 "extra" lanes
that only it populates).

**Stage 1 — introduce the unified array alongside the existing two.** Write BOTH the old
(`strand*` + `polyLen`) and the new unified array each block. Do **not** switch the read
path yet.

**Stage 2 — parallel-run validation.** Assert the unified array reproduces the existing
output **bit-exact** for voice 0 (vs `strand*`) and voices 1+ (vs `polyLen`), across:
solo mono; mono+East; mono+East+Macro; each owner/opt-in state; with/without CV on each
jack; reversible + plain modes; lock on/off. This harness is the safety net — you never
trust an unverified rewrite of the note path. Build it as standalone tests where possible
(the engine structs are already extractable — cf. the 347-test PatternEngine suite).

**Stage 3 — switch the sequencer read to the unified array.** Only once Stage 2 matches.
The sequencer reads one array; `strandLen/Off/Rot` become views onto `voice[0]` or are
retired.

**Stage 4 — delete the old paths.** Remove the `strand*` parallel store, the Y manual
blend, and the tab-1 mirror special-cases. They are now subsumed.

**Stage 5 — build + listen** at each prior stage, not just at the end. Ear-verification
matters as much as the test harness for this change.

---

## 5a. Feasibility test result — spread unification: **GO** (2026, explore branch)

The §6 spread-mode-as-parameter question — flagged as the make-or-break C feasibility
test — was checked on `feat/unified-voice0-explore` and **passes cleanly**. The two
spread computations are **already the same function**: both the mono path
(`MonsoonSandsManager` ~L186) and the poly path (`MonsoonExpanderManager` ~L128) call the
identical `redDot::SpreadInterp::apply(pe, mode, lane, step, nPoly, original, spreadAmount)`.

The only differences are arguments, all already parameterised:
- **source draw** `original`: mono → `pe.slewedRhythm[step]`; poly → `pe.slewedPolyRhythm[v][step]`.
- **target mode**: the `SpreadInterp::Target` enum (`AVERAGE_POLY` vs `MONO_DRAW`) is already a
  per-call parameter — the "ensemble-average vs per-voice" distinction is NOT two algorithms,
  it's one argument.
- **output array**: mono → `rhythmRandom[step]`; poly → `polyRhythmRandom[v][step]`.

Implication: C's highest-risk assumption is retired. The spread pipeline is already unified
behind `SpreadInterp`; voice 0 vs voices 1+ is "same `apply`, per-voice source + mode +
output", not a fork. This removes the strongest argument for keeping voice 0 separate.
**C is viable.** What remains genuinely structural (and still must be handled, not waved
away): the 6-vs-3 lane count, the `MONO_LANE_TO_STRAND` permutation, the base-source
selector, and the lock anchor (§2). None of those is a hot-path-branch problem; all are
edge parameterisation.

What did NOT happen on this branch, by design: no rewrite of the note-producing path. C
still requires the parallel-run bit-exact harness (Stage 2) before any read-path switch —
the absence of a Rack SDK in the working container means engine equivalence cannot be
proven here, and the spec's rule stands: unbuilt-and-unverified is not acceptable for the
note path. This branch is exploration/feasibility only.

## 6. Open decisions to settle before Stage 1

- **Permutation home:** does the unified array store voice 0 pre-permuted (so the read
  path is uniform), or store straight and permute at the mono read site? Leaning:
  permute at the edge (keep the array uniform), but verify the sequencer's strand reads
  (`getStrandIdx`, `strandLen(strand)`) don't assume the permuted layout elsewhere.
  *(Explore-branch finding: the sequencer has TWO distinct read sites — master reads
  `strandLen(strand)` via `MONO_LANE_TO_STRAND` (SequencerEngine ~L247), poly reads
  `polyLen[voice][polyLane]` straight (~L212). These two sites ARE the seam C unifies. The
  permutation is consumed only at the master read site, so "permute at the edge" is
  tractable: voice 0's read applies `MONO_LANE_TO_STRAND`, voices 1+ read straight, both
  off one array. The permutation does not leak elsewhere.)*
- **6-vs-3 lanes:** does the unified array allocate 6 lanes for all voices (wasting 3 on
  poly), or 6 for voice 0 and 3 for the rest (ragged)? Leaning: allocate 6 uniformly;
  poly voices simply never populate/read LEG/ACC/VAR. Simpler indexing, trivial memory
  cost (15×3 extra ints).
- **Spread mode as a parameter:** can the ensemble-average vs per-voice spread be expressed
  as a per-voice `spreadMode` enum fed to one spread function, rather than two functions?
  If yes, C is clean. If the two spread computations are too structurally different, that
  is the strongest argument that voice 0 stays partly separate — worth prototyping this
  one piece first as the C feasibility test.
- **Base-source selector:** voice 0 → Mono params; voices 1+ → existing owner/opt-in.
  Expressed as a `baseSource(i)` function feeding `combineLOR`, no branch in the body.

---

## 7. Recommendation

C is worth doing — it retroactively simplifies the entire control-inversion + Y work and
removes the last structural seam (mono-as-not-quite-a-voice). But it has **no urgency**:
everything built in the inversion arc stands on its own without it. Take C on with a fresh
head, the spread-mode feasibility prototype (§6) first as a go/no-go, and the parallel-run
harness (Stage 2) as a non-negotiable safety net before switching the read path. It is the
one change where "unbuilt and unverified" is not acceptable — the note path must be proven
equivalent before the old path is removed.
