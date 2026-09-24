# Sands East + Macro + Mono — finish the q-mix lane insertion (labels + bounds)

Branch: master. Scope: label arrays / loop bounds that still carry pre-q-mix
(4/6-lane) values, plus a compile-time guard so lane-count drift can't recur.

## Ground truth (do NOT re-derive by hand — route through these)
- Geometry counts: `SandsGrid::MONO_LANES=7`, `POLY_LANES=5`, `EAST_LANES=7`
  ([`SandsGrid.hpp`](../src/ui/SandsGrid.hpp:30)).
- Two label orderings that matter (SANDS_LANE_INDEX_AUDIT.md):
  - **EDITOR** (what rows/labels read top-to-bottom): `MEL OCT QMIX REST ACC VAR LEG`
    → q-mix at index **2**.
  - **ENGINE/SPREAD** (PROB_OUT ids, sprPid, spreadEffective): `REST MEL OCT ACC QMIX`
    → q-mix appended at index **4**.
  - Bridge tables already exist: `EDITOR_TO_ENGINE_LANE_QMIX`,
    `ENGINE_LANE_TO_EDITOR_QMIX` ([`LaneMapping.hpp`](../src/dsp/LaneMapping.hpp:117)).

## What is ALREADY correct (reference, do not touch)
- All of `StraitsEastSandsVisual.cpp` and `StraitsSandsMacroVisual.hpp`: use
  `SandsGrid::POLY_LANES/EAST_LANES` + the QMIX bridge; labels correct
  (`{"MEL","OCT","QMIX","REST","ACC"}` editor, `{"REST","MEL","OCT","ACC","Q-MIX"}`
  spread). Macro `.hpp` is fully migrated (macroBase[5][4], PROB_OUT_QMIX).
- `gen_east_clean.py` (N=7, ED_LANES=7, POLY_LANES loops, QMIX row 2).
- `LaneMapping.hpp` bridge tables + `lorStoreBank` guards.

## CONFIRMED STALE — the fix list

### A. Mono visual expander header — [`MonsoonSandsVisualExpander.hpp`](../src/MonsoonSandsVisualExpander.hpp)
1. **L150–152** PROB_OUT loop: `for (l=0; l<6; …)` + editor array
   `{"MEL","OCT","REST","ACC","VAR","LEG"}` — 6 entries, MISSING QMIX at index 2.
   → bound to `MONO_LANES` (7); array `{"MEL","OCT","QMIX","REST","ACC","VAR","LEG"}`.
   (Note PROB_OUT_START is editor-ordered here, unlike East's engine-ordered outs —
   verify against SandsMonoVisualIds before relabelling; keep whatever order the id
   enum uses and match the array to it.)
2. **L161** CV-config loop `for (l=0; l<6; …)` → `MONO_LANES`. (`names[]` at L154 is
   already the correct 7-entry editor array — good.)
3. **L181** dir-mod loop `for (l=0; l<6; …)` → `MONO_LANES`.
4. **L184–186** `delegNm[4]={"MEL","OCT","REST","ACC"}` + `for (l<4)`:
   delegation targets are the POLY lanes → `POLY_LANES` (5) and array
   `{"MEL","OCT","QMIX","REST","ACC"}`. Confirm delegModId count == POLY_LANES.
5. **L195** `bool delegModPrev[4]` → `[SandsGrid::POLY_LANES]`.

### B. East visual header — [`StraitsEastSandsVisual.hpp`](../src/StraitsEastSandsVisual.hpp)
6. **L218 / L224–226** `laneNm[4]`, `laneNames[4]`, `paramNames[4]` + `for(lane<4)`
   in the CV-config block. These are SPREAD/poly lanes. laneNames is SPREAD order
   `{"REST","MEL","OCT","ACC"}` → append `"Q-MIX"` → 5, and bound `for(lane<POLY_LANES)`.
   (`paramNames[4]={Len,Off,Rot,Spr}` is the 4 COLUMNS — leave at 4.) `laneNm` at L218
   is `(void)laneNm`-discarded dead code; either delete or extend to 5 for consistency.
7. **L299** `laneNm[6]={"MEL","OCT","REST","ACC","VAR","LEG"}` (EDITOR order) in the
   dir/deleg gate-mod block + `for(lane<6)` at L303/L306 → array gets QMIX at index 2
   → 7 entries `{"MEL","OCT","QMIX","REST","ACC","VAR","LEG"}`, loops `< EAST_LANES`.

