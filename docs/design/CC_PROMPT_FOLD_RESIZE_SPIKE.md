# CC PROMPT — seamless fold, step 1: the runtime-resize spike (Sends band)

Read first: `CLAUDE.md`, `docs/design/CHAT_HANDOFF.md`, `docs/design/SEAMLESS_FOLD_PLAN.md`
(this prompt is its build step 1), `docs/design/SEAMLESS_SUBPANEL_IDEA.md`,
`docs/design/MODULE_DISCOVERY_AND_VARIABLE_WIDTH.md`.

## Goal

Prove ONE thing: a NEW module whose width changes at runtime (`box.size.x`), with the
width saved in the patch, neighbours pushed aside, an undo step, and a background that
scales. Host the **Sends** band in it, because Sends has NO ports and NO Rack params (98
store-backed knobs, `SandsMacroSendSubpanel`), so nothing else can go wrong.

## Rulings (Rodney) — do not relitigate

- True resizable fold: ONE ModuleWidget grows. Not abutting modules.
- The OLD modules are NOT deleted or changed. The folded module is a NEW model beside
  them. Existing patches must load byte-for-byte as before.
- Port packing (Macro 28 CV -> 4 poly, Distribution 56 -> 8 poly, etc.) is LATER steps.
  Do not touch ports here. This spike has zero inputs/outputs and zero params.

## Reference (learn the pattern, do NOT copy code; GPL-3.0)

`res/tessera-main.zip` (Tessera's Roll): width is saved state (`panelHp`),
`applyPanelWidth` sets `box.size.x = hp * RACK_GRID_WIDTH` and calls `setModulePosForce`
so neighbours move out of the way, and the width change is on the undo stack. Write our
own, using our conventions.

## Do

1. **New model**, working name `SandsFold` (Rodney will rename; Singapore-geography naming
   is brand identity, so keep the name in ONE constant). Register in `plugin.json` and
   the plugin registration alongside the others. `config(0, 0, 0, 0)`.
2. **Rack-free sizing core** in a header under `src/ui/` (pure, unit-testable): allowed HP
   steps, clamp, hp -> px, "content needs at least N HP" rule. No `rack::` includes.
3. **Saved width.** `panelHp` in `dataToJson` / `dataFromJson`. Missing key -> default.
   Unknown/garbage value -> clamp, never crash.
4. **Runtime resize.** Widget method applies `box.size.x`, resizes the panel background,
   repositions right-anchored children (screws, right edge decor), calls
   `setModulePosForce` for neighbours. Triggered from a context-menu width choice.
5. **Undo.** A history action that restores the old width (and the old position if it was
   pushed). Mirror the Tessera idea only; use our own `history::ModuleChange` style
   already in the repo.
6. **Panel background that scales.** Either a procedurally drawn dark panel at any width,
   or SVG tiles. Prefer procedural: no new asset per width. Match the Sends panel look
   (`SandsMacroSendSubpanel.cpp` draws the title and footer itself).
7. **Host the Sends band.** Reuse the existing Sends knobs/widgets as a band in the new
   module. Read how `SandsMacroSendSubpanel` finds Monsoon and its editor store (the
   topology-epoch lookup in `ui/ModuleLookup*`) and use the SAME mechanism; do not invent
   a new one. The knobs must read and write the same store the old Sends subpanel does.
8. **Collision rule.** If the old Sends subpanel and the new fold are both docked to the
   same Monsoon, they edit the same store. That is fine for the spike; just note it.

## Tests (rack-free, `test/run_all.sh`, add to `TESTS[]`)

- Sizing core: clamp, HP -> px, min-content rule, round trip.
- Width persistence: JSON round trip, missing key, garbage value.
- Any pure layout helper you extract (anchor positions for a given width).
- Each test must FAIL before and PASS after (write it red first).

## Cannot verify here (say so; Rodney feel-tests in Rack)

- Neighbour push actually moves modules and no overlap remains.
- Undo/redo of a width change.
- Patch save/load keeps the width and a patch with the old Sends module still loads.
- Resizing while cables are attached (none expected in this spike).
- Background scaling at every allowed width, no seams, no flicker.
- Knobs read and write the same store as the old Sends subpanel.

## Out of scope

Ports, poly-packing, the Distribution and Macro bands, loadout slots, patch conversion,
the 16/32/64 step idea. Each is a later step in `SEAMLESS_FOLD_PLAN.md`.

## Done when

Unit tests green, `python3 test/audit_anchor_bind.py` still green, the plugin builds
(Rodney's job if no SDK), and the "Cannot verify" list is handed to Rodney as the
feel-test checklist.
