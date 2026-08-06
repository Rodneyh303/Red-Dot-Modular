# Monsoon Micro -- tuning/scale expanders (design, post-library)

Referenced by MICROTONAL_MASTER.md. Firms up the previously-vague "12/24 fixed-fader variants".
Reference model: VCV Scalar (vcvrack.com/Scalar) -- adopt its proven control surface.

## Feature set (from Scalar, adapted)
Adopt Scalar's core: per-degree CENTS tuning + per-degree ENABLE/DISABLE + Scala .scl import/export.
- CENTS per degree: 1/1200 octave. Root degree always 0 cents, uneditable (Scalar rule).
- ENABLE/DISABLE per degree: the N-bit scale mask (disabled = skipped/quantised away).
- Equal vs Unequal tuning; NOTES = degree count (fixed per variant: 12 or 24).
- OCTAVES: OCTAVE-INVARIANT -- same degrees on/off in EVERY octave (NOT Scalar's per-octave variable
  option). Forced by the architecture: the fader bank represents ONE octave's degrees repeated across
  octaves; there's no per-octave fader bank, so per-octave variation has no natural control surface.
  Also matches normal scale thinking (a scale is the same shape each octave). A simplification, and the
  architecturally-correct fit -- not a compromise.
- Scala .scl READ + WRITE. KEY POINT: .scl carries BOTH tuning (cents) AND scale (which degrees) --
  role-agnostic (see SCALES_AND_QUANTIZER_TODO). So one import/export mechanism gives tuning+scale
  storage. Author on faders -> SAVE to .scl from Monsoon.

## Two variants, expanders only: Monsoon Micro 12 + Monsoon Micro 24
Two FIXED-fader panels (12-tone, 24-tone) -- not a variable-N resizing module. Matches Scalar's 1-24
range at two sensible fixed points. Fixed panels = far simpler to build + reason about.

