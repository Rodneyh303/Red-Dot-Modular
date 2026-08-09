# Colonnades (Micro-12) panel -- lift-and-shift from Monsoon note faders

Rodney's direction: the Micro-12 panel should read as a MONSOON FAMILY MEMBER, not a new visual
language. It is a lift-and-shift of the Monsoon note-fader block, widened, with the cents knobs
staggered below. Consistency with Monsoon is the goal.

## What to carry over from Monsoon (exact geometry)

### The 12 note faders -- SAME spacing as Monsoon
Monsoon note faders (embed_monsoon.py:6): `SEMI{i}_PARAM` at X = `7.5 + i*9.0`, Y = 59.75, for
i in 0..11. The **9.0mm horizontal pitch** is the consistency anchor -- Colonnades faders must use
the same 9.0mm pitch so the two panels visually rhyme. Do not invent a new spacing.

- 12 faders, 9.0mm pitch, same fader track/handle styling as Monsoon.
- Widen the panel to accommodate: 12 faders * 9.0mm + margins. Monsoon fits them in its lower block;
  Colonnades gives them the whole width, so it will be wider than CC's current 14HP. Size to the
  faders at 9.0mm pitch plus margins (~16-18HP likely; let the fader math set it, not a guessed HP).

### Numbered, NOT note-labelled (Rodney)
Monsoon labels its faders with note names (C, C#, D...). Colonnades faders are NUMBERED 1..12 like
the Monsoon STEP numbers (embed_monsoon.py has the `1 2 3 ... 12` strip under the faders), NOT
note-labelled -- because in an arbitrary tuning the degrees are not notes. Use the same numbering
style/position as Monsoon's step-number strip.

### Light sliders that light when the note is on (Rodney)
Monsoon uses `MonsoonLightSlider : VCVLightSlider<TLightBase>` (MonsoonWidget.cpp:33) -- a light
slider that reflects note-on state. Lift this pattern:
- Widget: a `ColonnadesLightSlider : VCVLightSlider<TLightBase>` mirroring MonsoonLightSlider.
- Binding: `bindLightParamsContiguous<...>` (SvgPanelKit.hpp:246 idiom) over `param_weight_0..11`.
- Lighting source: Monsoon lights the fader from note-on / scale-mask state
  (`m->modViz.pitchLane[sem]` at MonsoonWidget.cpp:74-75, and `semiOutOfScale` dims out-of-scale
  faders to 0.4 alpha at :57). Colonnades reads the SAME viz -- the Micro owns the weight[] mask now,
  so the light reflects which degree is active/sounding. Reuse the dim-out-of-scale idiom for
  disabled degrees (weight = 0 -> dimmed).

### Modulation arcs like Monsoon (Rodney)
Monsoon draws mod arcs via `ModArcOverlay.hpp` + `queueModArcLinear` / `flushModArcs`
(MonsoonWidget.cpp:146-169). The arcs show CV/Interchange modulation on top of each control. Lift
this for Colonnades:
- The Interchange expander modulates Colonnades faders (per MONSOON_MICRO_SPEC -- Interchange gains
  followTarget + targetHalf). So the mod arcs show Interchange modulation on the weight faders.
- Same queue/flush pattern: attach a ModArcOverlay to each fader after binding, wired to read the
  modulation amount. Copy the MonsoonWidget flushModArcs path.

## What is NEW (not lifted -- the cents knobs)

### Cents knobs staggered on two rows below the faders (Rodney)
Below the faders, the per-degree CENTS knobs. Rodney: "plenty of vertical range, stagger them on two
rows, alternating, below the faders." So:
- Even-index degrees (0,2,4,6,8,10) on one row, odd-index (1,3,5,7,9,11) on a row slightly below,
  alternating -- a zigzag. This gives each knob more horizontal room than a single tight row of 12
  would, and uses the vertical space Colonnades has.
- Each cents knob sits under its fader's X (so degree i's knob aligns with degree i's fader),
  just staggered in Y by even/odd.
- Root (degree 0) has NO cents knob -- locked at 0. CC's current panel already handles this with a
  "locked plate" (gen_monsoon_micro_12.py:79). Keep that; the root's slot in the stagger is the
  locked plate.
- Cents knob styling: same dot.modular knob family as Monsoon's Big-5 knobs (DMKnobs / the flat
  Befaco-style knobs), scaled down to fit 12 across in two staggered rows.

## What CC already has right (keep)
- The weight-fader-plus-cents-knob-per-degree concept (gen_monsoon_micro_12.py).
- Root cents locked, no knob, locked plate.
- The dark/light theme dicts, the dot.modular palette.
- `param_weight_<i>` and `param_cents_<i>` marker naming.
- ConnectMark (`light_connect`) for the claim indicator.

