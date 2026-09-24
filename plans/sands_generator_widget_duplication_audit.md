# Sands generator ↔ widget constant-duplication audit

Every constant below is defined in BOTH the panel generator (.py) and the widget
(.hpp/.cpp). Each pair is a latent drift point: re-running the generator, or editing
one side, can silently desync placement from the SVG the widget binds/draws against.
This is the root pattern behind today's regressions (the ED_W/4-vs-ED_W/5 MIX-IN drift,
the label mislabels, etc.).

Severity key:
- **HIGH** — used to DRAW labels/overlays at runtime AND to place shapes in the SVG →
  a mismatch drifts labels off controls (the MIX-IN bug). Fix by anchor-derivation.
- **MED** — column x-positions the widget also hardcodes for binds/overlays; a
  mismatch mis-places a control but the kit bind still finds it by id (so it's
  usually caught visually, not silently wrong).
- **LOW** — value happens to be identical / already sourced from SandsGrid on one side.

## Already single-sourced (GOOD — reference pattern)
- `ED_Y`, `ED_H`, `ED_LANE_H` in BOTH East.hpp and Macro.hpp come from
  `dotModular::SandsGrid::{LANE_TOP, monoHeight/polyHeight, LANE_H}` — the geometry
  header. The .py mirrors the NUMBERS but the .hpp derives them, so the .hpp side
  can't drift. (The .py still hardcodes e.g. ED_H=91 — see below.)
- Lane COUNTS: SandsGrid::{MONO,POLY,EAST}_LANES + LaneMapping bridge tables, now
  length-static_asserted (this session).
- MIX-IN group geometry (Macro): FIXED this session — draw() derives from
  label_mixin_/param_send_/param_taplor_/param_tapspr_ anchors; generator owns
  GROUP_W/BLEND_*.

## FIXED this session
| Constant | gen_macro_mono.py | StraitsSandsMacroVisual.cpp | Status |
|---|---|---|---|
| GROUP_W  | ED_W/ED_LANES (5) | (removed — anchor-derived) | ✅ single-source |
| BLEND_TOP| 85.0              | (removed — anchor-derived) | ✅ |
| BLEND_H  | 35.0              | n/a                        | ✅ (gen only) |
| SEND_DX/DY/Y0 | in gen        | (removed — anchor-derived) | ✅ |

## REMAINING duplicates (candidates, by module)

### Macro — gen_macro_mono.py ↔ StraitsSandsMacroVisual.hpp/.cpp
| Constant | .py | .hpp/.cpp | Sev | Note |
|---|---|---|---|---|
| ED_X | 88.0 | 88.f (hpp:37) | MED | column origin; widget also uses in draw() header x |
| ED_W | 111.0 | 111.f (hpp:38) | MED | group pitch base (now only gen uses it for boxes) |
| ED_H | 65.0* | polyHeight() (hpp:47) | LOW | *gen hardcodes; hpp derives — MISMATCH RISK: gen 65 vs 5×13=65 ok now, but not linked |
| OWNER_X | 205 | 205.f (hpp:19) | MED | cell column (kit-bound by id, so visually caught) |
| DIR_X | 212 | 212.f (hpp:20) | MED | |
| DIR_MOD_X | 220 | 220.f (hpp:21) | MED | |
| PROB_OUT_X | 236 | 236.f (hpp:22) | MED | |
| JACK_X/ATTEN_X/SPREAD_X | lists | COL_* (hpp:35-36)/SPREAD_X | MED | column x's; kit-bound |

### East — gen_east_clean.py ↔ StraitsEastSandsVisual.hpp/.cpp
| Constant | .py | .hpp | Sev | Note |
|---|---|---|---|---|
| ED_X | 88.0 | 88.f (hpp:14) | MED | |
| ED_W | 111.0 | 111.f (hpp:15) | MED | draw()/label x |
| ED_H | 91.0* | monoHeight() (hpp:46) | LOW | gen hardcodes 91; hpp derives 7×13=91 — not linked |
| OWNER_X/DIR_X/DELEG_MOD_X/DIR_MOD_X/PROB_OUT_X | 205/212/220/228/236 | same (hpp:16-22) | MED | |
| JACK_X/ATTEN_X/SPREAD_X | lists | COL_*/SPREAD_X (hpp:35-37) | MED | |
| **BLEND_TOP / BLEND_H** | 74.0 / 22.0 (py:234-235) | **none** | **RESOLVED — not a drift** | VERIFIED: East's widget draw() paints only the background; it draws NO blend/row labels (all controls are kit-bound OwnerCell/DirCell/knobs that self-position from panel anchors). So the py BLEND block is static art with no widget counterpart → cannot drift. The reported "inconsistent row labels on hover" is the tooltip = `configInput` text, which was already routed through SandsLaneNames::EDITOR/SPREAD in the lane-name pass. |

### Mono — gen_macro_mono.py gen_mono() ↔ MonsoonSandsVisualExpander.hpp
| Constant | .py | .hpp | Sev | Note |
|---|---|---|---|---|
| ROW_TOP/ROW_BOT | 14/105 | (hpp uses SandsGrid) | LOW | |
| ED_X/ED_W | 88/111 | 88/111 (SandsMonoVisualIds) | MED | |
| JACK_X/ATTEN_X/SPR_* | lists | (hpp cols) | MED | |

## Recommendation (single session to eliminate, per Rodney)
1. **HIGH first**: verify East's blend/owner-source block. If the East WIDGET draws
   labels over the py's BLEND_TOP=74 block using its OWN BLEND_TOP, apply the SAME
   anchor-derivation fix used for Macro MIX-IN (emit label anchors, derive in draw()).
2. **Kill the MED column duplicates structurally**: the widgets that still hardcode
   column x's for overlays (mod-arcs, labels) should derive them from the kit anchors
   they ALREADY bind (input_/param_ ids) via findNamed/centerOf — the same pattern.
   The kit BIND already uses the anchors; only the draw()/overlay maths duplicates x.
3. **Link the LOW ED_H/ED_W numbers**: the generators should import the SandsGrid
   values rather than hardcoding 65/91/111 (a tiny shared constants module, or emit
   them into the SVG and read back). Lower priority — they match today and are
   asserted indirectly by the anchor-derived draws once (2) lands.

The end state: the generator is the SOLE owner of geometry; every widget position
comes from a panel anchor via findNamed/centerOf; the only numbers in the widget are
lane COUNTS (already SandsGrid + static_asserted). Then re-running the generator can
never undo a widget fix, because the widget has no geometry to undo.
