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

### Symptom 1 — mixed mono conventions (CENSUSED against call sites, not comments)

| array | convention | how verified |
|---|---|---|
| `lorBase[288]`, `spread[64]` | **OK** — voiceSlot, mono = slot 0 | callers use `VoiceResolver::voiceSlot(...)` |
| `varlegAtten[96]`, `macroSend[256]`, `macroAtten[256]` | **OK** — voiceSlot, mono = slot 0 | East passes `VoiceResolver::voiceSlot` results |
| `laneDir[96]`, `macroOwn[64]` | **LEGACY** — poly-bank, mono at index **15** | `getMonoLaneDir`/`getMonoMacroOwn` read `15*6` / `15*4` |
| `varlegDeleg[30]` | **NO MONO SLOT** — poly only (v = 0..14) | declaration + accessors |

So FIVE of eight arrays are already on the right convention; only two are legacy and one
lacks a mono slot. The scope of the fix is much smaller than the struct's comments suggest.

`macroOwn`'s comment is actively WRONG and is what made the whole struct look inconsistent:
it says "v=0..15 voice-slot, 15=mono", but `voiceSlot(V1)` is 0 — it is poly-bank indexing,
not voice-slot. Fix the comment even if the array migration is deferred.

CAUTION for the migration: several call sites pass `ch - 1` / `pv` rather than a named
`voiceSlot(...)` expression. Those must be read individually — the accessor tells you the
array's convention, but only the call site tells you what the caller believed. This is the
documented off-by-one breeding ground (two parallel addressing schemes).

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

## RE-PRIORITISED after the call-site census (supersedes the order below)

Reading every `laneDir`/`macroOwn` call site changed the ranking.

**Symptom 1 (mono convention) is inconsistency WITHOUT a bug, and does NOT block de-param.**
Those arrays use dual accessors — `getMacroOwn(polyBank,…)` / `getMonoMacroOwn(…)` — and
every call site correctly selects one:
```cpp
const float cur = (ch == 0) ? mm->getMonoMacroOwn(eng) : mm->getMacroOwn(ch - 1, eng);
if (mono) m->setMonoLaneDir(lane, v); else if (pv >= 0 && pv < 15) m->setLaneDir(pv, lane, v);
```
Mono is EXPLICIT at every site — arguably safer than a single accessor where mono is
silently "slot 0" and easy to pass wrong. Migrating means ~20 sites of working code with a
silent-failure mode, for uniformity only. And de-paramming moves params→store; it is
indifferent to how the store indexes internally.

**Symptom 2 (global scope has no home in the model) IS the real break, and DOES block it.**
With no global slice in the store, Macro's `params[]` became the model and the engine reads
them directly. That is the actual MVC violation, the reason East and Macro need different
de-param work, and what must be fixed first.

### Revised order
0. **DONE** (fix/mvc-step0-conventions) — correct the wrong/misleading convention comments.
1. **Add the global slice; move Macro's 44 engine-read params onto it.** The real unblocker.
   Afterwards every module's params are views and de-paramming is one shape of job.
2. Resume de-param: East (38) → Mono (54) → Macro (60), now uniform.
3. **Optional / low priority** — reconcile `laneDir`/`macroOwn` onto `voiceSlot`, give
   `varlegDeleg` a mono slot. Uniformity only; do it when those arrays are being touched
   anyway, not as standalone risk.

## Original order (superseded)

This precedes the remaining de-param work. Doing it after would mean migrating East onto a
model we then change, and re-platforming Macro twice.

Suggested slices, each build-verified (a wrong slot is a SILENT wrong-value read):
0. Fix `macroOwn`'s incorrect comment immediately (zero risk, and it is what makes the
   struct read as more broken than it is).
1. Reconcile `laneDir` and `macroOwn` onto `voiceSlot` — the ONLY two arrays needing it.
   Highest off-by-one risk; do it alone, extend the static_asserts, and read every
   `ch - 1` / `pv` call site individually rather than trusting the accessor.
2. Give `varlegDeleg` a mono slot so it matches the others.
3. Add the global slice and move Macro's engine-read params into it (the re-platforming).
4. THEN resume de-param: East, Mono, Macro — all now the same shape of job.

## Note for whoever plans work on top of this
Check the MODEL layer before sequencing VIEW-layer work. The de-param plan in
PARAM_CLASSIFICATION was sequenced without reading `EditorState`, which is why this was
found at slice four rather than slice zero.
