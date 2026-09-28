# Panel craft audit — look, feel, and the abstractions to get there (Rodney)

STATUS: review + plan. Feature vision is near-complete; this is the CRAFT pass. Written because the
suite is functionally strong but visually disparate — different eras of panel, six font families, a
shared design module (`dotmod_design.py`) that the OLD modules predate, art (Monsoon's supertrees)
buried under controls, Sands lane colours disturbed by the q-mix insertion, a washed-out light theme,
space lavish in places and cramped in others, and inconsistent SvgPanelKit adoption.

## Design principles this audit applies
1. **One visual system, applied everywhere.** A plugin should read as one instrument family. The test:
   cover the logo — can you still tell it is the same maker? Right now, often no.
2. **Hierarchy before decoration.** The eye should land on the primary control first, groups second,
   art last. Art is the BACKGROUND the controls sit on, never competing with them for attention.
3. **Alignment and a grid.** Controls on a shared mm grid with consistent row/column pitch read as
   intentional; ad-hoc coordinates read as noise even when each is individually fine.
4. **Consistent component vocabulary.** A jack looks like a jack, a bipolar knob like a bipolar knob,
   everywhere. The Big-Six / Straits / Sands knob unification is exactly this and is the model to
   extend.
5. **Label economy.** Every label is a small failure of self-evidence; group and position so fewer are
   needed, and when needed keep them one size, one font, one colour role.
6. **Contrast that survives both themes.** Dark and light are not inversions; each needs its own tuned
   contrast. Pastel-on-pale washes out; a light theme needs DARKER ink and stronger control outlines
   than a naive invert gives.
7. **Breathing room proportional to importance**, not to whatever space was free. Even margins; group
   gaps larger than intra-group gaps; the hero control gets the most air.

## Per-module state (dark-panel widths; ✓ on kit = bind-by-name, ✗ = hardcoded mm)
| Module | HP | Art | Kit | Notable craft issue |
|---|---|---|---|---|
| Monsoon (main) | 45 | supertrees, Flyer, logo | ~ (21 mm2px left) | art buried under dense controls; fought for space twice |
| Change Alley V2 | 60 | ticket fan | ✓ | large; connect-mark row + labels recently reworked |
| Intertropical | 47 | scene grid | ✗ (12 mm2px) | grid geometry hardcoded; functional, sparse |
| Lantern | 41 | — | ✗ (0 kit) | NO kit at all; bare |
| Colonnades / Duo | 22 / 44 | — | ~ | two sizes of one idea; generators not unified |
| Straits | 34 | — | ~ | q-mix knobs added; bulk-set menu pending |
| Causeway | 32 | — | ~ | q-mix lane added; dimmable per-voice knobs |
| Shophouse / Micro | 26 | — | ~ | functional, bare |
| Keppel | 14 | — | ✓ (just migrated) | newly widened for MPE; bare but consistent |
| Raffles | 18 | ticket fan | ✓ | just rebuilt to 3 columns; labels still off |
| Changi T1/2/3 | 24/20/28 | — | ~ | family, but three separate looks |
| Sikit / Junction / Interchange | 8 | — | ~ / ✗ | small utilities; Interchange 0 kit |
| Sands (Mono/East/Macro) | 8 | helix | ✓ (audited) | LANE COLOURS disturbed by q-mix; see below |

## The specific problems, with fixes

### A. No single applied system
`dotmod_design.py` IS the system (theme tokens, logo, jacks, trims, groups, editor recess, motif
waves, kit shapes) — but only the newer generators import it. Older panels hardcode their own colours
and shapes. FIX: make `dotmod_design` the ONLY source of colours, component shapes, fonts and spacing
constants, and migrate every generator onto it. Nothing new to invent — it exists; it is not
universally used.

### B. Fonts — six families in play
Measured: monospace (1067), sans-serif (110), Barlow (105), Helvetica Neue (55), Eurostile (32),
Arial (16), plus DejaVu/DSEG7 in widgets. FIX: pick TWO — a display face for the wordmark/subtitle
(Barlow is already the logo face) and one UI face for labels (choose one, e.g. Eurostile or Inter, and
kill the rest). Put both in `dotmod_design` as tokens (`FONT_DISPLAY`, `FONT_LABEL`) and a single
label helper so no generator writes a raw font-family again. Monospace is fine ONLY for numeric
readouts (BPM/seed); everywhere else it is an accident.

### C. Art buried (Monsoon)
The supertrees/Flyer are drawn at full strength behind a dense control field, so they read as clutter
rather than backdrop. FIX: push background art DOWN in contrast (lower opacity, desaturate) so it sits
clearly behind controls; give the hero art ONE area where it is unobstructed (Monsoon's top band works)
and let it fade under the control zones. Principle 2: art is backdrop, not competitor.

### D. Sands lane colours disturbed by q-mix
Inserting q-mix at index 2 shifted the lane palette. FIX: define the lane colours as a NAMED MAP in
`dotmod_design` keyed by lane MEANING (rhythm/melody/qmix/rest/accent/var/leg), not by index, so
inserting a lane never reshuffles the others — the same lesson as the lane-index bug, applied to colour.

### E. Light theme washes out
The light tokens are close to a naive invert (bg #e8e8ea, ink #2a2a2e) and pastels on pale lose contrast.
FIX: darken ink and strengthen control outlines specifically for light; treat light as its own tuned
palette, not `1 - dark`. Test every control at 100% zoom on a white rack rail.

### F. Space use uneven
Some panels lavish (Lantern 41HP, sparse), some cramped (label collisions on the 8HP utilities). FIX: a
shared spacing scale in `dotmod_design` (margin, group gap, row pitch, jack pitch as named constants),
and size panels to content + that scale rather than to a round HP number. The cramped-label problem is
usually a missing grid, not a missing millimetre.

### G. Inconsistent kit adoption
Per PANEL_KIT_CONSISTENCY_AUDIT.md — 5 of 19 migrated, Lantern and Interchange on none. Craft depends on
this: labels can only be derived from anchors (so they never drift) once the module is on the kit.

## Coding abstractions that make craft EASIER (the real leverage)
Craft is hard right now because each panel is hand-geometried. The fixes above mostly reduce to one
move: **turn `dotmod_design.py` from a helper library into a small PANEL FRAMEWORK.** Concretely:

1. **A `Panel` builder object.** `p = Panel(hp=..., theme=...)` that owns the mm grid and emits the
   background, accent rules, logo and subtitle in the house style, then offers `p.jack(row,col,id)`,
   `p.knob(...)`, `p.group(...)`, `p.label_for(id)` — all snapping to the shared grid and pulling
   colours/fonts/spacing from tokens. A new module becomes a layout table, not an SVG.
2. **Named design tokens** (colours already exist; ADD `FONT_DISPLAY/FONT_LABEL`, `MARGIN`,
   `GROUP_GAP`, `ROW_PITCH`, `JACK_PITCH`, `LABEL_SIZE`, and the lane-colour map by meaning).
3. **Components layer emitted by the builder**, so kit adoption is automatic — you cannot place a
   control without also emitting its anchor, which structurally ends the mm2px drift.
4. **One label helper** with a `dy` convention and anchor-relative placement, killing per-generator
   font/coordinate choices.
5. **Retire the graveyard**: `_GENERATED`, `_peranakan`, `Temasek`, `ChangeAlley` (V1), the many
   `interchange_*`/`straits_*` experiments, and the two-generator clashes (gen_layout vs gen_*). A
   clean `panel_src` is a precondition for a clean look — you cannot audit 40 SVGs.

## Suggested order (craft is iterative; do the framework first)
1. Extend `dotmod_design` to the full token set + `Panel` builder (no visual change yet; prove it by
   regenerating one already-good panel — Keppel — to byte-parity via panel_diff).
2. Lane-colour map by meaning; fix Sands (D).
3. Font consolidation (B) across all live generators.
4. Light-theme re-tune (E) with a side-by-side render harness.
5. Migrate the bare/no-kit modules onto the builder (Lantern, Interchange first — greenfield).
6. Monsoon art contrast pass (C) — the highest-visibility single improvement.
7. Retire the graveyard (abstraction #5).

NONE of this changes behaviour — it is all generator/SVG/token work, verifiable with panel_diff, and
orthogonal to the feature roadmap (connection rework, mode collapse, crab canon).
