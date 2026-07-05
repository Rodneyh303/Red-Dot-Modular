# Probability-modifier model — unifying LOR + Spread, mono + poly

Status: **design, not actioned.** Written because the "A1 strand→field table" cleanup surfaced that
the thing being tidied is one concept fragmented into ≥4 storage conventions by development history
("too much has diverged"). This doc fixes the *concept* first so the first code step moves toward one
object instead of hardening the accidental seams. Supersedes the narrow A1 framing in
`CODEBASE_REFACTOR_REVIEW.md` and folds in `SANDS_TOPOLOGY_RESOLVER_PLAN.md` step 5b (spread).

## 1. The true model (one object)

There are **four probability modifiers** — Length, Offset, Rotation, Spread — applied per **(lane,
voice)**. LOR index into the 16-step probability vector; Spread interpolates/reshapes it. All four are
the same *kind* of thing: **probability modifiers**. They are engine state (the engine consumes them to
produce per-step probabilities), regardless of where the current code happens to store them.

- **Lanes:** REST, MELODY, OCTAVE, ACCENT have all four modifiers. VARIATION and LEGATO are
  **mono-only and have LOR but no Spread** — the *only* real asymmetry in the model.
- **Voices:** voice 0 = mono/V1; voices 1..15 = poly. Mono is not a separate thing — it is voice 0.

That's it. `modifier ∈ {Len, Off, Rot, Spread}` × `lane ∈ {Rest, Mel, Oct, Acc (+Var, Leg for LOR)}` ×
`voice ∈ {0..15}`. Everything below is the same object seen through a development-history seam.

## 2. What actually exists today (the fragmentation)

| Slice | Storage today | File |
|---|---|---|
| Mono LOR | 18 **named int fields** (`rhythmLen`…`octaveRot`) + **6 hand-written switches** (`strandLen/Off/Rot` + `…Ref`) | `SequencerEngine.hpp:117-215` |
| Poly LOR | `polyLen[15][PL_LANES]`, `polyOff`, `polyRot` — **already the clean array model** | `SequencerEngine.hpp:134-136` |
| Spread (both) | `spreadEffective[]` **on the visuals, not the engine**, pushed into PE `*Random[]` | `StraitsSandsMacroVisual.hpp:221`, `MonsoonSandsVisualExpander.hpp:116` |
| Prob output | `rhythmRandom[16]`…`octaveRandom[16]` + **another 6-way switch**; poly `polyRhythmRandom[voice][idx]`… | `PatternEngine.hpp:72-92` |

Three symptoms of the drift, each a concrete bug-or-near-bug:

1. **Mono LOR ≠ poly LOR shape** for no reason. Poly is `[voice][lane]`; mono is 18 named fields. Mono
   is just voice 0 — it should be the same array.
2. **`spreadEffective` size mismatch: `[4]` on Macro vs `[6]` on Mono.** Same conceptual array, two
   sizes, two modules. The `[6]` carries VAR/LEG slots that are always 0 (VAR/LEG have no spread). This
   is exactly the "same fact, divergent copies" hazard — a live instance, not hypothetical.
3. **The 6-way `switch(strand)` is written FOUR times** (2 reader + ... actually 6: `strandLen/Off/Rot`,
   3 `…Ref`) in the engine, then AGAIN in PE for the `*Random[]` read. 36+ hand-maps that must stay
   consistent.

**The enum renumber already started this and stopped.** `EngineStrand` was renumbered to editor/lane
order (MELODY=0, OCTAVE=1, RHYTHM=2, ACCENT=3, VAR=4, LEG=5) so "editor lane == strand index" is now the
identity (`MONO_LANE_TO_STRAND` is identity). So **strand vs lane is already one concept** at the index
level; the storage just never caught up. There is no semantic "strand"; there is a lane, and 2 of the 6
lanes (VAR/LEG) happen to lack spread.

## 3. Target

One storage model on the **engine** (where modifiers semantically belong):

```
// lane ∈ 0..5 (MEL,OCT,REST,ACC,VAR,LEG — editor/strand order, now identical)
// voice ∈ 0..15 (0 = mono/V1)
int   len[16][6];    // was: mono named fields + polyLen[15][4]  → voice 0 + voices 1..15, all 6 lanes
int   off[16][6];
int   rot[16][6];
float spread[16][6]; // VAR/LEG columns (4,5) unused/always 0 — the one asymmetry, made explicit
```

