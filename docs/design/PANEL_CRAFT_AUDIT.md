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
8. **Every jack legible by ROLE; mixed-direction modules additionally separated by REGION.** Two parts,
   different scopes (Rodney):
   - **Role appearance — UNIVERSAL, the higher-leverage half.** Every jack, on every module, looks like
     an input or an output by APPEARANCE (a direction token: colour/ring differing by role), so a glance
     across a rack tells sources from sinks and consumers from producers without tracing cables or
     reading labels. This applies even to modules that are ALL inputs or ALL outputs — many expanders
     are — where there is nothing to "separate" but the module should still read unambiguously as a
     consumer or a producer (a useful cross-module cue in a Monsoon+expanders chain).
   - **Region separation — ONLY for MIXED-direction modules.** When a module has BOTH, group ins and
     outs into distinct regions with a clear divide; signal conventionally reads in->out
     left-to-right / top-to-bottom. Do NOT impose an in/out split on a single-direction module — there
     is nothing to split, and forcing one is worse than leaving it.
   State of the suite: role appearance is MISSING everywhere (GoldPolyPort is one style regardless of
   direction). Region separation is already CORRECT on Monsoon (bottom two rows: every jack x<=100 is an
   input, x>=114 an output, clean gap between — do not relayout it; it needs the appearance token and a
   slightly wider/marked divide at the 100->114 gutter so the split reads as a split, not just the next
   column) and on CA's expression pairs (ins LEFT, outs RIGHT, ROW-aligned so the row says "this in maps
   to this out" — the reference pattern).

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
   background, accent rules, logo and subtitle in the house style, then offers `p.knob(...)`,
   `p.group(...)`, `p.label_for(id)`, and — enforcing principle 8 — role-aware `p.input(...)` /
   `p.output(...)` that place ins and outs in distinct regions with the correct role appearance by
   default, so a module cannot accidentally mix them. All snapping to the shared grid and pulling
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

---

## Second workstream: C++ components (the audit above under-weighted this)

The generator draws the static WELL (ring/recess in the SVG); the widget places the interactive
COMPONENT on top (knob cap, port, light). Both carry design and both must agree, so "consistent look"
is a Python AND a C++ job.

### Finding: most interactive parts are STOCK Rack components
Measured `create*` usage: **Trimpot ×44, TL1105 ×25**, VCVSlider ×3, a couple of lights — against only a
few custom classes (`ThemedKnob`, `ScrubKnob`, `StoreKnob`, `GoldPolyPort`, `ConnectMark`,
`DimmableTrimpot`, all in `src/ui/`). So most knobs and buttons are Rack DEFAULTS. That is a large part
of why the modules do not read as one family — the wells are becoming consistent while the caps on top
are generic. Cover the panel art and the controls alone would not tell you it is one maker.

FIX: a small house component set in `src/ui/`, used everywhere:
- one `DotKnob` (with the bipolar/centre-detent variant), replacing bare `Trimpot`;
- one `DotButton`, replacing bare `TL1105`;
- **Two port ROLES, visually distinct**: an input port and an output port style (or one shape with a
  direction token — colour/ring — differing by role), so ins and outs are distinguishable by appearance
  everywhere, not only by position (principle 8). `GoldPolyPort` is currently one style regardless of
  direction; split it or tokenise the ring.
- `ConnectMark`, `DimmableTrimpot` already exist — fold them into the set.
Each reads the SAME tokens the generator uses (see below), so a knob cap and its well are sized and
coloured from one source. Changing the house knob then propagates everywhere — the C++ equivalent of a
design token.

### The JOIN: shared sizing constants both sides read
A well drawn at radius R (generator) and a cap sized for R' (C++) look wrong together. Today the
generator has radii in `dotmod_design.py` and each widget hardcodes its own. FIX: the sizing constants
(jack radius, knob radius, trim radius, light radius, label size, grid pitch) live in ONE place both
sides consume — export them from `dotmod_design.py` into a generated `src/ui/PanelTokens.hpp` (or a
hand-kept header the generator asserts against), so Python and C++ cannot disagree.