## What to change from CC's current attempt
1. WIDEN -- 12 faders at Monsoon's 9.0mm pitch, not squeezed into 14HP.
2. Faders NUMBERED 1..12, not the current layout -- match Monsoon's step-number strip style.
3. Faders become LIGHT SLIDERS (ColonnadesLightSlider), lit from the active-degree viz, dimmed when
   weight=0 -- lift MonsoonLightSlider.
4. Add MOD ARCS on the faders -- lift ModArcOverlay + queueModArcLinear/flushModArcs.
5. Cents knobs STAGGERED two-row zigzag below the faders, aligned in X to their faders.

## Cross-refs
- panel_src/embed_monsoon.py:6 -- the 9.0mm fader pitch to match.
- src/MonsoonWidget.cpp:33-90 -- MonsoonLightSlider (the light-slider + dim-out-of-scale to lift).
- src/MonsoonWidget.cpp:146-169 -- queueModArcLinear/flushModArcs (mod-arc pattern to lift).
- src/ui/ModArcOverlay.hpp -- the arc overlay widget.
- src/ui/SvgPanelKit.hpp:246 -- bindLightParamsContiguous idiom.
- MONSOON_MICRO_SPEC.md -- Micro-12 semantics (weight[] ownership, Interchange modulation).
- MONSOON_MICRO_CLAUDE_CODE_GUIDE.md -- the Micro build guide this panel serves.

## ROUND 2 CORRECTIONS (Rodney, from first CC render) -- exact values

The first render was a lookalike, not a true lift. Five specific gaps, each with the real number:

### 1. Light colour: GreenRedLight, grey-when-off -- NOT yellow
Monsoon binds `MonsoonLightSlider<GreenRedLight>` (MonsoonWidget.cpp:238). The fader body is GREY
when off and lights (red/green via GreenRedLight) when the note plays. CC's render used a YELLOW
fill -- wrong. Use the SAME light template:
```cpp
bindLightParam<ColonnadesLightSlider<GreenRedLight>>("param_weight_i", ...);
```
Grey base, GreenRedLight for on-state. Match Monsoon's exact fader body colour (the theme dict's
fadertrack/faderhandle, not a new yellow).

### 2. Level bars (tick columns) -- currently MISSING
Monsoon faders have level-marker TICKS: `fader_level_markers.py` emits one tick column per fader,
Befaco-Octaves style (long, solid, uniform weight, no major/minor). CC's render has bare tracks with
no ticks. Lift `fader_level_markers.py`'s tick emission for the 12 Colonnades faders -- single-sourced
from the fader position list so ticks can't drift from faders. This is the "level bars like Monsoon"
Rodney asked for.

### 3. Note numbers BELOW the sliders -- not above
Monsoon's 1..12 step-number strip sits BELOW the faders. CC put the numbers above. Move them below,
matching Monsoon's strip Y position and style.

### 4. Fader height/travel MUST align with Monsoon top-and-bottom
This is the key consistency fix. Monsoon fader travel (MonsoonWidget.hpp): SL_TOP = 45mm, travel
height SLH so bottom = 74.5mm, centre line 59.75mm. When Colonnades is placed ABOVE or BELOW Monsoon
in the rack, the faders should line up vertically. So Colonnades faders must use the SAME travel
geometry: top at 45mm, bottom at 74.5mm, centre 59.75mm, 29.5mm travel. Do NOT use a different fader
height -- match these exact values so the two panels' faders align when stacked.

### 5. (still) 9.0mm horizontal pitch, numbered not noted -- from round 1, keep

### Exact geometry summary (copy these)
- Fader X: 7.5 + i*9.0 mm, i = 0..11 (same as Monsoon SEMI faders).
- Fader travel Y: top 45mm, bottom 74.5mm, centre 59.75mm (SAME as Monsoon -- for stacking alignment).
- Fader light: ColonnadesLightSlider<GreenRedLight>, grey body when off (theme fadertrack/faderhandle).
- Level ticks: lift fader_level_markers.py per-fader tick column (Befaco-Octaves style).
- Numbers 1..12: BELOW the faders (Monsoon step-strip Y + style).
- Cents knobs: staggered two-row zigzag below the number strip (unchanged from round 1).

### Cross-refs (round 2)
- panel_src/fader_level_markers.py -- the level-tick emission to lift (with FADERS_MM/FADER_Y_MM).
- src/MonsoonWidget.cpp:238 -- MonsoonLightSlider<GreenRedLight> binding (the light colour to match).
- MonsoonWidget.hpp SL_TOP=45 / bottom 74.5 -- the travel geometry to match for stacking alignment.

## ROUND 3: cents-display refinements (Rodney, from second render)

