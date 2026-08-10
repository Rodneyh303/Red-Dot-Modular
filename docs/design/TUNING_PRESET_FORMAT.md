# dot.modular tuning preset format (.dmtune) -- portable tuning+scale

A dot.modular-native JSON format that stores a portable TUNING + SCALE (cents + enabled mask),
distinct from .scl. Purpose: a portable LIBRARY of tuning+scale setups the user builds themselves --
NOT a shipped ethnomusicology corpus, NOT a replacement for patch-save.

## WHAT .dmtune IS (final -- clarified by building Shophouse Micro)

Building Shophouse Micro (the consumer of this format) clarified what .dmtune should be. It reshaped
the format: earlier drafts called it "lossless full state" and stored weight[]. That was WRONG. The
final definition:

**.dmtune = a portable TUNING + SCALE. Two arrays: cents[] (the tuning -- where the N degrees sit) and
enabled[] (the scale -- which degrees are in play). It does NOT store weight.**

Why no weight: weight is the user's LIVE PERFORMANCE MIX (the faders). By the Shophouse analogy, a
scale MASKS the mix, it never MOVES your faders (non-destructive). Loading a .dmtune sets cents +
enabled and leaves the faders untouched -- so your performance mix rides THROUGH tuning/scale changes.
This is exactly what makes Shophouse Micro musical: switch .dmtune scenes while your fader mix persists.
If weight were in the file, loading a scene would overwrite your mix -- the thing Shophouse never does.

The three-format hierarchy (each a superset of the last, each with a clear owner):
- **.scl**  = tuning only (N cents).            Interchange standard. What you PLAY. Owner: Scala world.
- **.dmtune** = tuning + scale (cents + enabled). dot.modular native. What you SET UP. Owner: dot.modular.
- **VCV patch** = tuning + scale + mix + all.    Adds weight + everything. What you PERFORM. Owner: your patch.

The mix lives ONLY in the patch. That is the key realisation Shophouse Micro forced: the module's whole
point is that your mix rides through scene changes, which is only possible if the scenes carry no mix.

Symmetry worth noting: the cents / enabled / weight split that defines the three formats IS the
three-control split on the Colonnades panel -- cents knob, enable band, weight fader. Format layers and
panel controls are the same three facts, which is a sign the decomposition is right.

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
  "version": 2,
  "n": 12,
  "cents":  [0.0, 90.58, 200.0, ...],
  "enabled": [true, false, true, ...],
  "scaleOnly": true,
  "transposable": true,
  "name": "optional user label",
  "notes": "optional free text"
}
```
- `scaleOnly` (optional, default false): a 12-TET SCALE authored on a Monsoon — `cents` are the implied
  12-TET ladder and only `enabled` matters. Still a valid full preset (Colonnades can load it as a
  12-TET tuning+mask). UI hint only. (MONSOON_SCALE_AUTHORING_DIRECTION.)
- `transposable` (optional, default false): the `enabled` mask is ROOT-RELATIVE (tonic normalised to
  degree 0) and is transposed by the LIVE root control (Monsoon scale-menu root / Shophouse front
  root), exactly like a built-in scale — no absolute root stored. false = absolute mask. Microtonal
  (cents-carrying) .dmtune from Colonnades/Duo NEVER sets this (arbitrary tunings don't scale-
  transpose; Monsoon is 12-TET only). (TONIC_TRANSPOSE_BUILD_BRIEF.)
- `n`: 12 or 24 (Micro-12 vs Micro-24). Same format both; n distinguishes.
- `cents[n]`: ALL degrees' cents including INACTIVE ones (this is the whole point -- .scl loses these).
  Root (index 0) always 0.
- `enabled[n]`: per-degree SCALE MEMBERSHIP (the mask). true = in scale, false = out of scale. This is
  the scale carved into the tuning. (NO weight[] -- weight is the live fader mix, not stored here.)
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
- version is 2 (v1 stored weight[] and derived the mask from weight==0 -- superseded). v1 migrates on
  load: cents kept, enabled[i] = (v1 weight[i] > 0), v1 weight discarded.
- On LOAD: set cents params + enabled[]; DO NOT touch weight params (the faders stay as the user left
  them). On SAVE: write cents[] + enabled[]; weight is NOT written (it's the live mix, patch-local).
- Reuse rack's json_t (jansson) -- already used by dataToJson; no new dependency.

## The four file operations on a Micro (summary)

| Format  | Load                              | Save                               |
|---------|-----------------------------------|------------------------------------|
| .scl    | N pitches -> N active degrees     | active degrees' cents, ascending   |
|         | (weight 0 for rest), NOTES=N      | (lossy: tuning of inactive lost)   |
| .dmtune | cents + enabled restored;         | cents + enabled written;           |
|         | faders (weight) untouched         | weight NOT written (live mix)      |

.scl for interchange (portable, standard, lossy). .dmtune for the personal library (lossless, native).

## Cross-refs
- SCALA_FILE_AND_LOAD_UI.md -- the .scl reader/writer + osdialog UI this sits alongside.
- COLONNADES_PANEL_LIFT_SPEC.md round-4 -- the .scl tuning/scale collapse (why .scl is lossy, why
  .dmtune exists to be lossless).
- MonsoonMicro12.cpp -- the cents/weight params .dmtune reads and writes.

## .dmtune SUPERSEDES the .kbm motivation (Rodney)

Once .dmtune exists, most of the reason for .kbm evaporates. .kbm was wanted as the answer to the
"up-to-N loading" placement question -- when a short .scl loads into a Micro, .kbm's mapping vector
would say WHICH slot positions the degrees occupy (see SCALA_FILE_AND_LOAD_UI .kbm section). That is a
placement problem that exists ONLY because .scl is lossy about slot assignment.

.dmtune stores the full slot state directly -- every cents at every degree, every fader position. There
is no placement ambiguity because nothing was lost; the mapping is just THERE, not reconstructed from a
second file. So for the round-trip-into-a-Micro use case, .dmtune does not implement .kbm -- it makes
.kbm UNNECESSARY.

.kbm retains value ONLY for Scala-world interchange of MAPPINGS -- handing a scale+mapping to someone
using actual Scala software or another .kbm-aware tool. That is real but narrow, carries MIDI-facing
fields the Micros don't use, and needs the mapping-vector-onto-fader-slots reinterpretation worked out
earlier. Given the stated non-goal of ethnomusicology interchange, it is probably not worth the parser
complexity.

RULING: .kbm is DEFERRED / probably DROPPED. .dmtune covers the round-trip use case .kbm was wanted
for. Revisit .kbm only if pure Scala-mapping-interchange becomes a real, requested need.

Resulting format scope (three -> two):
- .scl    -- interchange of the scale/tuning (standard, portable, lossy by design).
- .dmtune -- portable tuning + scale (cents + enabled), native, one file. NOT full state (no weight).
- .kbm    -- dropped unless Scala-mapping-interchange is specifically needed later.
