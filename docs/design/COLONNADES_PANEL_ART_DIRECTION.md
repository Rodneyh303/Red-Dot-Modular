# Colonnades / Duo panel-art direction -- Singapore brutalism + honeycomb (inspiration, NOT a rebuild)

Panel-art DIRECTION for the two microtonal fader modules, grounded in two real Singapore buildings whose
names AND structure the modules already echo. This is design intent to inform the EXISTING generators
(gen_colonnades.py), NOT a from-scratch panel spec -- do not restart the panel. Abstract the
architectural LANGUAGE into dot.modular's own vocabulary; do not trace a facade.

## The two buildings (why they fit uncannily well)

### Colonnades -> "The Colonnade" (Paul Rudolph, 1986, 82 Grange Road)
- Literally named The Colonnade -- the module name IS the building, not a loose allusion.
- Rudolph's core idea: repeated prefab units, the "twentieth-century brick" -- modular units stacked
  into a whole. That is EXACTLY a dot.modular module: a repeated cell (fader/degree) tiled into a
  structure. Same thesis, in concrete.
- Form: four rectangular quadrants lifted on TWO ROWS OF CLOSELY-SPACED COLUMNS, at staggered heights
  -> the famous "zigzag" look. Closely-spaced columns + repetition + stagger = the fader bank and the
  staggered cent-knob rows, already present in the panel.
- Brutalist: pour-in-place concrete, orthogonal, "a stack of houses" (rhymes with the Shophouse motif).

### Colonnades Duo -> "DUO" (Buro Ole Scheeren, 2017, Bugis / Kampong Glam)
- Literally named DUO, TWIN TOWERS -- Colonnades Duo is literally two Colonnades. Name + "two of the
  same" structure match the twin-tower form.
- Signature: a HEXAGONAL HONEYCOMB facade -- repeated hexagonal sunshade cells tiled across the curved
  towers. A repeated unit forming a field: the module's visual grammar, but HEXAGONS not rectangles.
- Same Singapore-geography naming vein as the collection (Esplanade / Shophouse / Raffles / Changi);
  DUO sits by Bugis Junction in Kampong Glam.

## The distinction this encodes (the useful part)
Both buildings are "a repeated modular unit forms a whole" -- the dot.modular thesis. But:
- **Colonnades = brutalist RECTILINEAR repetition** (columns + concrete blocks, orthogonal, stagger).
- **Colonnades Duo = HEXAGONAL honeycomb repetition** (hex cells, twin-form, curved field).
So the 12->24 doubling gets a VISUAL analog: rectangles -> hexagons, single -> twin. The Duo reads as
distinct-but-related: same grammar (tiled cells), different cell geometry. This is how the Duo can look
like "two Colonnades" (COLONNADES_DUO_PANEL_SPEC, same generator N=24) AND carry its own identity (the
honeycomb texture as an overlay/accent), without being a different panel.

## Direction for Colonnades (brutalist column-and-block)
- Lean into the column-and-block language: panel structure as stacked concrete quadrants on
  closely-spaced columns; the FADER BANK reads as the colonnade of columns; the staggered cent-knob
  rows as the zigzag stagger of the lifted quadrants (which the round-7 staggered grid already does).
- Palette: concrete greys, board-formed-concrete texture hints, with the dot.modular Singapore red
  (#d4001a) as the single accent. Brutalist = weight, shadow, orthogonal repetition.
- This is mostly a RE-SKIN/texture pass over the existing gen_colonnades geometry, not a relayout.

## Direction for Colonnades Duo (DUO honeycomb)
- The hexagonal honeycomb as the defining texture -- the thing that makes the Duo instantly its own.
- Options (least to most invasive, all as ACCENTS over the existing N=24 generator, pick one):
  1. A honeycomb frame/border around the fader bank (hex-cell trim), everything else as Colonnades.
  2. The enable-band / number-strip cells drawn as HEXAGONS instead of rectangles (functional hex =
     the per-degree cell, echoing the sunshade-cell metaphor).
  3. A subtle hex-mesh texture behind the fader bank (the honeycomb "skin"), low contrast.
- Twin-form: the two-Colonnades-side-by-side reading (COLONNADES_DUO_PANEL_SPEC) already gives the
  twin-tower echo; the honeycomb accent is what layers DUO's identity on top.
- Keep it an ACCENT: the Duo must still read as two Colonnades (same faders/knobs/grid at N=24). The
  honeycomb is texture/framing, not a relayout.

## GUARDRAIL (learned from the panel dances): abstract the language, don't trace the building
- These are INSPIRATION, not literal depiction. Do NOT copy a recognizable facade -- both buildings are
  recent (Rudolph estate-held; Scheeren living), so a traced facade is both an IP question and a
  drift-magnet. Abstract the LANGUAGE (brutalist rectilinear repetition; hexagonal honeycomb
  repetition) into the panel's own vocabulary, the way the Shophouse panel evokes a shophouse without
  copying one specific building.
- This doc informs the EXISTING generators as a texture/accent/palette pass. It does NOT authorise a
  from-scratch panel or a relayout. The geometry (fader pitch, stagger, grid, N-parameterisation) is
  settled (rounds 7/10 + DUO_PANEL_SPEC); this is the finish over that geometry.
- If a change would move a fader/knob/grid position, it's OUT of scope for this doc -- that's geometry,
  already frozen. In scope: palette, texture, framing, cell-shape (rect vs hex), accents.

## Sequencing
Post the functional work (enabled/N build, the dim fix verify, the missing-header commit). Panel finish
is a LATE polish pass -- the modules must WORK first. Captured now so the direction is set before the
finish pass, not to trigger it now.

Cross-ref: COLONNADES_PANEL_LIFT_SPEC.md (geometry, rounds 7/10), COLONNADES_DUO_PANEL_SPEC.md (N=24
same generator), gen_colonnades.py (the generator to re-skin, not restart), the dot.modular brand
(#d4001a, "little red dot") in the logo/brand docs.