The DSEG 7-segment amber cents readout (Micro12CentsDisplay, MonsoonMicro12.cpp:106-148) is good.
Three refinements:

### 1. Show 2 decimal places
Currently `(int)std::lround(...)` -> integer cents. Cents params carry sub-cent precision (the .scl
loader and equal-division defaults produce fractional cents, e.g. 111.73). Show two decimals:
```cpp
double cents = module
    ? (double)module->params[Micro12Ids::CENTS_PARAM_0 + i].getValue()
    : (double)MonsoonMicro12::defaultCents(i);
char buf[12];
std::snprintf(buf, sizeof(buf), "%.2f", cents);
// Ghost backing must widen to match: "888.88" not "888"
```
Update the DSEG ghost string from "888" to "888.88" so the off-segment backing spans the wider value.
Note DSEG7ClassicMini renders '.' -- verify the decimal point glyph exists in that face; if not, the
DejaVu-Bold fallback handles it, or use DSEG7ClassicMini (non-Mini) which includes the point.

### 2. STAGGERED gridded compartments -- rows offset by half a cell (Rodney, CORRECTED)
IMPORTANT: the round-2 render used a STRAIGHT 6x2 grid (columns aligned top-to-bottom). Rodney
prefers the STAGGERED layout: the lower row offset horizontally by HALF a cell pitch, so each cents
value sits directly above ITS OWN knob -- paralleling the staggered knob zigzag below. The straight
grid broke that parallelism (display columns no longer aligned with the staggered knobs).

Correct layout:
- Upper row = even degrees (0,2,4,6,8,10) at their knob X positions.
- Lower row = odd degrees (1,3,5,7,9,11) offset by +HALF the column pitch, sitting directly above
  their (staggered) knobs.
- Each cents value is a label floating over the knob it controls -- the display MIRRORS the physical
  knob stagger. This is the whole point: readout parallels knobs, cell-for-cell.
- Grid cells/dividers follow the SAME stagger: the compartment borders zigzag with the values, not a
  straight rectangular grid. A horizontal divider between the row BANDS is fine; the vertical
  dividers step by the half-cell offset between upper and lower.
- Thin, dim lines (theme ring/ink low alpha) -- Scalar's restraint, but staggered not rectangular.

So: keep the compartment look (round 3 ask), but the compartments STAGGER to match the knobs, they
do not form a straight grid. The parallelism between the cents readout and the knob row is the
feature -- each number labels its knob.

### 3. Bigger font -- there's room
Current nvgFontSize is 8.5f. The display band has vertical room; increase the font so the readout is
legible at a glance. Scale up (try ~11-12f) and re-fit the cell heights / ghost backing to match.
The whole point of the LED readout is at-a-glance tuning state, so bias toward legible.

### Layout note
With a proper grid + 2 decimals + bigger font, the display becomes a real "tuning table readout"
panel element, closer to Scalar's information density but in the dot.modular amber-LED idiom. This is
a nice differentiator -- Monsoon doesn't have this; it's a Micro-specific affordance that shows the
tuning the scale faders can't (cents aren't visible on a fader). Worth making it a signature element
of the Micro panels.

### Cross-refs (round 3)
- src/MonsoonMicro12.cpp:106-148 -- Micro12CentsDisplay (the readout to refine).
- res/fonts/DSEG7ClassicMini-Bold.ttf -- the LED face (verify decimal-point glyph, or use non-Mini).

## ROUND 4: zigzag dividers + a NOTES knob (Rodney)

### Zigzag dividers -- follow the value stagger, not a rectangular grid
The round-3 render still drew straight vertical dividers. Rodney's overlay (sketch) shows the dividers
should FOLLOW THE ZIGZAG: the compartment borders trace a connected path between the staggered values,
stepping up and down with each value so the whole readout snakes across in parallel with the knob
stagger below. NOT a rectangular grid -- a zigzag "ribbon" whose cells step between upper and lower.
- Each value's cell border steps up/down to sit with the value it contains.
- The dividing lines connect between adjacent cells like a path, tracing the up-down-up-down of the
  stagger (the sketch shows this as a continuous zigzag line linking the cells).
- Result: the compartments visually parallel the staggered knob row -- the readout is a ribbon that
  mirrors the physical knob zigzag, not a grid sitting above it.
- Keep it subtle (theme ring/ink low alpha) so the amber values lead.

### NOTES knob (1..12) -- active-degree count, drives tt.N (Rodney)
Add a knob (or stepped control) setting the number of active notes, 1..12. This is MORE than cosmetic:
`tt.N` is currently HARDCODED to 12 (MonsoonMicro12.cpp:28: `tt.N = Micro12Ids::N_DEGREES`). A notes
knob makes it VARIABLE, and the whole tuning system already keys off tt.N -- so this is exposing a
frozen value, not new plumbing.

