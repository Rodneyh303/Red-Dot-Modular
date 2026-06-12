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

**Kit-from-SVG is the DEFAULT binding path. The JSON pipeline + generated
`.gen.hpp` is KEPT as an opt-in alternative — not deleted.**

Two legitimate authoring paths, picked per panel:

- **Kit-from-SVG (default).** The widget binds to named shapes in the panel SVG.
  Positions live in the SVG → you can **live-edit** them (move a knob in the SVG,
  reload, done) and there is **no second source to drift**. This is the big win
  for complex/designed panels where art and controls are co-designed.

- **Code-driven (`.gen.hpp`, opt-in).** JSON → SVG *and* a `constexpr` layout
  header; the widget reads positions from the header. Genuinely better when you
  want layout **as code**: programmatic placement, diffable numeric coords,
  generating many regular panels from data. Kept available for those cases.

The thing we DON'T want is a panel using BOTH at once — that's the
`NOTEVALS`/`GS_NOTE_FRACS` two-sources-of-truth trap (edit the JSON, regenerate
one, the other drifts). So the rule is **per-panel, pick one**:
- kit-bound panel → its SVG is the only position source (don't also read a header)
- code-driven panel → the `.gen.hpp` is the source (the SVG is generated to match)

The `.gen.hpp` generator stays in the tree; it's just not forced onto kit-bound
panels.

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

3. **Convert Causeway to the kit (default path), keep the generator available.**
   In `MonsoonCausewayExpander.cpp`:
   - `addInput(createInputCentered<PJ301MPort>(L(CausewayLayout::CAUSEWAY_SLEW_R_CV), module, …))`
     becomes
   - `bindInput<PJ301MPort>("input_CAUSEWAY_SLEW_R_CV", MonsoonIds::CAUSEWAY_SLEW_R_CV);`
   - Drop the `#include "gen/CausewayLayout.gen.hpp"` from this widget and bind
     labels via `findPrefixed("lbl_")`.
   - **Keep** `src/gen/CausewayLayout.gen.hpp` and the header-emit in
     `gen_layout.py` — they remain available for code-driven panels. Causeway just
     no longer *consumes* the header (its SVG is now the single source).

4. **`experiment/interchange-peranakan` → master.** The big art/engine branch.
   Only conflict is `MonsoonWidget.cpp` (peranakan edited the arc labels to read
   from `NoteValues.hpp`; variadic restructured the same widget to bind via the
   kit). Resolve by hand: keep the kit binding structure from variadic, and inside
   it keep peranakan's `NOTE_VALUES[i].label` loop for the arc labels. ~1 file,
   ~20 lines, mechanical.

## Per-panel producer assignment

Note the pending renames (art done, code held): **Causeway → Raffles**
(raffle-tickets motif), **Surge → Junction** (Bugis pinisi motif). "Causeway"/
"Surge" as names become obsolete.

| Panel (post-rename) | Producer | Rationale |
|---|---|---|
| Monsoon | hand | art + knobs co-designed (the interweaving you praised) |
| Interchange family (Straits E/W, Macro) | hand | Esplanade/MBS art interwoven with controls |
| Raffles (was Causeway) | JSON-or-hand | regular grid; layout is data, but kit-bind it |
| Junction (was Surge) | JSON-or-hand | regular grid; kit-bind it |

A panel can switch producer columns later with zero C++ change (binding is by
shape id either way).

## Post-merge: "all active panels → kit" branch

Do this AFTER the three-way merge, on one dedicated branch, because it pairs with
the renames — converting then renaming the same widget is doing it twice.

Sequence per panel: **rename + kit-convert in the SAME pass.**
- Surge→Junction: rename slug + plugin.json model + `SURGE_*`→`JUNCTION_*` enums,
  AND convert the widget to kit-bind. Head start: variadic's `SurgeKitTest.cpp`
  already kit-binds the 5 CV + 5 att controls by name — promote it to the real
  widget under the Junction name.
- Causeway→Raffles: rename slug + plugin.json + `CAUSEWAY_*`→`RAFFLES_*` enums,
  AND convert to kit. Recipe ready: see the parked WIP commit on
  `feat/causeway-on-kit` (`3e975b1`) — a working kit-bound Causeway widget
  (18 controls bound, labels via `centerOfNamed()`, no header). Re-apply it under
  the Raffles name + enum prefix rather than merging it as-is (its module is the
  pre-rename one).

Why not do it now: the parked Causeway-on-kit widget binds the pre-rename module;
landing it before the rename means rewiring twice. Park the recipe, do it once
post-merge.

## Net effect on the codebase

- One binding mechanism (kit) as the default; `.gen.hpp`/code-driven path KEPT as
  an opt-in for panels where programmatic placement is the better fit.
- One position source per panel (never both the SVG ids and a header at once).
- JSON kept where it pays; live-editable SVG binding the default for the rest.
- `centerOfNamed()` added to the kit so kit-bound widgets read label positions
  from the same shapes as their controls (no parallel label coords).