### Labels: BAKE INTO THE SVG (decided)
Runtime `nvgText` label draws are widespread — Intertropical 13, Lantern 12, Interchange 12, MicroTuning
10, Macro 8, Monsoon 7, and more. That is where much of the font inconsistency lives (each widget picks
its own face and size) and a source of label/control drift.
DECISION: **static control labels are baked into the SVG by the generator**, from the shared label
helper and font tokens — one place emits all label text, so font and size are consistent by
construction and a label cannot drift from its control. This is what most polished Rack plugins do.
Runtime `nvgText` is reserved for genuinely DYNAMIC strings only: live numeric readouts (BPM, seed),
a selected-scale name, a mode/status string. Everything static moves into the generator.
Consequence: as each module migrates onto the Panel builder, its static `nvgText` calls are deleted and
re-emitted as SVG `<text>` at anchor-relative positions.

### Order within this workstream
1. Define the house component set in `src/ui/` and the shared `PanelTokens.hpp` join.
2. Swap stock `Trimpot`/`TL1105` for the house classes module by module, alongside that module's Panel
   builder migration (do the SVG and C++ passes for a module together, not in separate sweeps — they
   share the tokens and must land consistent).
3. Move static labels SVG-side as each module migrates.
Verify each module with panel_diff (SVG) AND a Rack load (components) before moving on.

---

## Build hygiene: one scale, one gen-all, tokens as a generated artefact (Rodney)

### PanelTokens.hpp should be GENERATED, not hand-kept
`dotmod_design.py` is the single source of the token VALUES; the C++ header is a derived artefact.
So a small `gen_tokens.py` emits `src/ui/PanelTokens.hpp` (a `// GENERATED — do not edit` constexpr
header) from the same Python constants the generators use. Python and C++ then cannot disagree by
construction, and the earlier "assert against a hand-kept header" idea is unnecessary. Regenerating
tokens is part of the gen-all run below.

### One scale, one meaning for `S` (this is the "inconsistent mm" root cause)
Surveyed: `S` currently means FIVE different things across generators — `75/25.4` (correct: px per mm
at Rack's 75 dpi), `75`, `8`, `3.7795` (96-dpi px/mm), `767.99`, and more. Some generators draw in mm,
some in raw px, some at 96 dpi. There is no shared definition of what a coordinate MEANS, which is
exactly why mm usage feels inconsistent — it IS inconsistent.
FIX: **all geometry is authored in millimetres; the ONLY scale is `S = 75/25.4` (px per mm), defined
once in `dotmod_design.py` and imported.** No generator defines its own `S`. `px(mm)` is the single
converter. Legacy 96-dpi art (the Monsoon supertrees are the known case) is converted once to mm and
then never sees 96 again — the reverse-engineer already did this for Monsoon; apply the same to any
other 96-dpi holdouts. This is a precondition for the Panel builder: a shared grid is meaningless if
generators disagree on the size of a millimetre.

### A gen-all script
There is no build-all today, so regenerating the suite is manual and error-prone (and it is how the
two-generator clashes like gen_layout-vs-gen_raffles go unnoticed). Add `panel_src/gen_all.sh` (or a
Makefile target) that:
  1. runs `gen_tokens.py` -> `src/ui/PanelTokens.hpp`;
  2. runs every ACTIVE generator in the correct order (art generators LAST where a layout generator
     also writes the same SVG — the gen_raffles caveat), driven by the panel_src/README.md table so
     there is one authoritative list;
  3. optionally runs panel_diff against the committed SVGs and fails if anything moved unexpectedly
     (a regression guard for the whole suite, like run_all.sh for tests).
Retiring the graveyard generators (abstraction #5) is a precondition — gen-all must run only the
active set, so the dead ones have to go or be clearly quarantined.

### Elements vanishing at maximum zoom (a real rendering bug, not just craft)
Some graphical elements disappear at maximum zoom. Likely causes, in order of probability:
  1. **Sub-pixel stroke widths.** A `stroke-width` that is a fraction of a mm can round to <1 device
     pixel and drop out; at extreme zoom the rounding flips. FIX: floor hairline strokes to a minimum
     mm width, and define stroke widths as tokens (`HAIRLINE`, `RULE`, `HEAVY`) rather than ad-hoc
     small numbers.
  2. **nanosvg vs the zoomed renderer disagreeing** on very thin or very low-opacity shapes — the
     0.78125-wrap and mixed-scale coordinates make some strokes land on non-integer positions that
     vanish at some zooms.
  3. **Opacity stacking** so low that a shape is invisible until composited at a particular scale.
Diagnose by rendering the offending panel at several zoom levels (the nsvgrender tool in
panel_src/tools/ + a scale sweep) and finding which elements fall below ~1px. This should be its own
small investigation; note the specific modules/elements when found. It matters more than pure craft
because it is visible breakage, not just inconsistency.