Behaviour:
- **On .scl READ**: set automatically from the file's degree count (a 7-note file sets NOTES=7). Just
  reflects the loaded state.
- **On .scl WRITE / manual**: defines how many degrees are active/exported. NOTES=7 -> degrees 8..12
  disabled (weight=0), export the first 7 degrees' cents. A fast "this is a 7-note scale" control vs
  dragging faders to zero.

Model (recommend model 1 -- count only, Scalar-parity):
- NOTES = N means degrees 1..N active, N+1..12 disabled. Contiguous from root.
- Drives tt.N directly. The .scl writer exports tt.N degrees.
- The weight faders still weight WITHIN the active N for the scale mask; the tuning EXPORT is the first
  N cents values.
- This matches Scalar's NOTES field exactly and keeps .scl export length trivially = tt.N.
- Trade-off: can't express a non-contiguous subset (e.g. whole-tone 1,3,5,7,9,11) via the NOTES knob
  alone -- but the weight faders CAN still disable individual degrees within N for the scale mask, so
  the scale mask stays fully flexible; only the tuning-export length is the contiguous-first-N count.

Interaction with the existing .scl accept-predicate work (SCALA_FILE_AND_LOAD_UI): the reader already
validates degree count; NOTES is set FROM that count on read. On write, NOTES IS the count passed to
the writer. So NOTES knob = the read/write degree-count made visible and user-settable.

Panel placement: NOTES fits naturally near the display (it's a display-adjacent tuning parameter, like
Scalar groups NOTES with TUNING/OCTAVES/CENTS). Could sit in or beside the LED readout band.

### Cross-refs (round 4)
- MonsoonMicro12.cpp:28 -- tt.N currently hardcoded to N_DEGREES; NOTES knob makes it variable.
- SCALA_FILE_AND_LOAD_UI.md -- the .scl read/write degree-count that NOTES reflects/sets.
- src/dsp/TuningTable.hpp -- tt.N, the field NOTES drives.

## ROUND 4 CORRECTION: .scl collapses tuning and scale -- no distinction in the file (Rodney)

My Meaning-A-vs-B framing was the wrong axis. It tried to preserve the engine's internal tuning/scale
split (cents[] vs weight[]) THROUGH the .scl file. But Scala .scl has exactly ONE concept: an ordered
list of pitches. There is no "which are active" layer -- the file IS the scale, expressed as its
tuning. Scala has no other scale notion. So for the Micros, the .scl round-trip COLLAPSES tuning and
scale by design, because the format cannot hold the distinction and we don't want it to.

The rule for Micro-12/24 .scl:
- The number of pitches in the file = the scale = the active degrees. One list, no separate mask.
- **READ**: N pitches -> N active degrees (cents from file), remaining 12-N degrees not part of this
  scale (weight 0). NOTES = N.
- **WRITE**: collect the ACTIVE degrees (non-zero weight), export their cents in ASCENDING ORDER as an
  N-pitch file. The active degrees ARE the scale ARE the file.

Non-contiguous scales export correctly under this rule: whole-tone (degrees 1,3,5,7,9,11 active) writes
a 6-PITCH file (those six cents, in order). Re-import gives a 6-note scale (new degree 1 = old degree
1's cents, new degree 2 = old degree 3's cents, ...). The "gaps" do not survive because Scala has no
gaps -- a whole-tone scale IS a six-note scale, not a twelve-note scale with holes. This is correct
Scala behaviour.

This SIMPLIFIES the NOTES knob: it is not a separate "export length" that could disagree with the
faders. It is a readout/setter of "how many degrees are active," and active-degrees-with-their-cents is
exactly what the file is. NOTES and the fader-mask agree by construction -- same thing, two views.

Supersedes the Meaning-A/Meaning-B discussion above: there is no distinction to preserve. The active
degrees (non-zero weight), their cents, in order = the file. Both directions.

## ROUND 5: NOTES knob ruling (Rodney, answering CC's Ask-2)

CC asked A/B/C for the NOTES knob. First, two things already settled that reframe the question:
- CC's (i)-vs-(ii) check: it is (i) DEGREE COUNT, not multiple tunings per file. Rule out (ii) --
  a multi-tuning container is a different, larger feature, not intended.
- The round-4 correction ALREADY made the fader mask AUTHORITATIVE for export (WRITE = active degrees,
  non-zero weight, cents ascending). So "export uses the mask not the knob" is already true regardless
  of whether a knob exists. The knob is NOT choosing authority; the mask has it, settled.

So the models collapse:
- Model A (knob master, flattens sparse masks) -- REJECTED. Destroys the sparse-mask capability the
  faders exist to provide (whole-tone etc. must stay expressible) and contradicts round-4.
