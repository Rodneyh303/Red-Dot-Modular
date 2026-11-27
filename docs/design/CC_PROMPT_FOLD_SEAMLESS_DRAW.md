# CC prompt — Fold step 2: the seamless draw (titled bands, no seam)

> **STATUS (2026-11-22): §1 (FoldBandLayoutCore.hpp) and §2 (test_fold_band_layout, registered,
> green, mutation-proven) are DONE — built directly, not by CC. CC's remaining work is §3 THE DRAW
> only (SandsFold.cpp): consume the core, the StraitsSeamFiller edges, the theme finding. Do NOT
> re-implement the core; the layout arithmetic lives in FoldBandLayoutCore.hpp. Note also
> FoldBandSetCore.hpp (width-from-enabled-bands + edge-pinned order + removal guard, test_fold_band_set):
> the band SET/width/order feeds layoutBands(totalPx=FoldBandSetCore width, specs in FoldBandSetCore order).**


Branch: `fix/fold-resize-spike` (continue). Plan context: `docs/design/SEAMLESS_FOLD_PLAN.md`
(build order step 2) and `SEAMLESS_SUBPANEL_IDEA.md`. Do this AFTER the push-order fix
(`CC_FIX_FOLD_PUSH_ORDER.md`) lands, or in a separate commit series; they touch different code
(draw vs `applyWidth`/`restoreMoves`), but both edit `SandsFold.cpp`, so rebase carefully.
Rack cannot run here: every visual claim is a feel-test item for Rodney. Make the LAYOUT a
tested Rack-free core; keep the draw a thin consumer of it.

