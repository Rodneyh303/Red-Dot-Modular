# Panel-build merge strategy: kit + pipeline + peranakan

## The situation

Three feature branches, all off the same clean master tip (`9ce38a5`), almost no
file overlap:

| Branch | What it is | Touches |
|---|---|---|
| `experiment/svghelper-variadic` | The mature **SvgPanelKit** binding layer (bind widgets to named SVG shapes) + Monsoon wired to it | 9 files |
| `feat/panel-layout-pipeline` | **JSON → SVG + generated C++ layout header**; Causeway wired to the header | 11 files |
| `experiment/interchange-peranakan` | All the recent art/engine work (NoteValues SSOT, Esplanade, MBS, Sands editor) | 46 files |

File-overlap (the entire conflict surface):
- variadic ∩ pipeline → `res/panels/Surge_panel_dark.svg` (1 file)
- variadic ∩ peranakan → `src/MonsoonWidget.cpp` (1 file)
- pipeline ∩ peranakan → none

## The core decision

**The kit is the single binding layer. The JSON pipeline is demoted to an
SVG-producer. The generated C++ layout header is dropped.**

Why: the pipeline currently derives **two** position sources from one JSON —
the SVG (`input_/param_` shape ids) *and* `CausewayLayout.gen.hpp` constants.
The kit only needs the SVG. Keeping both is the same two-sources-of-truth trap as
the old `NOTEVALS` / `GS_NOTE_FRACS` duplication: edit the JSON, regenerate one,
and the other silently drifts.

Proven-compatible: hand-authored Monsoon emits `param_NOTE_VALUE_PARAM`;
pipeline-generated Causeway emits `param_CAUSEWAY_SLEW_R_ATT` /
`input_CAUSEWAY_SLEW_R_CV`. Identical `{param|input|output|light}_{ENUM}` style.
The kit's `findNamed` / `findPrefixed` / `bindParams(prefix,…)` bind either one
with no change.

### Resulting model (the "best of both, per panel" you wanted)

```
            authoring (choose per panel)              binding (always the kit)
  grid/boring  ─ JSON ─┐
  panels               ├─►  panel.svg  (input_/param_ ids) ─►  kit.bind*()  ─► widget
  designed/    ─ hand ─┘
  interwoven    (Python art gens)
```

- A panel's SVG is **born** from JSON (Causeway, Surge, Raffles, Junction) or
  **hand-authored** (Monsoon, Interchange family — art co-designed with knobs).
- The C++ **never changes** when a panel graduates from JSON to hand-authored;
  it just binds names.
- One position source (the SVG). No generated header to drift.

## Merge order (low-risk → high-risk)

1. **`feat/panel-layout-pipeline` → master.** Lands the JSON authoring tools
   (`gen_layout.py`, `layouts/*.json`) and the generated Causeway SVG. Clean (no
   overlap with master beyond its own new files).

2. **`experiment/svghelper-variadic` → master.** Lands `SvgPanelKit.hpp` +
   `SvgHelper.hpp` + Monsoon-on-kit. Only conflict is `Surge_panel_dark.svg`
   (both branches added the kit ids to it) — take the variadic version (it has the
   binding ids).

3. **Convert Causeway from header to kit** (the SSOT-collapsing step). In
   `MonsoonCausewayExpander.cpp`:
   - `addInput(createInputCentered<PJ301MPort>(L(CausewayLayout::CAUSEWAY_SLEW_R_CV), module, …))`
     becomes
   - `bindInput<PJ301MPort>("input_CAUSEWAY_SLEW_R_CV", MonsoonIds::CAUSEWAY_SLEW_R_CV);`
   Drop `#include "gen/CausewayLayout.gen.hpp"`, delete `src/gen/CausewayLayout.gen.hpp`,
   and remove the header-emit step from `gen_layout.py` (keep the SVG-emit step).
   The labels block likewise binds via `findPrefixed("lbl_")` instead of
   `LBL_*` constants.

4. **`experiment/interchange-peranakan` → master.** The big art/engine branch.
   Only conflict is `MonsoonWidget.cpp` (peranakan edited the arc labels to read
   from `NoteValues.hpp`; variadic restructured the same widget to bind via the
   kit). Resolve by hand: keep the kit binding structure from variadic, and inside
   it keep peranakan's `NOTE_VALUES[i].label` loop for the arc labels. ~1 file,
   ~20 lines, mechanical.

## Per-panel producer assignment (initial)

| Panel | Producer | Rationale |
|---|---|---|
| Monsoon | hand | art + knobs co-designed (the interweaving you praised) |
| Interchange family (Straits E/W, Macro) | hand | Esplanade/MBS art interwoven with controls |
| Causeway | JSON | regular grid; layout is data |
| Surge | JSON | regular grid |
| Raffles, Junction | JSON | simple, broadly OK as-is |

A panel can switch columns later with zero C++ change.

## Net effect on the codebase

- One binding mechanism (kit) instead of two (kit + header reads).
- One position source per panel (the SVG).
- JSON kept where it pays (grid panels), dropped where it fought you (designed
  panels).
- The generated header — the latent drift risk — is gone.