- Model C (no knob) -- viable and honest; active count already visible via faders + cents LED, export
  already uses the enabled count.
- Model B (knob = "set first N active" shortcut + live count readout, faders authoritative) -- viable,
  the one-gesture "make this a 7-note scale" has real ergonomic value.

RULING: Model B, with ONE critical constraint that removes CC's "knob disagrees with faders" tension:

THE NOTES KNOB HOLDS NO INDEPENDENT STATE. It is a LIVE READOUT of the active-degree count (derived
from the mask every frame), that is ALSO draggable as a bulk-set shortcut. It never stores an N that
can contradict the mask.
- Reading: NOTES always displays the current count of active (non-zero weight) degrees. Derived, not
  stored.
- Turning it to N: performs the enable-first-N action -- set degrees 1..N weight=1, N+1..12 weight=0.
  A bulk gesture on the faders, not a stored value.
- After a manual fader edit: the readout re-derives. Lift degree 9 while showing 7 -> it now reads 8
  (the true count). There is no stale "7" to disagree with anything, because the knob IS the mask's
  cardinality displayed.
- On .scl READ: the mask is set to N-active (round-4), so the readout naturally shows N. "Set on read"
  is satisfied by derivation, not by writing a separate knob param.
- On export: uses the mask (round-4), which the readout reflects. "Exported too" is satisfied because
  the count IS the mask's active count.

This is model B done safely: a draggable readout of mask cardinality, not a parameter that can drift.
It cannot disagree with the faders because it has no independent value to disagree with.

Implementation note: because it holds no persistent independent state, it is NOT a stored param in the
usual sense -- it's a custom widget that (a) reads the live active-count for display, (b) on drag,
writes the first-N enable pattern into the weight params. If VCV param persistence is awkward for a
derived control, it can be a non-param custom widget. CC's call on the widget mechanics; the SEMANTIC
is: derived readout + bulk-set actuator, zero independent state.

If CC prefers to ship C first (no knob) and add B later, that is acceptable -- B is an ergonomic
addition, not a correctness requirement. But B-done-right is the target.

## ROUND 5b: NOTES knob + "set equal-tempered" = a tuning-CREATION workflow (Rodney)

Beyond the scale-shortcut role, the NOTES knob has real value as the STARTING POINT for creating new
tunings, combined with an equal-tempered reset. The workflow:

1. Set NOTES = N (the bulk enable-first-N gesture: degrees 1..N active).
2. "Set equal-tempered" -> divide the octave into N EQUAL steps: cents[i] = i * (1200.0 / N) for the
   active degrees. This gives a clean N-EDO starting tuning.
3. Detune individual degrees by hand (drag the cents knobs) to sculpt the tuning you actually want.

So NOTES + equal-tempered is a "blank canvas at N divisions" button: pick how many notes, get them
equally spaced, then shape them. This is how you bootstrap a fresh microtonal tuning rather than
starting from 12-TET and editing.

Note the existing action is FIXED at 12: MonsoonMicro12.cpp:276 "Reset to 12-TET (all degrees, equal
division)" uses defaultCents(i) = i*100 (MonsoonMicro12.hpp:45). The new capability is N-AWARE equal
temperament:
- Existing: 12-TET reset (all 12 degrees, 100c steps). Keep it.
- New: "Set equal-tempered (N divisions)" -> for the currently-active N degrees, cents[i] =
  i * (1200.0 / N). N comes from the NOTES readout (= active count). So after NOTES=7, this gives
  7-EDO (171.43c steps); after NOTES=5, 5-EDO (240c steps); etc.

This makes the NOTES knob dual-purpose and gives it clear standalone value:
- As a SCALE shortcut: "make this a 7-note scale" (enable first 7).
- As a TUNING-CREATION seed: "start me a 7-note equal tuning I can then detune" (NOTES=7 + set
  equal-tempered).

Placement: "Set equal-tempered (N div)" as a context-menu action alongside the existing 12-TET reset,
OR a small panel button near NOTES. Menu is lower-surface; a panel button makes the create-a-tuning
workflow more discoverable. CC's call; the SEMANTIC is: equal-divide the octave across the current
active-N degrees.

Cross-ref: MonsoonMicro12.cpp:276 (existing fixed-12 reset), MonsoonMicro12.hpp:45 (defaultCents).

## ROUND 6: the grid is CLOSED STAGGERED CELLS, not a continuous zigzag line (Rodney)

CORRECTION of my round-4 wording. I wrote "connected ribbon path", "snakes across", "continuous
zigzag line linking the cells" -- that misled CC into drawing ONE continuous jagged waveform weaving
between the numbers. It reads as signal, not structure. That is NOT the intent.

