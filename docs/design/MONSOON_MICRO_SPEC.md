# Monsoon Micro -- tuning/scale expanders (design, post-library)

Referenced by MICROTONAL_MASTER.md. Firms up the previously-vague "12/24 fixed-fader variants".
Reference model: VCV Scalar (vcvrack.com/Scalar) -- adopt its proven control surface.

## Feature set (from Scalar, adapted)
Adopt Scalar's core: per-degree CENTS tuning + per-degree ENABLE/DISABLE + Scala .scl import/export.
- CENTS per degree: 1/1200 octave. Root degree always 0 cents, uneditable (Scalar rule).
- ENABLE/DISABLE per degree: the N-bit scale mask (disabled = skipped/quantised away).
- Equal vs Unequal tuning; NOTES = degree count (fixed per variant: 12 or 24).
- OCTAVES setting: enable/disable state shared across octaves vs per-octave (Scalar has both; default shared).
- Scala .scl READ + WRITE. KEY POINT: .scl carries BOTH tuning (cents) AND scale (which degrees) --
  role-agnostic (see SCALES_AND_QUANTIZER_TODO). So one import/export mechanism gives tuning+scale
  storage. Author on faders -> SAVE to .scl from Monsoon.

## Two variants, expanders only: Monsoon Micro 12 + Monsoon Micro 24
Two FIXED-fader panels (12-tone, 24-tone) -- not a variable-N resizing module. Matches Scalar's 1-24
range at two sensible fixed points. Fixed panels = far simpler to build + reason about.

## CENTS control = ONE dial + edit-mode selection (Scalar's model, NOT per-fader)
Per-degree cents dials are panel-impossible (esp. 24). Adopt Scalar: ONE CENTS dial that tunes the
CURRENTLY-SELECTED degree. EDIT MODE: tap a fader/degree to select it, the single cents dial tunes that
degree. Fader = degree on/off + level; cents dial (edit mode) = fine-tune the selected degree.
[CONFIRM -- RODNEY]: one shared cents dial + selection (Scalar model) is assumed. vs per-fader cents (rejected: impossible at 24).

## Authoring vs consuming split
- MONSOON MICRO (this doc): AUTHORING. Dial cents, toggle degrees, SAVE to .scl from Monsoon. The
  tuning/scale editor.
- SHOPHOUSE (micro): CONSUMING -- import/display/modulation. Loads a tuning, shows it, modulates within
  it. Not the primary author. (See SHOPHOUSE_SPEC.)
Clean separation: Micro writes tunings; Shophouse reads/modulates them.

## DELEGATION RULE (the key architectural decision)
- Only ONE Micro may be attached at a time.
- When a Micro IS attached: Monsoon's own main faders BLANK OUT, and tone/tuning authority DELEGATES to
  the expander. Exactly one owner of the tuning at any time -- Monsoon's 12 faders (no Micro) OR the
  Micro (attached), never both. (Same single-owner discipline as the offset "which Raffles" resolution.)
- The blank-out is the visual signal that authority has moved.

### The 12 vs 24 blank-out asymmetry [note for implementation]
- Micro-12: clean 1:1 -- Monsoon's 12 faders blank, the Micro's 12 faders take over (direct correspondence).
- Micro-24: Monsoon has only 12 faders; they blank ENTIRELY and the 24-tone authority lives WHOLLY on the
  expander (no 1:1 map -- Monsoon's faders can't represent 24 tones, so they just go dark). This sidesteps
  the .kbm degree->fader mapping problem by giving the 24 its own dedicated 24-fader panel.

## Open / to decide (post-library)
- [CONFIRM] one cents dial + edit-mode selection (assumed).
- Save-from-Monsoon UX: context menu export, or a panel SAVE control?
- Does the Micro also quantise incoming CV (like Scalar's QN outs), or only DEFINE the tuning the Monsoon
  engine uses? (Scalar is a quantiser; Micro might be a tuning-DEFINITION expander feeding Monsoon's pitch
  gen rather than a standalone quantiser. Decide the data flow: Micro -> Monsoon engine tuning table.)
- Humanization (Scalar's depth/track/warp/offset): adopt any? Offset overlaps the seed-offset work
  (different thing -- Scalar offset = pitch transpose). Probably skip humanization for v1.
- Modulation on Shophouse-micro: what's modulatable (which degrees active? cents? -- ties to
  scales-within-tunings live modulation).
