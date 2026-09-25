# SvgPanelKit batch-binder survey — where the count/prefix binders apply

Binding N similar controls by name is a recurring pattern. The kit now offers, in
`src/ui/SvgPanelKit.hpp` (`Bind` mixin):

- **Variadic pack** — `bindParams/Inputs/Outputs/Lights("prefix", id0, id1, …)`
  — compile-time count; names `prefix0..prefixN`; ids passed explicitly.
- **Count-driven** (added this migration) — `bindParamsN/InputsN/OutputsN(
  "prefix", count, startId, config=nullptr)` — RUNTIME count (pass a
  `SandsGrid::…` constant so the loop bound can't drift), names `prefix0..
  prefix(count-1)`, ids `startId..startId+count-1`, one `config` applied to each.
- **Initializer-list custom** — `bindParams({"a","b"}, cfg, idA, idB)` /
  `bindParamsCustom({...}, ConfigPair{...}, …)` — heterogeneous names/ids/config.

## Converted this pass (Sands Mono)

`MonsoonSandsVisualExpander.cpp` dir-mod / deleg-mod / prob-out rows →
`bindInputsN`/`bindOutputsN` with `SandsGrid::MONO_LANES` / `POLY_LANES`. Audited
1:1 by `test/audit_anchor_bind.py`.

## Conversion candidates (deferred to each module's own migration)

| Site | Pattern | Fits which binder | Note |
|---|---|---|---|
| `StraitsEastSandsVisual.cpp:315-319` dir-mod + deleg-mod | `input_dir_mod_<l>` / `input_deleg_mod_<l>`, ids contiguous, theme config | `bindInputsN` (with config) | Do in **Stage 2 (East)** |
| `StraitsSandsMacroVisual.cpp:326` dir-mod | contiguous + theme config | `bindInputsN` | Do in **Stage 3 (Macro)** |
| `MonsoonShophouseExpander.cpp:268` scale row | `param_scale_<f>` f=0..N-1 contiguous | `bindParamsN` | Clean; standalone module task |

## Candidates that DON'T fit the current N-form (would need extension)

- `Sikit.cpp:120` and `MicroTuning.cpp:476`: loop starts at **i=1**, so names are
  `param_cents_1..`. `bindParamsN` emits `prefix0..`. → add an optional
  `startIndex` (name suffix base) param to the N-binders before converting these.
- East `input_<cvId(r,c)>` / Macro `input_<cvId(lane,c)>` (2-D CV grids): the
  numeric id is embedded in the NAME (`"input_" + cvId`), and ids are non-contiguous
  across the 2-D fold. These are not a simple prefix+contiguous row; leave as
  explicit loops OR migrate the generator to emit `input_cv_<lane>_<col>` (as Mono
  now does) and then a nested `bindInputsN` per lane becomes possible.
- East `output_prob_<l>` : name index `l` but id `PROB_OUT_REST +
  EDITOR_TO_ENGINE_LANE_QMIX[l]` (permuted) → non-contiguous id; keep explicit or
  add an id-map overload.

## Suggested small extension (when the offset cases are tackled)

Add `int startIndex = 0` to `bindParamsN/InputsN/OutputsN` so the name suffix can
begin at a non-zero base (covers Sikit/MicroTuning `i=1`). Keep it optional so
existing call sites are unaffected.