Intended: each value sits in its OWN CLOSED COMPARTMENT (a rectangular cell with four walls). The
CELLS are staggered vertically by row -- upper-row cells (even degrees) sit higher, lower-row cells
(odd degrees) sit half a step lower -- so the arrangement of boxes zigzags, but each box is a complete
closed frame around its number. Discrete compartments, offset by row. NOT a single weaving line.

Think of it as two rows of boxes where the lower row is shifted DOWN (and by half a column in X, per
the stagger) relative to the upper row -- like brickwork offset, each brick a closed cell. The
"zigzag" is the PATTERN OF BOX POSITIONS, not a drawn zigzag line.

Concretely:
- Each degree's cents value = one closed rectangular cell (4 walls, subtle theme ring/ink lines).
- Even degrees: cells on the upper row.
- Odd degrees: cells on the lower row, offset down and +half-column in X.
- Do NOT draw a continuous line connecting cells. Do NOT draw a waveform. Each cell is independently
  framed; they just sit at staggered positions.
- The visual should read as "boxes at two staggered heights," parallel to the staggered knobs below --
  each box floating over its knob.

If a single divider grid is simpler to reason about: it is a normal grid of boxes, but the two rows
are vertically offset so the boxes interlock like offset brickwork rather than aligning into straight
columns. Closed cells, staggered placement. That is the whole correction.

## ROUND 7: it is SCALAR'S GRID, offset 50% horizontally between rows (Rodney -- definitive)

Stop reinventing this. The intended grid is EXACTLY Scalar's two-row cell grid (its "1 2 3 4 5 6 7 8"
boxes over the second row of boxes) -- a normal rectangular cell grid, two rows -- with ONE
modification: the LOWER ROW is shifted 50% horizontally (half a cell) relative to the upper row, so
each cell centres over its corresponding staggered knob below.

Construction (this is all it is):
- A normal rectangular grid: two rows of cells, thin dividers, like Scalar's degree grid.
- VERTICAL dividers: one short vertical line between adjacent cells in each row (Scalar already does
  this).
- HORIZONTAL divider: a line between the upper row and the lower row (Scalar already does this too).
- THE ONLY CHANGE from Scalar: the lower row is offset by HALF A CELL WIDTH horizontally, so the lower
  cells sit between the upper cells (in X), each lower cell centred on its knob. Because the upper and
  lower rows are offset by half a column, the horizontal divider between them appears to step (each
  upper cell's bottom edge and each lower cell's top edge are half-column-shifted) -- THAT stepping is
  what I mislabeled "zigzag." It is just two offset rows of a normal grid.

So: build Scalar's grid. Offset the lower row 50% in X. Done. Do not draw a waveform, do not draw a
connected path -- draw two rows of rectangular cells with the lower row half-a-cell shifted, exactly
like Scalar's grid but staggered to match the half-offset knob rows.

Each cell contains its degree's cents value. Upper row = even degrees over their knobs; lower row =
odd degrees over their (half-offset) knobs. The grid mirrors the knob stagger because the cells are
offset the same 50% the knobs are.

This supersedes rounds 4 and 6's wording entirely. Reference: Scalar's top-two-rows grid; apply a
50% horizontal offset to the lower row.

## ROUND 8: setting ENABLED per degree (the mask, distinct from weight) (Rodney)

The enabled/weight split (SHOPHOUSE_MICRO_SPEC rulings) needs a per-degree ENABLED control distinct
from the weight fader. Per degree we now have TWO facts: enabled (in/out of scale) and weight (loudness
within scale). The fader is weight; enabled needs its own affordance.

### DECISION: the NUMBER LABEL below each fader is a clickable enable toggle
Each fader already has a numbered label (1..N) beneath it (the number strip). Make that number the
enable toggle:
- CLICK the number -> toggle that degree's enabled (in/out of scale).
- Number APPEARANCE shows state: bright/normal = enabled (in scale); dimmed/greyed = disabled (out of
  scale) -- matching the fader light's own dim-when-out-of-scale, so a disabled degree reads dimmed at
  BOTH the fader and the number.
- The fader stays PURELY weight. The number becomes the mask control. Clean separation, zero new panel
  real estate.

Why this over alternatives:
- vs clicking the fader light: overloading drag-for-weight + click-for-enable on one widget is fiddly
  (accidental toggles when grabbing to drag) and undiscoverable. Rejected.
- vs a dedicated per-degree toggle button row: 12/24 new widgets eat vertical space wanted for faders
  + cents knobs. Rejected.
- vs fader-bottom = disabled: that's the CURRENT buggy conflation (weight==0 == out-of-scale) we're
  explicitly removing. Rejected -- it's the whole reason enabled must be separate.

