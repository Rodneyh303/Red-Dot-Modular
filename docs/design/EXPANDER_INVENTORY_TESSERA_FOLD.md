# Tessera source notes — zip-verified mechanics for the fold arc

Status: COMPLEMENT. The classification authority is GROUP_PANEL_INVENTORY.md (Rodney's
2026-11-22 rulings: 1:1 association folds, 1:many is an island) and the rulings live in
SEAMLESS_FOLD_PLAN.md (one ModuleWidget grows; old modules kept as models alongside;
declare-everything-up-front ports; packed-poly + mono-breakout, SUM combine). This doc
does NOT re-classify. It records what the actual Tessera source
(`res/tessera-main.zip`, read 2026-11-27) verifies about the mechanics those rulings
assume, plus a first pass of the port counts the inventory defers to freeze time.

(An earlier draft of this file re-derived a classification from plugin.json/source; it
is superseded by GROUP_PANEL_INVENTORY.md — notably it mis-classed Lantern as a fold
pane, where the 1:many ruling correctly makes it an island: a free-aimed VIEW.)

## 1. The resize mechanics, verified in Tessera's source

SEAMLESS_FOLD_PLAN rules "one ModuleWidget grows." Tessera's implementation of exactly
that, for reference when the spike hardens (file: `src/ui/TesseraWidget.cpp`):

- `applyWidth(int hp)`: `box.size.x = hp * RACK_GRID_WIDTH`; right screws repositioned
  to `width - 2*RACK_GRID_WIDTH`; the editor region fills `width - controlsWidth`
  (controls stay put at the left); extension/editor widgets toggled `visible` on wide.
- Row collision is one public call: `APP->scene->rack->setModulePosForce(mw, mw->box.pos)`
  after the size change — Rack shoves the neighbours; no private API.
- A `PanelResizeHandle` strip at the right edge drags the width; snap-closed below the
  midpoint between `kPanelMinHp = 24` (controls alone) and the editor minimum; clamped
  to `kPanelMaxHp = 160`.
- Persistence: `panelHp` in the module JSON, CLAMPED on load (never trust the saved
  value), kept under a `ui` sub-object that survives preset save/load selectively.

## 2. The second surface Tessera has and we haven't discussed

Besides the widened panel, Tessera has `EditorWindow`: a full-Rack-window overlay
(`OpaqueWidget` + an explicit event policy — every key/mouse inside is consumed so the
rack never pans or fires shortcuts behind it; only save/open/quit/fullscreen pass).
One open at a time; it closes itself if its module is deleted. Both surfaces host the
SAME pane stack (`EditorHost`), so the overlay is nearly free once panes exist.
Optional for us (Sands authoring is the plausible customer); independent of the fold.

## 3. Where Tessera differs from our rulings (deliberately, ours stand)

Tessera's editor surfaces carry NO jacks — its IO lives on small fixed breakout
modules. Our ruling is different and already made: ALL ports declared up front on the
group module, the pop-out merely reveals already-declared port widgets, packed-poly on
the main + mono-per-jack breakout on the pop-out, SUM combine. Recording the divergence
so nobody later "fixes" us toward Tessera: our way keeps cables valid across open/close
by construction, which is the stronger property for a patching instrument.

## 4. License

Tessera is GPL-3.0-or-later. Ruling (Rodney 2026-11-27): dot.modular WILL be open
source, license to be decided; no wholesale copying either way. Everything in §1–2 is
public Rack SDK usage (patterns, not code), so nothing above depends on the license
choice. `plugin.json` still says `"proprietary"` — update it when the license is chosen.

## 5. Port-count first pass (the freeze-time check GROUP_PANEL_INVENTORY defers)

Bind CALL SITES in each module's widget (`bindInput/bindOutput/addInput/addOutput`);
sites inside loops multiply into more physical jacks (e.g. StraitsBase's subgate MOD
binds at StraitsBase.cpp:298 loop over lane x decision), so freeze-time counting must
expand loops. Zero-jack modules omitted.

| Module | in sites | out sites |
|---|---|---|
| MonsoonWidget (the host) | 12 | 8 |
| MonsoonRafflesExpander | 17 | 0 |
| MonsoonInterchangeExpander | 4 | 0 |
| MonsoonCausewayPolyExpander | 3 (poly) | 0 |
| MonsoonJunctionExpander | 1 | 0 |
| MonsoonShophouseExpander | 1 | 0 |
| MonsoonShophouseMicro | 1 | 0 |
| MonsoonStraitsExpander | 1 | 5 |
| StraitsBase | 2 (one looped) | 5 |
| StraitsEastSandsVisual | 6 | 1 |
| StraitsSandsMacroVisual | 4 | 1 |
| SandsDistributionExpander | 1 | 0 |
| MonsoonChangiExpander (T1) | 0 | 3 |
| MonsoonChangiT2Expander | 0 | 2 |
| MonsoonChangiT3Expander | 0 | 1 |
| Intertropical | 0 | 1 (+config'd 1 in / 6 out) |
| PhaseDistortion | 1 | 1 |
| Keppel | 12 | 1 |
| Woodlands | 0 | 7 |

The hidden spawned panels (5 Straits lanes, 8 subgate panels, QmixOct) and the Sands
subpanels (MacroSend, PhaseDist) bind NO ports of their own — consistent with the
declare-on-host rule already being half-true in the existing code.
