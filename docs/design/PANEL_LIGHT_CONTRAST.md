# Light-theme contrast — systemic issue across the panel set (backlog)

## Problem
The **light theme is low-contrast across all ~12 module panels** (Straits, Causeway, Changi,
Interchange, Junction, Lantern, Macro/Mono, Raffles, Shophouse, …). Pale/pastel motif colours
(waves, tile grounds, linework, decorative fills) sit on a near-white background (`#dcdcdc` /
`#e8e4d4`) with too little tonal separation, so texture and decoration that read well in the dark
theme nearly vanish in light. Dark theme is fine; the issue is light-specific.

Root cause: light palettes were each hand-picked per generator to be "soft," and drifted too light —
decorative strokes at low opacity on a light ground disappear.

## Why it's systemic (not per-panel)
Each of the 12 `panel_src/gen_*.py` defines its **own** `THEMES["light"]` dict — there is **no
shared palette**. So the same mistake was made independently 12 times, and any fix made per-panel
will drift again.

## The right fix (a real project, not a quick patch)
Extract a **shared `panel_src/panel_palette.py`** — one dark + one light palette (background, ink,
gold, jack well/ring, the decorative accent families) that every generator imports, replacing the
12 local dicts. Tune the light palette ONCE for adequate contrast:
- darker/more-saturated decorative colours (don't rely on near-white grounds),
- a slightly deeper light background so pale strokes separate,
- minimum contrast floor for any stroke meant to be *seen* (vs. faint texture).
Then re-render all panels and eyeball the set together.

This mirrors the engine refactor's "single source of truth" discipline (one palette authority,
not 12 copies) and prevents future drift — same reasoning as `lorStore_` / `random_`.

## Interim
Per-panel bumps are OK as stopgaps (e.g. Straits light waves bumped to read), but flag each as
"superseded when the shared palette lands." Don't invest heavily in per-panel light tuning that the
shared-palette project will redo.

## Status
Backlog. Do after the current panel-art pass (Changi/Straits/Causeway) when there's appetite for a
cross-panel palette project. Container-verifiable throughout (cairosvg renders).