- Mono = row 0. Poly = rows 1..15. No named fields, no `spreadEffective` on the visual — the visual
  *computes* a spread value and writes it to `spread[voice][lane]` through the single-writer path (the
  same discipline as the strand ledger). VAR/LEG spread columns exist but are never written (documented
  invariant), so the model stays rectangular without a special case at every access.
- All access via `int& len(voice, lane)` etc. — **one** accessor per modifier, no switches anywhere.
  The PE `*Random[]` read collapses the same way (or the arrays themselves move to `[voice][lane]`).

Property: the strand→field mapping, the mono/poly split, and the spread-on-visual detour **all
disappear** because there is one object. This is the D4 ("one resolver per concept") pattern applied to
storage, and it's what the presence-authority work did for topology.

## 4. Why NOT big-bang (the risk)

This touches the **hot per-sample PE path** and every LOR/spread read+write. A wrong move here is a
silent probability-corruption bug that no brace-check catches. So: incremental, behaviour-inert per
step, a build + the (new) round-trip test between each. The strand-write ledger is the runtime proof
that single-writer holds through the move.

## 5. Incremental path (each its own branch, build-verified)

**Step 0 — round-trip test FIRST (safety net).** Before moving any storage: a test that sets each
(voice, lane, item) and reads it back through the current accessors, for all lanes incl. VAR/LEG and
voices 0 + a poly voice. This is the regression oracle for every step below. *(review A1 asked for this
for mono LOR; widen it to the full model.)*

**Step 1 — collapse the 6 mono-LOR switches to ONE table, API-preserving (the re-scoped A1).**
Keep `strandLen/Off/Rot(+Ref)` signatures (zero call-site churn), but back them with a single
`int& field(lane, item)` over a pointer-to-member (or, better, start migrating the named fields to a
`monoLOR[6][3]` array so this step already moves toward §3 rather than cementing named fields). Decide:
pointer-to-member table (smaller diff, keeps named fields) vs. array (bigger diff, real progress). Lean:
**array**, since named-field storage is part of what diverged — but gate on the round-trip test.

**Step 2 — unify mono LOR into the poly array as voice 0.** Make mono LOR `len[0][lane]` etc.; delete
the named fields. Now mono and poly are one structure. Callers use `len(voice, lane)` with voice 0 for
mono.

**Step 3 — move Spread onto the engine as `spread[voice][lane]`.** Retire `spreadEffective[]` on both
visuals (fixes the [4]-vs-[6] mismatch by construction). The visuals *compute* spread and write it
through a single-writer `setSpread(role, voice, lane, value)` mirroring `setStrand`; extend the ledger
to spread (this IS `SANDS_TOPOLOGY_RESOLVER_PLAN` step 5b). VAR/LEG columns documented-unused.

**Step 4 — collapse the PE `*Random[]` read switch(es)** the same way (or move `*Random` to
`[voice][lane]`), retiring the last hand-written strand→field map.

**Step 5 — lint + lock.** grep: no `rhythmLen|melodyOff|spreadEffective|switch (strand)` outside the one
model. Keep the round-trip test + ledger permanently.

## 6. Decisions to settle before Step 1

1. **Lane count in the unified array: 6 (incl. VAR/LEG) or 4 + a mono-only tail?** Rectangular `[*][6]`
   with VAR/LEG spread unused is simpler to index (no branch); costs 2 unused float columns × 16 voices
   = trivial. **Lean: 6, rectangular, VAR/LEG-spread documented-unused.**
2. **Step 1 storage: pointer-to-member table vs. array-now?** (see Step 1.) **Lean: array-now**, so the
   first step is real progress toward §3, not a detour that gets un-built.
3. **Voice-0-is-mono everywhere, or keep a `kMonoSlot` alias?** Keeping the named alias
   (`VoiceResolver::kMonoSlot == 0`) reads better at call sites. **Lean: keep the alias, drop the
   separate storage.**
4. **Naming.** `len/off/rot/spread` as the four modifiers, `lane` (not "strand") as the axis — retire
   "strand" from the vocabulary except where it means the raw 16-step vector. Confirm before mass-rename.

## 7. Not in scope

- The probability *math* (SpreadInterp, getStrandIdx) is unchanged — only *where the inputs are stored*
  and *how they're addressed*. This is a storage/addressing unification, not a DSP change.