## CENTS control = PER-FADER knob (DECIDED -- better than Scalar's single dial)
Each degree gets its OWN cents knob, next to its fader. EQUAL-DIVISION DEFAULT (each degree starts at its
equal-tempered cents, like Scalar's default); drag to detune. This is a DEDICATED expander whose whole
job is these degrees, so it has the panel room Scalar lacks -- Scalar uses one dial only because it's a
cramped standard-width quantiser. Per-fader is strictly better UX: every degree's tuning visible +
directly adjustable, no edit-mode select-then-dial indirection. (Took Scalar's good part -- equal-division
default, per-degree cents -- and dropped the single-dial constraint that was only there for space.)
Per-degree strip = FADER (on/off + level) + CENTS knob + (enable toggle). 24 such strips = a big but fine
expander panel (~30-36HP for the 24; the 12 is comfortable). Layout: vertical strips, note label per strip.

## MICRO-24 LAYOUT: ONE ROW of 24 (DECIDED), not two rows of 12
DECISION: one row of 24 faders/strips, accept the width. Reasons:
- HONEST TO THE DATA: the 24 degrees are ONE linear ascending sequence (low->high within the octave). One
  row = left-to-right is monotonic pitch order; any degree's position encodes its pitch height; the scale
  SHAPE is legible at a glance (like a keyboard / spectrum). Two rows break monotonicity (degree 13 sits
  spatially below/left of degree 1 but is higher in pitch).
- DECIDING REASON -- arbitrary .scl support: because we import ARBITRARY 24-note Scala tunings, the 24
  degrees may NOT decompose into any 12+12 structure (a true 24-EDO, or 24 unequal steps, has no
  "primary vs quarter-tone" pairing). One row of 24 is the ONLY layout honest to ALL possible 24-tone
  tunings. A "top=12 primary / bottom=12 quarter-tone" split would BAKE IN the 12+quarter-tone
  interpretation and MISLEAD for any tuning that isn't that -- unacceptable given .scl import.
- Matches Scalar's single-row model (users of this module class already know it).
- Cost: wide (~30-36HP). It's a DEDICATED expander, so the width is acceptable; it buys a representation
  correct for every tuning.

FALLBACK (only if a target rack truly can't take the width): two rows as a PLAIN FOLD -- row1 = degrees
1-12, row2 = degrees 13-24 (pitch order preserved within each row). NOT a primary/inflection split (that
lies about non-quarter-tone tunings). One row is strongly preferred; the fold is a last resort.

Micro-12: one row of 12, comfortable, no question.

## Authoring vs consuming split
- MONSOON MICRO (this doc): AUTHORING. Dial cents, toggle degrees, SAVE to .scl from Monsoon. The
  tuning/scale editor.
- SHOPHOUSE (micro): CONSUMING -- import/display/modulation. Loads a tuning, shows it, modulates within
  it. Not the primary author. (See SHOPHOUSE_SPEC.)
Clean separation: Micro writes tunings; Shophouse reads/modulates them.

## DELEGATION RULE (the key architectural decision)
- Each Monsoon accepts AT MOST ONE Micro -- either a Micro-12 OR a Micro-24, NEVER both, NEVER two of
  the same kind. Three mutually exclusive tuning-source states per Monsoon:
    (a) no Micro attached  -> Monsoon's own 12 faders (built-in 12-TET default)
    (b) Micro-12 attached  -> Micro-12's 12 faders (custom 12-tone tuning)
    (c) Micro-24 attached  -> Micro-24's 24 faders (24-tone tuning)
- When ANY Micro IS attached: Monsoon's own main faders BLANK OUT + tone/tuning authority DELEGATES to
  the expander. The blank-out is the visual signal that authority has moved. Same single-owner discipline
  as everywhere in the design (which-Raffles, shared-CA-writer, WriteLedger).

### Enforcement: multiple Micros attempted -- use the existing ConnectMark (Rodney)
If more than one Micro is attached in the expander chain: FIRST FOUND WINS + rest are VISIBLY not claimed
by any Monsoon. Use the existing branded connection indicator, src/ui/ConnectMark.hpp -- the dot.modular
mark that shows full-colour when connected/claimed, greyed/faded when not. Wire the Micro's mark to
isConnectedAndClaimed(module): the authoritative Micro's mark is bright; the rejected Micro's mark greys
out naturally. NO ad-hoc "inert indicator" needed -- reuse the branded expander connection semantics.
- The authoritative Micro: mark = full colour ("claimed").
- Any rejected/extra Micro: mark = greyed ("not claimed by any Monsoon"), which is the truth from that
  Micro's perspective.
Same compositional principle at the UI layer: don't invent a "Micro-specific not-authoritative" affordance
when the existing branded connection indicator carries the right meaning. Users already read this mark
across the module family; it says exactly what needs saying here.
The rest of the rejected Micro's panel: faders/knobs visible (state preserved for future reactivation)
but INERT because it isn't the tuning source. User sees the greyed mark, understands why, rearranges.

### Micro without a Monsoon (standalone) -- ConnectMark greys, panel inert
A Micro with no paired Monsoon: ConnectMark greys (no Monsoon to claim it), panel inert. It's a tuning-
DEFINITION expander with no purpose without a Monsoon to feed. Consistent with how the ConnectMark is
already used elsewhere in the module family -- greyed mark = 'not connected to a host, not doing
anything'. No standalone mode; the greyed mark tells the user why.

### Interaction with Interchange pairing (see MICRO_TUNING_INTEGRATION_PLAN)
The one-Micro-per-Monsoon rule keeps the Interchange story clean: one Monsoon -> one Micro-24 -> two
Interchanges cooperatively modulate its 24 faders (pair number + half selector). If you have two Monsoons
each with their own Micro-24, that's four Interchanges total, each pair-numbered to its target Micro.
The pairing tech scales naturally; this rule just clarifies the Monsoon-side count (one Micro each).

### The 12 vs 24 blank-out asymmetry [note for implementation]
- Micro-12: clean 1:1 -- Monsoon's 12 faders blank, the Micro's 12 faders take over (direct correspondence).
- Micro-24: Monsoon has only 12 faders; they blank ENTIRELY and the 24-tone authority lives WHOLLY on the
  expander (no 1:1 map -- Monsoon's faders can't represent 24 tones, so they just go dark). This sidesteps
  the .kbm degree->fader mapping problem by giving the 24 its own dedicated 24-fader panel.

## Open / to decide (post-library)
- Save-from-Monsoon UX: context menu export, or a panel SAVE control?
- Does the Micro also quantise incoming CV (like Scalar's QN outs), or only DEFINE the tuning the Monsoon
  engine uses? (Scalar is a quantiser; Micro might be a tuning-DEFINITION expander feeding Monsoon's pitch
  gen rather than a standalone quantiser. Decide the data flow: Micro -> Monsoon engine tuning table.)
- Humanization (Scalar's depth/track/warp/offset): adopt any? Offset overlaps the seed-offset work
  (different thing -- Scalar offset = pitch transpose). Probably skip humanization for v1.
- Modulation on Shophouse-micro: what's modulatable (which degrees active? cents? -- ties to
  scales-within-tunings live modulation).