### C. Macro generator — [`gen_macro_mono.py`](../panel_src/gen_macro_mono.py) `gen_macro()`
8. **L14** `N=5` but the draw/component loops use **`ED_LANES`** (L54, L72–74, L104,
   L112, L122) which is **never defined in `gen_macro`** — it leaks from module scope
   (East's `ED_LANES=7`) or NameErrors. This is the "Macro missing a row / labels
   mixed up" root cause: 7-lane loops into a 5-lane recess, or a crash.
   → Define `ED_LANES = 5` (POLY count) in `gen_macro`, OR replace every `ED_LANES`
   in `gen_macro` with `N`. Prefer an explicit local `ED_LANES = N` right after L14
   so the two names can't disagree.
9. **L26–27** `DISPLAY_ORDER=[1,2,0,3]` + `LANE_NAMES_D=["MELODY","OCTAVE","REST",
   "ACCENT"]` (4 entries, pre-q-mix, engine-ish order). The current draw path (L52–54)
   says "no ESLOT/DISPLAY_ORDER remap; row == editor lane" and doesn't use either —
   confirm they're dead, then DELETE both (leaving a stale 4-entry table is exactly
   the trap). If any label draw still reads `LANE_NAMES_D`, replace with the editor
   5-table `["MELODY","OCTAVE","Q-MIX","REST","ACCENT"]`.
10. Verify `gen_mono(dark)` (same file, L131+) — it already uses `N=7` and `range(N)`;
    just confirm no residual 4/6 literal or stale name array in its label draws.

## The recurrence guard (the point of this pass)
Add a compile-time length check tying each label array to its lane constant, so the
next lane change fails to BUILD instead of silently mislabelling. Pattern:

```cpp
static constexpr const char* kEditorLaneNames[SandsGrid::MONO_LANES] =
    {"MEL","OCT","QMIX","REST","ACC","VAR","LEG"};
static_assert(std::size(kEditorLaneNames) == SandsGrid::MONO_LANES,
              "editor lane-name table must match MONO_LANES");
static constexpr const char* kSpreadLaneNames[SandsGrid::POLY_LANES] =
    {"REST","MEL","OCT","ACC","Q-MIX"};
static_assert(std::size(kSpreadLaneNames) == SandsGrid::POLY_LANES,
              "spread lane-name table must match POLY_LANES");
```
- Put ONE editor table + ONE spread table in a shared spot both headers include
  (candidate: a small `namespace dotModular { namespace SandsGrid { … } }` addendum,
  or a new `SandsLaneNames.hpp`). Every `config*()`/label site indexes those, never a
  local literal. (C-string arrays can't cross TUs as `constexpr` easily — if a shared
  header is awkward, at minimum give EACH local array its own `static_assert` on the
  matching lane constant; that alone converts the silent mislabel into a build error.)
- Python side: after fixing, assert in each generator that
  `len(LANE_NAMES) == N`/`ED_LANES` so a future edit trips at gen time too.

## Order of work (one logical change; build+assert gate each step)
1. C++ headers A + B: swap literals → lane constants + correct arrays; add the
   per-array `static_assert`s.
2. `gen_macro_mono.py`: fix `ED_LANES`, delete/repair `DISPLAY_ORDER`/`LANE_NAMES_D`,
   add `len==N` asserts.
3. `make -j16` — must be clean (the new static_asserts are the real test).
4. Re-render panels: `python panel_src/gen_east_clean.py` and
   `python panel_src/gen_macro_mono.py` (confirm exact invocation from each file's
   `__main__`); rebuild.
5. Rack check: East's five spread rows read MEL/OCT/QMIX/REST/ACC; Macro shows five
   rows with matching labels; LEGATO row has its controls; nothing labelled OCTAVE on
   the melody row.

## Notes / hazards
- Do NOT renumber the poly ENGINE lane order (that's the deferred dedicated refactor
  in LaneMapping.hpp §NOTE) — only fix label arrays + bounds.
- Watch the editor-vs-engine ordering PER ARRAY (the brief's core warning): PROB_OUT
  and spread arrays are ENGINE/SPREAD order; row/label arrays are EDITOR order. Putting
  QMIX at index 2 in a spread-order array (or index 4 in an editor-order array) just
  moves the mislabel — check each array's convention at its declaration comment first.