### Relationship to the NOTES knob (the bulk vs per-degree pair)
- NOTES knob = BULK enable: NOTES=N enables degrees 1..N, disables the rest (COLONNADES round-5).
- Click-the-number = PER-DEGREE override on top of that bulk set.
- Fader = weight within the enabled set.
Three clean, non-overlapping controls: NOTES (bulk enable), number-click (per-degree enable), fader
(weight). The number label does double duty -- degree IDENTITY and enable toggle -- without clutter.

### Behaviour recap (with enabled now a first-class per-degree control)
- enabled=false (dimmed number + dimmed fader): out of scale. Fader zeroed at read, movement no effect.
- enabled=true, weight=0: in scale, silent. Fader can raise it.
- enabled=true, weight>0: in scale, sounding.
Root (degree 0) is always enabled (it's the tonic) -- its number is not a toggle (or is a no-op),
consistent with root having no cents knob (locked).

### .scl vs enabled reminder (don't reconflate)
The number-click sets enabled (the SCALE MASK, .dmtune only). It does NOT change NOTES (the tuning
size / .scl export length). Disabling degree 5 masks it out of the scale but it STILL EXISTS in the
tuning and STILL exports to .scl (SHOPHOUSE_MICRO_SPEC ".scl export is NOTES-based"). NOTES shrinks the
tuning; number-click masks within it. Distinct.

Cross-ref: the number strip (numbered labels below faders, COLONNADES round-2), NOTES knob (round-5),
enabled/weight split + .scl-by-NOTES (SHOPHOUSE_MICRO_SPEC).

## ROUND 9: number strip = interactive enable band, swipe-paint for one-or-many (Rodney)

Supersedes round-8's per-number-click. The number strip becomes an EXPLICIT INTERACTIVE BAND with a
swipe-paint gesture for enabling/disabling one OR a range of degrees. Indicator = the faders dim
(round-8 corrected: dimmed FADER shows disabled, consistent with out-of-scale everywhere).

### The panel band (new element)
Draw a BAND on the panel BEHIND the number strip (centred on NUM_Y=80.0, spanning the fader X range
FIRST_X..FIRST_X+(N-1)*PITCH plus a small margin) to visually define it as an active gesture area.
- A subtle filled/outlined strip (theme namewell/ink tone, low contrast) -- reads as "you can act
  here" without competing with faders/knobs.
- Full width of the fader bank so the whole 1..N row is one gesture zone.
- N-parameterised (spans to 24 in the Duo automatically -- it's just FIRST_X..last fader X).
- New kit marker: enable_band (the band rect the widget reads for hit-testing the swipe).

### The swipe-paint gesture (one or many)
On the band (the widget hit-tests within enable_band, maps X -> degree index via the fader pitch):
- SINGLE CLICK on a degree's number -> toggle that one degree's enabled.
- HORIZONTAL DRAG across the band -> PAINT a range. The paint STATE is set by the FIRST degree touched:
  - start on an ENABLED degree -> the drag DISABLES every degree in the swept range.
  - start on a DISABLED degree -> the drag ENABLES every degree in the swept range.
  (Standard drag-select "paint" idiom -- first cell sets the paint value, the drag applies it. One
  gesture both builds a scale (swipe from disabled) and clears one (swipe from enabled). Predictable on
  mixed ranges because it SETS to one state, doesn't independently flip each.)
- LIVE feedback: faders dim/undim across the range AS you paint -- the scale mask forms in real time on
  the fader bank.

### Why this (vs round-8 single-click only)
24 faders make one-at-a-time toggling tedious, and scales are usually contiguous/patterned runs. Swipe-
paint builds a 24-tone scale in a couple of swipes; works identically at 12. Composes with NOTES (bulk
first-N) + swipe (the actual pattern) + single-click (touch-ups).

### Geometry / implementation
- Band behind the number row at NUM_Y; hit-test X -> degree via round((x - FIRST_X)/PITCH), clamp
  0..N-1, ignore out-of-band Y.
- Root (degree 0) stays always-enabled: the paint SKIPS degree 0 (or treats it as a no-op) -- can't
  disable the tonic. A swipe over it just doesn't change it.
- Indicator is the FADER dim (ColonnadesLightSlider already dims out-of-scale) -- enabled[] drives the
  same dim path the scale mask uses. The number itself can also dim for extra clarity but the fader is
  the primary indicator (consistency with Monsoon+Shophouse out-of-scale dimming).
- enabled[] is the per-degree bool array (SHOPHOUSE_MICRO_SPEC v2 .dmtune) the swipe writes.

Cross-ref: gen_colonnades.py NUM_Y=80 + notelabel_<i> anchors (round-2), the enabled/weight split +
fader-dim indicator (SHOPHOUSE_MICRO_SPEC), NOTES bulk-enable (round-5). Supersedes round-8's
number-click-with-number-dim (gesture kept as a click, but now part of the band's click+drag, and the
indicator is the fader not the number).

## ROUND 10: N = TUNING SIZE (greyed faders beyond N), distinct from the enabled mask (Rodney)

Resolves the .dmtune save-n question and corrects the NOTES semantics. THREE per-degree states, THREE
distinct quantities. The current code has only two (it conflates "beyond the tuning" with "disabled").

### Three quantities
- **capacity** = nDegrees = 12 (Colonnades) / 24 (Duo). Module-fixed, the array size.
- **N (tuning size)** = the panel NUMBER control (was "NOTES"). 1..capacity. How many faders are LIVE
  (non-greyed). This is the value that sizes the tuning. NEW as an explicit persisted value -- the
  current code has no tuning-size concept (nDegrees is always capacity).
- **enabled[0..N-1]** = the scale mask WITHIN the tuning, set by the enable band (round 9).

### Three per-degree states (what the panel shows)
1. **Beyond the tuning** (degree >= N): GREYED OUT fader, not part of the tuning. No cents, not saved,
   masked in UI. On a Duo at N=17, faders 18..24 are here.
2. **In tuning, out of scale** (degree < N, enabled=false): DIMMED fader (round 8/9), has cents,
   zeroed at read, raisable-if-re-enabled. In the tuning, not in the current scale.
3. **In tuning, in scale** (degree < N, enabled=true): LIT fader, sounding.

So the panel now has THREE visual states: greyed (beyond N), dimmed (in-tuning out-of-scale), lit
(in scale). Greyed != dimmed -- greyed is "not in the tuning", dimmed is "in the tuning, masked".

### N and the enable band DECOUPLE (behaviour change)
Currently NOTES = enabled count: setActiveCount does enabledState[i] = (i < N) -- NOTES dictates the
mask. UNDER THIS MODEL they split:
- The NUMBER control (N) sets the TUNING SIZE only: faders >= N grey out (leave the tuning); faders
  < N become live. It does NOT set the enabled mask.
- The ENABLE BAND (round 9) is the SOLE writer of enabled[] -- which of the live 0..N-1 faders are in
  scale. Could be all N, could be a sparse subset.
So N sizes, the band masks-within. N no longer forces "enable first N". (When N grows, the newly-live
faders default enabled=true; when N shrinks, faders >= N leave the tuning -- their enabled state is
irrelevant while greyed, preserved if N grows back is optional/nice-to-have.)

### .dmtune SAVE/READ (resolves the n=24 bug)
- SAVE: write `n = N` (the tuning size), N cents, N enabled flags. Degrees N..capacity-1 are NOT
  written -- they're not in the tuning. (BUG being fixed: MicroTuning.cpp:597 uses
  `const int n = mod->nDegrees` (capacity=24) -- change to N, the tuning size.)
- READ: `n` from the file sets N (tuning size); load n cents + n enabled; faders n..capacity-1 grey out
  (state 1). A 17-degree .dmtune in a Duo -> 17-note tuning, faders 18..24 greyed.
- So "what cents beyond N?" DISSOLVES: nothing beyond N is in the file, because the file's n IS N.

### .scl consistency (already correct under this)
.scl export length = N (the tuning size) -- matches the "NOTES-based .scl" ruling (SHOPHOUSE_MICRO_SPEC).
.scl carries the N-degree tuning; the enabled mask is .dmtune-only. All consistent now:
N sizes both .scl and .dmtune; enabled[] is the .dmtune mask.

### Implementation delta
- Add a persisted tuning-size N (int, 1..capacity) to MicroTuningModule -- the NUMBER control writes it.
- NUMBER control (MicroNotesControl): drag sets N (tuning size), NOT the enabled vector. Rename intent
  from "active enabled count" to "tuning size / live fader count".
- Faders >= N: greyed (new visual state, distinct from the dimmed out-of-scale state). Widget checks
  deg >= mod->N -> greyed.
- Enable band writes enabled[] within 0..N-1 only (can't enable a greyed/beyond-N degree).
- .dmtune save: p.n = N (not nDegrees). .dmtune load: set N from file n.
- tt publish / Monsoon read: the tuning the engine sees is the first N degrees (tuning.N = N, not
  capacity). Confirm the engine's tuning.N tracks the tuning-size N, not the module capacity.

Cross-ref: MicroTuning.cpp:284-305 (MicroNotesControl -- change enabled-count to tuning-size),
:597/607 (save n -> N), round 9 (enable band writes enabled within N), SHOPHOUSE_MICRO_SPEC (.scl by N).
