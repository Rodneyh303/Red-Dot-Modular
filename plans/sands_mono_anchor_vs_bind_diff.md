# Sands Mono — anchor-vs-bind diff (the missing-controls bug, enumerated)

Scope: `MonsoonSandsVisualExpander` (Sands **Mono**, 48HP) only. This is the
"report before changing anything" deliverable. Nothing here is a code change; it
is the ground-truth inventory that the Mono migration will be checked against.

## The root cause, stated precisely

Three artifacts each carry a **copy** of the same geometry, with no cross-check:

1. `panel_src/gen_macro_mono.py :: gen_mono()` — draws the art AND emits a
   `components` anchor layer.
2. `src/MonsoonSandsVisualExpander.hpp` — a second copy of every X column and the
   `rowY()` formula, plus the id enums.
3. `src/MonsoonSandsVisualExpander.cpp` ctor — places every control with
   `mm2px(Vec(CONST_X, rowY(lane)))`, reading the `.hpp` copy.

The widget uses `setPanel(createPanel(...))` (lines 110-112), **not**
`loadPanel(...)`. `createPanel` never populates the SvgPanelKit shape cache, so
`findNamed()` returns nothing even if called. The generator's `components` layer
is therefore **dead weight** — the generator comment admits it: "these markers
are advisory" (`gen_macro_mono.py:210-212`). Result: the panel and the widget are
two independent geometries. Every fix to one silently un-fixes against the other,
which is the entire q-mix regression history.

## Anchor inventory (what the generator emits)

From `gen_mono()` `components` layer (lines 216-240). All editor-lane indexed,
all visible (`fill=none stroke=none`, never `display:none`):

| Anchor id pattern | Count | Source line | Kind |
|---|---|---|---|
| `input_<0+el*3+p>`  (LOR CV, el 0..6, p 0..2) | 21 | 219 | input |
| `param_<26+el*3+p>` (LOR atten)                | 21 | 220 | param |
| `param_<21+sidx>`   (spread base, sidx 0..4)   | 5  | 223 | param |
| `input_<21+sidx>`   (spread CV)                 | 5  | 224 | input |
| `param_<47+sidx>`   (spread atten)              | 5  | 225 | param |
| `param_dir_<lane>`      (lane 0..6)             | 7  | 228 | param (named) |
| `input_dir_mod_<lane>`  (lane 0..6)             | 7  | 230 | input (named) |
| `input_deleg_mod_<lane>`(lane 0..4)             | 5  | 234 | input (named) |
| `output_prob_<lane>`    (lane 0..6)             | 7  | 238 | output (named) |

Note the **two id conventions**: LOR/spread use bare numeric `kit_shape("input", idx, …)`
→ `input_<idx>`; dir/deleg/prob use descriptive `input_dir_mod_<lane>` etc.

## Bind inventory (what the widget actually places, and how)

Everything below is placed by absolute `mm2px`, **zero** anchor lookups:

| Control | Widget line | Placement | id used | Anchor that SHOULD back it |
|---|---|---|---|---|
| LOR CV jack (×21) | 180-181 | `mm2px(Vec(JACK_X[p], rowY(lane)))` | `cvId(lane,p)` = `0+lane*3+p` | `input_<0+el*3+p>` ✅ id matches |
| LOR atten (×21) StoreKnob | 183-187 | `Vec(ATTEN_X[p], rowY(lane))` | store-backed, **no paramId** | `param_<26+el*3+p>` ⚠️ anchor exists, control is param-less |
| Spread base (×5) StoreKnob | 209-213 | `Vec(SPR_BASE_X, rowY(edLane))` | store-backed, **no paramId** | `param_<21+sidx>` ⚠️ param-less |
| Spread CV jack (×5) | 234-235 | `mm2px(Vec(SPR_CV_X, …))` | `sprCvId(l)` = `21+l` | `input_<21+sidx>` ✅ id matches |
| Spread atten (×5) StoreKnob | 238-242 | `Vec(SPR_ATTEN_X, …)` | store-backed, **no paramId** | `param_<47+sidx>` ⚠️ param-less |
| Owner cell (×5) | 258-278 | `mm2px(Vec(OWNER_X, rowY(l)))` bare widget | none | **no anchor emitted** ❌ |
| Dir cell (×7) | 294-324 | `mm2px(Vec(DIR_X, rowY(lane)))` bare widget | none | `param_dir_<lane>` — emitted but IGNORED |
| Dir-mod jack (×7) | 328-330 | `mm2px(Vec(DIR_MOD_X, …))` | `dirModId(lane)`=`26+lane` | `input_dir_mod_<lane>` — emitted but IGNORED |
| Deleg-mod jack (×5) | 332-334 | `mm2px(Vec(DELEG_MOD_X, …))` | `delegModId(lane)`=`33+lane` | `input_deleg_mod_<lane>` — emitted but IGNORED |
| Prob-out jack (×7) | 337-340 | `mm2px(Vec(PROB_OUT_X, …))` | `PROB_OUT_START+l` | `output_prob_<lane>` — emitted but IGNORED |
| Visual editor | 117-169 | `mm2px(Vec(ED_X, ROW_TOP))` box | n/a | recess is art only — needs `param_editor_recess` anchor for box |
| Connect mark | 343-346 | `mm2px(Vec(W_MM*0.5, 124))` | n/a | no anchor |

## The three concrete defect classes

**Class A — id conventions don't match (the "missing control" the user hovers for).**
LOR atten / spread base / spread atten are **StoreKnobs with no paramId**, so
even after `loadPanel`, a numeric-id `bindParam` cannot target them. The generator
emits `param_<idx>` but there is no param `<idx>` to bind. These knobs can only be
bound by `bindWidget("param_atten_<lane>_<col>", storeKnobPtr)` — which requires
the generator to emit **descriptive** anchors, not numeric ones.
→ **Generator must switch LOR/spread anchors from `kit_shape("param", idx, …)` to
named `param_atten_<el>_<col>`, `param_spr_<sidx>`, `param_spratten_<sidx>`.**

**Class B — Owner cell has NO anchor at all.**
5 owner cells are placed at `OWNER_X` but `gen_mono()` never emits an
`param_owner_<lane>` circle (the owner_block at line 184 draws `draw_cells=False`
and emits no per-cell anchor). This is a bind with no anchor → the CI audit's
"every bind must resolve" catches it, and it's a latent drift point.
→ **Generator must emit `param_owner_<lane>` (lane 0..4).**

**Class C — count divergence is invisible.**
The widget loops `N_LANES` / `POLY_LANES` from `SandsGrid`; the generator loops its
own `N`/`N_SPREAD`. Today they agree (7/5), but nothing enforces it. When q-mix
widened 6→7, this is exactly the class of bug that dropped a lane.
→ **The anchor-vs-bind CI audit makes count divergence a build failure.**

## After migration, the widget's placement lines become

Every `mm2px(Vec(CONST_X, rowY(lane)))` is replaced by
`centerOf(findNamed("<anchor>"))`. The `.hpp` X-constants (`JACK_X`, `ATTEN_X`,
`SPR_*_X`, `OWNER_X`, `DIR_X`, `DIR_MOD_X`, `DELEG_MOD_X`, `PROB_OUT_X`) and
`rowY()` are **deleted** — the generator becomes the single geometry source.
Lane-name/label tables already live in `SandsLaneNames.hpp`; labels move to
`centerOf(findNamed(...))` so the MIX-IN-style drift cannot recur.
