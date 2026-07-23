# MVC unification — finish Stage 1 BEFORE any more de-param work

## The rule (what we should have been holding to)

**Params and widgets are always VIEWS. The store is always the MODEL. Scope — mono,
per-voice, global — selects a SLICE of the model; it must never change which layer owns
the data.**

Today that rule is broken in a way that made East and Macro need fundamentally different
de-param work (see PARAM_CLASSIFICATION "why East and Macro differ"). That difference is
not justified by their scope. It is an artefact.

## How we got here (diagnosed, not guessed)

`Monsoon::EditorState` shows a migration that was STARTED and left half-done. `lorBase` and
`spread` carry the comment "Stage 1 / Stage 1b of the MVC unification" and use a clean
convention. The arrays that predate them were never migrated, and Macro was never brought
into the store at all — so Macro's params became the model by default.

### Symptom 1 — three different mono conventions in ONE struct
| array | mono lives at |
|---|---|
| `lorBase[288]`, `spread[64]` (Stage 1/1b) | slot **0** (`VoiceResolver::voiceSlot`) |
| `laneDir[96]`, `macroOwn[64]` | index **15** |
| `varlegDeleg[30]` | **absent** — poly only |

`macroOwn`'s comment is self-contradictory: "v=0..15 voice-slot, 15=mono". `voiceSlot(V1)`
is 0, so "voice-slot" and "15 = mono" cannot both hold. This is exactly the documented
failure mode — two parallel addressing schemes is where off-by-ones breed.

### Symptom 2 — global scope has no home in the model
Macro's global LOR / attens / spread / taps live in Macro's `params[]`, which the engine
reads directly. So for global scope the VIEW is the MODEL. Nothing about "global" requires
that; the store simply has no global slice, so params filled the gap.

### Symptom 3 — the consequence we tripped over
East's params are a redundant mirror (delete them) while Macro's are load-bearing storage
(re-platform them). Same panels, same lane indexing, opposite layer roles.

## Target state

1. **One addressing convention everywhere**: `VoiceResolver::voiceSlot` — slot 0 = V1/mono,
   1..15 = V2..V16. No array keeps its own mono placement. `laneDir`, `macroOwn`,
   `varlegDeleg`, `varlegAtten`, `macroSend`, `macroAtten` migrate onto it.
2. **Global scope becomes a slice of the model**, not a param array — either an extra slot
   (16 = global) or a parallel `globalX[]` with identical bank/item shape. Then Macro reads
   and writes the store exactly as East and Mono do.
3. **No `params[]` is storage anywhere.** Engine reads the store; widgets are views over it;
   de-paramming becomes uniform — the same subtraction for every module.

## Order

This precedes the remaining de-param work. Doing it after would mean migrating East onto a
model we then change, and re-platforming Macro twice.

Suggested slices, each build-verified (a wrong slot is a SILENT wrong-value read):
1. Reconcile the mono convention in `laneDir` and `macroOwn` onto `voiceSlot` (highest
   off-by-one risk; do it alone, with the static_asserts extended).
2. Give `varlegDeleg` a mono slot so it matches the others.
3. Add the global slice and move Macro's engine-read params into it (the re-platforming).
4. THEN resume de-param: East, Mono, Macro — all now the same shape of job.

## Note for whoever plans work on top of this
Check the MODEL layer before sequencing VIEW-layer work. The de-param plan in
PARAM_CLASSIFICATION was sequenced without reading `EditorState`, which is why this was
found at slice four rather than slice zero.