## Goal
The folded module reads as ONE window (Tessera Roll's feel). Today `SandsFoldWidget::draw`
paints one flat band with spike text ("SENDS · FOLD", footer "dot modular — fold spike").
Replace it with a band model so later steps (Distribution band, Macro band) just add bands,
and so the look has no module-seam look at any allowed width.

"Seamless" here means two things:
1. INSIDE the module: bands are separated by a titled divider, never by a gap, a doubled
   border, or a mismatch of background. One continuous background fill, bands titled with the
   SUBPANEL NAMES (Rodney's ruling: "Titled bands keep the subpanel names").
2. AT THE EDGES: when a Sands/Straits family module is docked flush on the left or right,
   the join shows no sub-pixel seam (reuse `redDot::StraitsSeamFiller`, as
   `SandsMacroSendSubpanel.cpp` ~178-195 does: rule-free, host bg, family dark #16181c).

## 1. Pure layout core — `src/ui/FoldBandLayoutCore.hpp` (Rack-free, namespace `redDot::fold`)
Every number comes from here; `SandsFold.cpp` does no layout arithmetic (same discipline as
`FoldPanelWidthCore.hpp`).
- `struct BandSpec { int titleId; float minWidthPx; bool flexible; }` and an ordered list
  (left to right). Today: ONE band, SENDS (flexible), spec'd so adding Distribution / Macro
  bands is data, not a rewrite. Exactly one flexible band per layout is the grow target
  (the plan: Distribution band grows right, Sands band grows left); assert/handle 0 or >1 safely.
- `layoutBands(totalPx, specs) -> std::vector<BandRect>` with `{titleId, x0Px, x1Px}`:
  rects TILE `[0, totalPx]` exactly — each `x1 == next.x0`, first `x0 == 0`, last `x1 == totalPx`,
  no overlaps, no gaps, every rect >= its `minWidthPx` when `totalPx` allows; below the sum of
  mins, define and test the behaviour (flexible band clamps to its min; never negative width).
- `dividerX(rects)` = the interior band boundaries (the divider draw positions), PIXEL-SNAPPED
  to integers (the width is a multiple of `kGridPx = 15` so interior edges built from integer
  band widths stay integer; assert it, because half-pixel dividers are exactly what shows as a seam).
- `titleAnchorPx(rect)` and a conservative `titleFits(rect, charCount, fontPx)` (use a fixed
  per-character advance estimate, a named constant) so a title is drawn only when it fits and
  otherwise abbreviated/omitted deterministically. No text overlap at the minimum HP.
- Header/footer strip geometry constants (the 24 mm header rule and 8 mm footer rule currently
  hardcoded in `draw`) move here as named constants.

## 2. Tests — `test/test_fold_band_layout.cpp` (register in `test/run_all.sh`; failing first)
INDEPENDENT oracle: recompute expected tilings by brute force from the spec (accumulate mins,
distribute the remainder to the flexible band) — do not call `layoutBands` inside the oracle.
- For EVERY allowed HP (`isAllowedHp`, `contentMinHp(BAND_RIGHT_MM)` .. `kMaxHp`, step `kStepHp`)
  and several spec lists (1 band; 2 bands, flexible left; 2 bands, flexible right; 3 bands):
  exact tiling, no gap/overlap, mins honoured, dividers integer, widths monotone in HP.
- Title placement: no two titles overlap at the minimum HP; a title never extends past its band.
- Degenerate inputs: zero/negative total, no flexible band, two flexible bands — defined, no crash.
Mutation-check: (a) off-by-one on a boundary (x1 = next.x0 + 1) -> fail; (b) dividers snapped with
floor on a half-pixel input -> fail; (c) flexible remainder dropped -> fail; (d) title-fit always
true -> the min-HP overlap test fails. Show red first, then green, then the full suite
(`CPLUS_INCLUDE_PATH=/home/claude/simde bash test/run_all.sh`).

## 3. The draw (`SandsFold.cpp`, thin)
- ONE background fill of the whole box (the family dark, from the shared constants — grep
  `dotmod_design` / the palette the subpanels use; do not invent a second colour), then per band:
  the title (the SUBPANEL name: "SENDS" for this band — drop "FOLD" and the spike footer
  "dot modular — fold spike"; footer text becomes the family footer or nothing), the column
  captions at the existing mm stations, the lane labels. Dividers only BETWEEN bands, 1 px,
  integer x, the family's subtle rule colour. NO per-band background rect (no overlapping
  fills = no seam).
- Text that does not fit its band (per the core) is clipped/omitted via the core's decision,
  not by overdrawing neighbours.
- Theme: the existing draw is dark-only. Report how the family handles light/dark
  (`SandsMacroSendSubpanel.cpp` swaps `panelSvgDark/Light` per frame ~225). If it is a simple
  switch, follow it; if it is more than that, STOP at dark-only and say so in the report.
- Seam fillers: add `StraitsSeamFiller` left and right, `ruleHeightPx = 0`, family bg, active
  only when a family neighbour (`SandsMacroSendSubpanel::isFamilyModel`-style predicate, extended
  to include `modelSandsFold` itself and the Sands/Straits host visuals) sits FLUSH on that side
  (|neighbour edge - our edge| < 1 px) in the same row. Toggle in `step()` like the subpanel.
  The right filler position must follow every width change: put it in `repositionRightAnchored()`
  with the screws, and in `setOwnWidth`'s path, so it is correct after resize, undo and redo.
- Do not touch the resize/undo logic or the knobs' store wiring.

## Report back
- Red/green evidence, mutation results.
- Which colours/constants were reused (file + name), and what the theme finding was.
- The flush-neighbour predicate you used.
- Anything the core could not express.

## Rodney's feel-test list (needs Rack — do not claim these)
- No visible seam or flicker in the panel at ANY allowed width, at 50 / 100 / 200 % and a
  fractional zoom.
- Dock the fold flush against a Sands Macro / Straits panel on each side: the join is invisible;
  push it 1 px away: the filler turns off; resize, undo and redo keep the filler on the correct edge.
- Titles/captions legible at the minimum width; nothing overlaps.
- Patch save/load at several widths looks identical.
