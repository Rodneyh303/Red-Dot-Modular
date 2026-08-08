# dot.modular tuning preset format (.dmtune) -- portable tuning+scale library

A dot.modular-native JSON format that stores the FULL Micro state (all cents + all fader weights),
distinct from .scl. Purpose: a portable LIBRARY of tuning+scale setups the user builds themselves --
NOT a shipped ethnomusicology corpus, NOT a replacement for patch-save.

## Why this exists (scope, Rodney)

Two file formats, two jobs -- do NOT conflate them:
- **.scl (standard)** = what you PLAY. The active scale as an ordered pitch list. Portable to Scala,
  the archive, other synths. Collapses tuning and scale (SCALA_FILE_AND_LOAD_UI + round-4 correction).
  Lossy by design (inactive degrees' cents are not in the file). MUST stay standard -- never put fader
  data in a .scl; that forks the format and breaks every other tool.
- **.dmtune (dot.modular JSON)** = what you SET UP. The full 12/24-degree tuning table + the fader
  mask. dot.modular-only. Lossless round-trip back into a Micro. A portable unit you can move between
  patches and collect into a library.

Relationship to patch-save: the Micro state IS already saved in the VCV patch (dataToJson). .dmtune
does NOT replace that. It earns its place ONLY as a PORTABLE unit -- move a tuning+scale between
patches, share it, build a personal library. If you only need "restore my setup," patch-save already
does it; .dmtune is for the library/portability use case, which is the stated intent.

Explicitly NOT the goal: shipping a curated ethnomusicological scale collection. .dmtune is a
save/reload mechanism for whatever the USER builds -- it makes no scholarly claim about what any named
scale "is." Users (and Rodney) accumulate their own working set. This sidesteps the research burden
entirely.

## Format: JSON (chosen over extended-.scl-with-comments)

JSON, not .scl-with-comment-stashed-weights. Reasons:
- Matches existing VCV serialization (dataToJson/dataFromJson) -- reuse the infrastructure.
- Human-readable, trivially validated.
- Keeps the two jobs cleanly separate. Stashing weights in .scl comments is fragile (a comment is not
  a contract -- any other tool re-saving the .scl drops the data) and conflates .scl's portable-scale
  job with .dmtune's full-state job. Two distinct files, two extensions.

### Schema
```json
{
  "format": "dotmodular.tuning",
  "version": 1,
  "n": 12,
  "cents":  [0.0, 90.58, 200.0, ...],
  "weight": [1.0, 0.0, 0.75, ...],
  "name": "optional user label",
  "notes": "optional free text"
}
```
- `n`: 12 or 24 (Micro-12 vs Micro-24). Same format both; n distinguishes.
- `cents[n]`: ALL degrees' cents including INACTIVE ones (this is the whole point -- .scl loses these).
  Root (index 0) always 0.
- `weight[n]`: ALL fader positions (the scale mask). 0 = disabled degree.
- `name` / `notes`: optional user metadata for the library.
- `version`: for forward-compat.

Lossless round-trip: export .dmtune from a 12-tone-tuning-with-7-active -> re-import -> exact 12 cents
+ exact 7-active mask restored. (Contrast .scl from the same state: re-import gives a 7-note tuning,
the 5 inactive degrees' tuning gone.)

## Implementation

- Reader/writer: `dotModular::TuningPreset` in src/tuning/ (sibling to dotModular::ScalaFile). Small
  focused JSON struct, same "write our own" reasoning as the .scl parser.
- n-aware, one implementation for both Micros (mirrors ScalaFile's per-caller accept predicate; here
  the caller validates n == its N_DEGREES).
- Load/save UI: context-menu "Load .dmtune..." / "Save .dmtune..." alongside the .scl load/save items
  (SCALA_FILE_AND_LOAD_UI's osdialog pattern; filter "dot.modular Tuning:dmtune").
- On load: populate ALL cents params + ALL weight params from the file. On save: write ALL cents +
  ALL weights (not just active -- lossless).
- Reuse rack's json_t (jansson) -- already used by dataToJson; no new dependency.

## The four file operations on a Micro (summary)

| Format  | Load                              | Save                               |
|---------|-----------------------------------|------------------------------------|
| .scl    | N pitches -> N active degrees     | active degrees' cents, ascending   |
|         | (weight 0 for rest), NOTES=N      | (lossy: tuning of inactive lost)   |
| .dmtune | ALL cents + ALL weights restored  | ALL cents + ALL weights written    |
|         | (lossless)                        | (lossless, dot.modular-only)       |

.scl for interchange (portable, standard, lossy). .dmtune for the personal library (lossless, native).

## Cross-refs
- SCALA_FILE_AND_LOAD_UI.md -- the .scl reader/writer + osdialog UI this sits alongside.
- COLONNADES_PANEL_LIFT_SPEC.md round-4 -- the .scl tuning/scale collapse (why .scl is lossy, why
  .dmtune exists to be lossless).
- MonsoonMicro12.cpp -- the cents/weight params .dmtune reads and writes.
