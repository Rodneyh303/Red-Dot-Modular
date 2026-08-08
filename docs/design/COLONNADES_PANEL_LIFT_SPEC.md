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
