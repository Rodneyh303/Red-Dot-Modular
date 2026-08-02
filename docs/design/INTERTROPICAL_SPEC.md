# Intertropical — scene sequencer (design)

Status: DESIGN (build AFTER the control-layer trilogy: East de-param -> undo -> lock). Concept
frozen here while vivid; not started, to avoid forking the current arc.

## Concept
A graphical sequential switch scoped to VOICES — the arrangement layer over dot.modular's
generation layer. Monsoon generates rich per-voice material (Sands, spread, Change Alley
correlation); Intertropical arranges which voices SOUND in which passage, over time. This is
the form/section layer the instrument lacked.

Name: the ITCZ (Intertropical Convergence Zone) drives the monsoon cycle, so Intertropical
driving scene changes off Monsoon's phrase/boundary crossings is the same causal shape, not a
cosmetic label — scene advance IS a monsoon-cycle crossing.

## Intertropical is the thesis-test (why it matters beyond being a module)
The whole instrument bets on correlation-as-composition / horizontal poly conservation /
voices-as-material-across-time. Sands, spread, and Alley PRODUCE that correlated poly; nothing
until Intertropical lets you ARRANGE and HEAR it as sections over time. So Intertropical is the
first place the thesis becomes audible as MUSIC WITH FORM rather than as texture. It's a genuine
falsification condition: if arranging Alley-correlated voices into scenes sounds COMPOSED
(coherent sections, relationships heard across the arrangement), the idea works; if it sounds
like a fancy mute matrix over noise, the correlation was never doing the compositional work the
thesis claimed. This proves the THESIS, not automatically the instrument's SUCCESS — those can
come apart (the thesis could hold while the module is fiddly, or vice versa). Success also rides
on the fun/usability axis below.

## Design constraint: the Rhythm-Explorer fun loop (load-bearing, not a feature)
Reference point: Venom Rhythm Explorer (VCV, Vermona Random Rhythm lineage) has a simple
pattern-length + repeats control that is genuinely FUN in its much simpler context. Intertropical
should be a MULTIPLE of that fun. Why that control is fun, named so it can be preserved:
immediate, legible, low-commitment variation — one simple gesture, an audible understandable
result RIGHT NOW, a tight loop between action and heard consequence that rewards fiddling.

Intertropical has the ingredients but ALSO a risk the simple version doesn't: an 8x17 grid, 5
output types, phase-aware advance. Complexity is the enemy of the tight fun loop. Therefore a
GUARDRAIL, stated now while it's the explicit goal so it survives the build:

  **The primary loop — click a cell / set repeats / hear it at the next boundary — must stay as
  immediate and legible as Rhythm Explorer's length+repeats. No advanced capability (phase
  nonlinearity, Lantern view, poly routing depth) may compromise that immediacy. Depth is
  OPTIONAL and must never sit in the way of the basic fun.**

This is easy to lose incrementally as the grid accretes features; it's a design test to apply to
every addition, not a one-time note.

## Main display: 8 x 17 grid
- **8 columns = 8 scenes.**
- **Top row (row 0): repeat count 1..8** per scene — how many phrase-boundary CROSSINGS the
  scene holds before advancing.
- **Rows 1..16: voice/channel opt-in** — each cell clickable, coloured = voice IN the scene,
  hollow = out. A scene is a column: its repeat count (top) + its 16-voice membership mask.
- Similar grid discipline to Sands (SandsGrid.hpp row-grid alignment) — reuse the pitch/top/
  height conventions.

## Outputs: poly, routed
Poly outputs for GATE, CV, ACCENT, LEGATO, SLEG. As Monsoon plays, Intertropical routes ONLY
the voices in the active scene to these outputs. A graphical sequential switch: the scene mask
gates which of the 16 voices reach the poly outs.

## Advance: boundary crossings, phase-aware
- Repeat count = number of phrase-boundary CROSSINGS (NOT forward steps), in EITHER direction,
  because phase drive can run the sequencer backward. Counting crossings is the phase-coherent
  definition — consistent with the stateless lane-position model (position = pure function of
  totalStepsElapsed).
- **ONE PHASE CYCLE = the 16 steps.** The scene boundary is at the CYCLE EDGE, not inside it.
  So the main phase use case — moving around the 16 steps NONLINEARLY (jumping, non-monotonic
  traversal) — happens ENTIRELY WITHIN one scene's phrase and crosses NO scene boundary. You
  only cross a scene boundary by completing a whole cycle. This retires the "frequent boundary
  crossings under phase wobble" concern: normal nonlinear phase play never touches the scene
  boundary; you'd have to deliberately oscillate phase across the cycle ENDPOINT to rack up
  crossings, which is a rare, intentional gesture, not an incidental one. The jitter case is
  pathological, not the default.
- **Scene membership is READ only at the phrase boundary.** Belt-and-braces with the above:
  membership is sampled once per boundary, acted on, done. (Same move as slew / the effect-
  timing hierarchy — resolved by WHEN it's read.)

## Lock-mode classification (settled by Rodney's four constraints)
Intertropical is the OUTPUT/arrangement layer, downstream of all generation. By the
read-vs-map principle (LOCK_SEMANTICS.md 9): output-mapping stays LIVE.
- **De-parammed from the start.** No host params ever, no DAW automation. Nothing to de-param;
  built store-backed on the new substrate natively — sidesteps the trilogy by construction.
- **No modulation.** No CV/automation INTO Intertropical, so no modulation-latching question.
- **LIVE during lock mode.** You lock the PATTERN (what voices generate); the ARRANGEMENT keeps
  playing — scenes keep advancing, outputs keep switching. Freeze the material, keep moving
  through which voices you hear. Same live side as transpose.
- **Two things, both true at once:** grid EDITS (clicking cells) apply live (output routing,
  not latched). The membership the engine ACTS on is read at the phrase boundary. So a live
  edit to the current scene takes effect at the NEXT boundary, not mid-phrase — rearrange
  freely, hear it snap in on the beat.
- Scene advance shares the SAME boundary clock as queued regeneration (dice/scatter release).
  They co-occur on the same crossings but are independent: a scene advance switches routing; a
  queued redraw changes material. No interaction to resolve — different layers.

## Why this, not a plain sequential switch (the thesis)
We earlier considered NOT doing a sequential switch — a plain "route N of M voices" utility is
generic and doesn't earn a module. Intertropical is different because of WHAT it switches, not
how. It is a sequential switch POINTED AT the axis the instrument is uniquely about:
- Vertical axis = voices-as-DEPTH (16 voices, spread collapsing them toward mono). Already
  well-served.
- Horizontal axis = voices-as-MATERIAL-across-time. This is the axis Change Alley operates on
  (correlating voices to each other) and where "high poly conservation" lives — but it had no
  direct editing/viewing surface.
Intertropical makes the HORIZONTAL axis directly editable and visible: which voices, when, for
how long. It teaches the user to see the poly as a HORIZONTAL thing, not just vertical, and to
leverage Alley's correlation effects by arranging the correlated voices into sections and
watching them play out. So it is integrative, not utilitarian — it earns its place by exposing
the instrument's core idea (horizontal poly conservation + correlation) rather than by adding a
generic feature. Generic in MECHANISM, specific in WHAT it switches: the horizontally-conserved,
Alley-correlated poly. Lantern (below) closes the loop by making the result legible.

## Visual feedback
- Playing-scene indicator (which column is active).
- Remaining steps/crossings in the current scene (countdown toward advance).
- Direction indicator if phase drive is running backward (optional).

## Companion: Lantern shows Intertropical's output (PREFERRED over a second module)
Lantern ALREADY has a viewMode enum (Notes/Velocity/Prob, persisted -- Lantern.cpp:102) AND
both grid and piano-roll rendering (LANTERN_SPEC.md, LANTERNS_PIANO_ROLL_SPEC.md). So the
enhancement is NOT "add grid/piano modes" (they exist) -- it is "add Intertropical's routed
output as a SOURCE the existing grid/piano rendering can point at." Smaller work than it first
sounds: reuse the render, add a source.

What the Lantern mode SHOWS (per Rodney's original framing -- "show what's being produced by
Intertropical"):
- The voices Intertropical is CURRENTLY routing (the active scene's members) as they play, in
  grid and/or piano-roll form -- so you SEE the arrangement Intertropical is producing, not just
  Monsoon's raw output. Closes the loop: Intertropical arranges the horizontal poly, Lantern
  makes the arranged result legible.
- Ideally distinguishes routed-IN voices from muted-OUT ones (colour/hollow, echoing the
  Intertropical grid's own cell convention) so display and sequencer read consistently.
- Follows the same step/phrase-boundary timing as Intertropical's routing (membership read at
  the boundary), so the display switches WITH the audio, not ahead of it.

Rationale for mode-not-module:
- One visualiser with a clean mode switch beats two overlapping visualisers (no "which Lantern?"
  confusion).
- A Lantern mode reading Intertropical's routed output keeps the display-source dependency
  INSIDE one module that knows how to render several sources, vs two modules agreeing on a
  cross-module protocol. The preferred option is also the more maintainable one.
- Reuses Lantern's existing grid + piano rendering and viewMode/persistence machinery rather
  than duplicating it.
- Second Lantern is the fallback ONLY if mode-crowding on one Lantern becomes a real UI problem.

## Why build AFTER the trilogy (not now)
- Focus: lock + undo designs are freshly settled and want BUILDING now; forking into a large
  new module (8x17 grid + 5 poly output types + playhead + Lantern mode ≈ Sands-scale surface)
  risks a half-built scene sequencer next to a half-built lock mode.
- Substrate: Intertropical is store-backed/output-only by design, so it WANTS the de-parammed,
  lock-aware substrate to exist first — build it once, correctly, on the finished foundation.

## Panel plan / scaffold

Panel language: dotmod_design.py tokens (native 75 DPI, nanosvg-safe: no masks/gradients/
filters, every shape carries its own paint, no <text> for control labels). Generator
panel_src/gen_intertropical.py -> res/panels/Intertropical_panel_{dark,light}.svg. HP: the
8-col x 17-row grid + poly outputs wants width; target ~20-24HP (confirm against a grid cell
big enough to click; Sands cell pitch is the reference).

### Layout (top to bottom)
1. Brand strip: dot.modular wordmark (logo_embed), module name "Intertropical".
2. Repeat-select row (grid row 0, 8 cells).
3. Scene progress indicator.
4. Voice-membership grid (rows 1..16 x 8 scene columns).
5. Poly outputs: 5 jacks -- GATE, CV, ACCENT, LEGATO, SLEG (poly, one jack each).
6. Phase/clock: confirm whether Intertropical reads Monsoon's phase via the expander bus or
   needs its own jack.

### 1. Repeat selector (top area, "convenient + visual") -- SUBDIVIDED GRID CELL
Better than a separate LED meter (Rodney): use the SAME grid idiom as the voice cells so the
repeat row reads as part of the same continuous display, not a foreign control. The repeat row
is one cell per scene, each subdivided HORIZONTALLY into 8 sub-segments. The widget uses
colour-DEPTH to carry BOTH signals in one cell:
- COUNT: N sub-segments lit = N repeats.
- PROGRESS: the fill deepens/advances through them as the scene plays -- "repeat 3 of 5" reads
  as 3 done (bright) + 2 pending (dim). No separate progress indicator needed; the same cell
  shows count and how-far-through at once.
Panel draws the static sub-gridlines (scene boundaries slightly stronger than the 8 sub-segment
ticks, so it reads as "8 cells each split into 8", not "64 equal cells"); the widget draws the
fill/progress.
Input gesture (decide at build): (a) click at position N/8 to set N repeats -- compact but ~1.5mm
targets at 22HP are tight; (b) click-DRAG across the cell like a mini horizontal fader, 8 detent
stops -- more forgiving, natural "more/fewer", keeps the one-gesture fun loop. LEAN (b). Display
is identical either way.

### 2. Scene progress indicator ("which scene playing + how far through repeats")
Both signals live (Intertropical is LIVE under lock):
- Playing-scene: the active scene COLUMN highlighted (bright border / underglow on that whole
  17-cell column). Unmistakable which is sounding.
- Repeat progress: in the active scene's repeat bar, lit segments = total count, a brighter/
  distinct-fill cell marks the CURRENT repeat -- "3 of 5" reads as 5 lit, 3rd emphasised.
  Advances each phrase-boundary crossing. Optional thin progress arc across the current repeat
  toward the next boundary if steps-within-repeat is wanted.

### 3. Voice-colour question -- RESOLVED
The grid shows MEMBERSHIP (voice in/out), NOT pitch. So:
- IN/OUT is the LOUD primary: filled = in, hollow = out, high contrast, reads instantly.
- Voice IDENTITY secondary -- YES colour it, for identity not decoration, REUSING Lantern's
  voiceColour(v) palette (Lantern.cpp:803, 8 hues, wraps mod-8). Then voice 3 is the SAME amber
  on the Intertropical grid as on Lantern -- colour carries CROSS-MODULE identity, exactly what
  Lantern's plain grid view lacks. Filled cell = voiceColour(row); hollow = faint voiceColour
  outline. 16 rows wrap at 8 (rows 1&9 share a hue; fixed row position disambiguates).
- LEFT GUTTER: just VOICE NUMBERS 1..16 (widget-drawn text), NOT colour swatches. Identity-by-
  colour lives in the CELLS (a filled cell is that voice's hue); a gutter swatch would be
  redundant. A plain number is all the row label needs, and it frees the gutter. (Rodney.)
This is why per-channel colour earns its place: makes "which voices am I arranging" legible and
consistent with the rest of the instrument -- answering the Lantern-grid gap directly.

### Grid geometry + build note
- Reuse SandsGrid.hpp row-grid discipline (uniform pitch, LANE_TOP/LANE_H) adapted to 8 columns.
- Column = scene (repeat bar on top + 16 membership cells). Row = voice 1..16.
- Store-backed, output-only, NO params (de-parammed from the start): grid cells + repeat bars
  are store-backed toggle widgets (bindWidget, not addParam).
- Panel art is STATIC (grid wells, brand, output labels); ALL live state (membership fill,
  active scene, repeat progress, playhead) is WIDGET-DRAWN over it. Single-source-geometry:
  panel is the source, widget reads cell positions from it (same as Sands).

## Open (decide at build)
- Exact repeat-count semantics at the boundary: does a scene with N repeats advance ON the Nth
  crossing or AFTER it? (Off-by-one to pin against the playhead display.)
- Backward phase across a scene boundary: does the scene index decrement (true reversal) or
  does "crossings" only ever count UP toward advance regardless of direction? Membership-read-
  at-boundary makes either safe, but the playhead semantics differ.
- Default scene when none are defined / all cells hollow (probably: pass nothing, or pass all —
  decide which is the sensible empty state).

## Design note: C5 (alternative grid layout — deferred)
Considered during the routing design discussion: **rethink the grid as 8 scenes x 8 outputs**
instead of 8 scenes x 16 voices. Each cell = which voice (1-16) is routed to that output in
that scene. This directly shows the routing (output to voice), naturally enforces the 8-voice
limit (8 outputs, one voice each), has a smaller grid (8x8 vs 8x16), and matches the VCV
Octal Router's model (each output gets one input, per scene).

**Why deferred:** the 8x16 grid (voices as rows) lets you scan a voice's row horizontally to
see which scenes it participates in — the "arrangement view" that is Intertropical's primary
thesis. C5 loses that (voice numbers are scattered across output rows). The hybrid (auto-pack
+ right-click override on the current grid) was chosen instead, preserving the arrangement
view while adding routing control.

**When to revisit:** if the right-click override proves too fiddly for real use, or if users
consistently want to see "what's on output 3 across all scenes" (a routing view), C5 could be
added as an alternate display mode. The data model (sceneOutput[8][16]) already supports it.

## v2 direction (from first working build -- "Intertropical rocks", vibey 8-bar mono loop)

The first playable build reframed the module: the original design was really a channel FILTER
(route the active scene's voices, block the rest). That is NOT a sequential switch -- a switch
ROUTES (decides what comes out WHERE). Adding channel COLLAPSE (map active voices down onto
output channels) is what makes it the switch it is named for. This is the core v2 shift, already
implemented as a hybrid auto-pack + override allocator.

### Multiple Intertropicals -- phase-locked by construction (DECIDED)
All Intertropicals sync off the MAIN Monsoon system drive (clock / gates / phase) -- they read
the same totalStepsElapsed, so they advance and their scene-repeats WRAP at the SAME phrase
boundary. This kills the drift-vs-polymeter ambiguity: multiple Intertropicals are phase-locked
by construction, not by a setting. Polymetric arrangement still works WITHIN the shared clock (a
2-repeat and a 3-repeat scene realign every 6 phrases), but every one of those boundaries is a
shared Monsoon boundary -- there is always a common downbeat. "Band playing together" is the
structural default, not an option to get wrong.
Endgame this enables: N Intertropicals each collapse a DIFFERENT voice-subset to a DIFFERENT
output, each its own arrangement -- bass (voices 1-4 -> mono, transposed down), riff (5-8), chords
(9-16 kept poly). Monsoon generates one correlated poly field; the Intertropicals carve it into
parts. The horizontal-conservation thesis as a multitrack arrangement.
Requires: EMPTY SCENES must be allowed (a scene with no members = that part rests this phrase --
essential for arrangement, e.g. bass drops out for a section).

### Per-scene transpose (CLEAN next feature)
Transpose is output-mapping (LIVE, post-generation -- LOCK_SEMANTICS 9), so a per-scene transpose
is just "shift this scene's collapsed output by N semitones". Composes for free: same voice-set,
different scenes at different transpositions = a chord progression / modulating riff from ONE
pattern. Turns the arrangement layer into a HARMONIC layer. Stays fun: one number per scene
column, like the repeat count. Highest-leverage addition; do this.

### Collapse policy -- REVIEW of the implemented computeRouting()
Current logic (Intertropical.hpp:85) is a clean two-pass allocator and is the right design:
- Pass 1: honour forced OVERRIDES (sceneOutput[scene][v] >= 0), each claiming its channel.
- Pass 2: auto-pack remaining active voices in VOICE ORDER into free channels 0..7.
So: default = voice order, optional per-voice override. Members limited to 8 per scene
(MAX_VOICES_PER_SCENE) = 8 output channels. This is correct and matches "default to voice order
with optional override". Right-click cycles a cell's override (Auto -> 0..7 -> Auto).
Points to firm up (the "still needs thought" part):
- OVERRIDE COLLISION: if two voices are forced to the same channel, pass 1's `used[]` gives the
  first-scanned voice the channel and the second falls through to auto-pack. That is defensible
  but SILENT -- the user set an override that didn't take. Consider a visual "conflict" mark on
  the losing cell so it is not a mystery.
- >8 MEMBERS: the mask can hold up to 16 bits but only 8 can route. Enforce the 8-member limit at
  SET time (setCell refuses a 9th) so the grid can't represent an unroutable scene -- cleaner
  than silently dropping voices 9+ at route time. (Confirm current setCell behaviour.)
- EMPTY SCENE: nOut must be allowed to be 0 (part rests). Confirm setChannels(0) path is clean.

### UI ideas to improve the collapse (OPEN -- Rodney invited)
- Show the OUTPUT CHANNEL each active cell maps to (already drawn as a corner number). Could also
  tint the cell by output-channel rather than voice, toggeable, so you SEE the collapse.
- A thin "output rail" (8 slots) beside the grid showing which output each channel currently
  carries -- makes the collapse legible as a mapping, not just per-cell numbers.
- Drag a cell onto an output slot to set its override (more direct than right-click cycling).
- Mark override COLLISIONS and the >8 OVERFLOW visibly.
(Decide in play; keep the fun-loop guardrail -- the basic click-to-add must stay immediate.)

### Panel: align Intertropical lanes to Lantern grid lanes (WORK NEEDED)
Lantern's grid-view lane height is H / N_VOICES (16 lanes fill the display height,
Lantern.cpp:395). For Intertropical's 16 voice rows to ALIGN with Lantern's lanes when the two
are viewed together (the produce/observe pair), Intertropical's main grid screen must use the
SAME effective lane pitch -- its 16 rows dividing its screen height the same way Lantern's 16
lanes divide theirs. The panel generator (gen_intertropical.py) needs adjusting so GRID row pitch
matches Lantern's laneH convention (shared SandsGrid.hpp-style constants if possible), and the
repeat band + gutter sized so the 16 voice rows land on the Lantern lane grid. This is real panel
work, not just cosmetics -- lane alignment is what makes the two modules read as one system.

## Collapse routing model -- avoid the cube (DECIDED)

The flexibility Rodney wants from 8 outputs (mono bass + mono melody + 2-voice riff + 4-note
chord from ONE Intertropical) includes FAN-OUT: one voice contributing to more than one output in
a scene (e.g. a voice thickening both melody and chord). That breaks the current model, and
naming why fixes the design:

- Current state is sceneOutput[scene][voice] = ONE output per voice (int8_t). That is a FUNCTION
  voice->output. Fan-out (voice -> SET of outputs) is a RELATION, which a single int8_t can't hold.
- Representing per-scene fan-out fully = a scene x voice x output BOOLEAN CUBE (8 x 8 x 8 = 512
  cells of routing state, ON TOP of the membership grid). There is no honest 2D view of a cube;
  you can only show one scene's slice and scrub the third axis. This is where the fun-loop
  guardrail SNAPS -- it stops being "click cells, hear a loop" and becomes a modular routing
  environment. The cube is the thing to AVOID.

### The collapse of the cube: routing is PER-INSTANCE, not per-scene
The scene axis of the cube exists ONLY because routing is allowed to change per scene. But
routing DEFINES THE PARTS ("voices 1-2 = riff, voice 3 = melody, 4-7 = chord") -- that is an
INSTRUMENT SETUP, not an arrangement move. What actually changes scene-to-scene is WHICH voices
are active (membership) and their TRANSPOSE. So:
- **Routing (voice -> outputs, INCLUDING fan-out) is PER-INSTANCE:** one 8-voice x 8-output
  MATRIX, set once. A voice feeding both melody and chord = two lit cells in its row. This is a
  SQUARE (fully visualisable, learn-once), not a cube. It keeps the FULL fan-out expressiveness
  Rodney asked for.
- **Membership + transpose stay PER-SCENE:** the existing grid, unchanged -- the fun loop.

Making routing an instance-level PATCH removes the scene axis entirely: the cube collapses to a
square. You keep "voice into melody-and-chord"; you just declare it ONCE rather than re-declaring
every scene. Composes with multiple Intertropicals: each instance = "these voices, patched to
these outputs this way, playing this arrangement of scenes at these transposes." The patch is the
instance's identity; the scenes are its arrangement.

### What this gives up, and the escape hatch
Given up: a voice that feeds melody in scene 1 but chord in scene 3 (routing that REARRANGES
between scenes). This is a much rarer want than fan-out-WITHIN-a-scene (which the per-instance
matrix handles fully). If play specifically demands per-scene re-routing, the answer is NOT to
add the scene axis back (the cube) -- it is to SPIN UP A SECOND INTERTROPICAL for the differently
-routed part. Multiple instances is the escape hatch from the cube: two square matrices instead
of one cube. This is consistent with the whole multi-instance-as-parts design.

### Consistency + trade-off notes
- The per-instance 8x8 voice x output matrix is the SAME idiom as Change Alley's pin matrix (a
  cell = a connection). Intertropical's collapse becoming a patch matrix is consistent with the
  instrument, not a new concept.
- FAN-OUT COUPLES PARTS: a voice feeding both melody and chord means the SAME generated line
  appears in both -- they move in lockstep. Sometimes wanted (unison doubling, anchoring a chord
  to the melody), sometimes muddy (chord not independent). Fan-out is powerful but not free
  richness; it couples the parts. Know this before reaching for it.

### Tiering (keep the fun loop)
- v1 DEFAULT stays simple: voice -> one output, auto-packed in voice order (current
  computeRouting). The fast one-grid experience. NOTE: the current sceneOutput[scene][voice] is
  per-SCENE single-output -- migrate it toward a per-INSTANCE mapping (drop the scene index) when
  the routing matrix is built, so v1 doesn't bake in the very per-scene axis we're avoiding.
- OPT-IN routing matrix (per-instance, fan-out capable) is the power path -- designed here,
  DEFERRED until the simple path has been played enough to know exactly what the matrix UI needs
  to feel like. Do not build speculatively.

## Routing model + fan-out (SETTLED -- supersedes the earlier per-instance-square sketch)

Converged over several design passes. Three spaces, two mappings; fan-out lives entirely in the
global mapping so no per-scene routing / cube is needed.

### Three spaces
1. **Global voices (16)** -- Monsoon/Straits per-voice output. The vertical poly.
2. **Scene slots (<=8)** -- per scene, a selection of 0..8 of the 16 global voices, seated in
   slots. 8 is the HORIZONTAL budget: the poly-conservation law applied horizontally (16 vertical
   voices conserved down to <=8 horizontal slots per scene). 8 is a deliberate compromise -- fits
   most extended chords; if you truly want a 16-note chord, use Monsoon/Straits directly.
3. **Outputs (8)** -- each a mono channel of Rack's 16-ch poly out.

### Two mappings
- **voice -> slot : PER-SCENE (membership).** Which global voices are seated, and in which slot,
  this scene. This is the arrangement; it MUST be per-scene. Default: auto-pack members into slots
  in voice order (the current computeRouting behaviour). Opt-in: explicit slot seating when you
  need a specific voice in a specific slot.
- **slot -> output : GLOBAL (routing setup).** Which output(s) each of the 8 slots drives. Set
  once for the instance; defines the PARTS ("slot 3 = 3rd tone of chord A -> output 3"). This is
  instrument setup, not an arrangement move.

Because slot->output is global and voice->slot is a per-scene function, the whole cube is avoided:
the only per-scene state is a function (each slot holds one voice), and the relation (fan-out)
lives once in the global layer.

### Fan-out -- what it is and where it lives
Fan-out = one slot driving MORE THAN ONE output. Data: slotOutput[slot] becomes an 8-BIT OUTPUT
MASK (not a single index). slot 3 with mask 0b00100100 -> outputs 3 and 6. So `slotOutput` is 8
bytes, each a bitmask over the 8 outputs. Global, tiny, fully viewable.
- Fan-out lives ENTIRELY in the global slot->output layer. The per-scene voice->slot stays a clean
  function. So you get fan-out expressiveness with NO per-scene routing and NO cube: whatever voice
  is seated in a fanned-out slot this scene feeds all that slot's outputs. Per-scene scoping of
  fan-out is achieved by WHICH voice sits in the slot (membership), not by per-scene routing.
- TRADE-OFF (know before reaching for it): fan-out means the SAME generated voice line hits all the
  slot's outputs in lockstep. For chord tones that is often intended unison/octave doubling;
  sometimes it is muddy (the outputs are not independent). Fan-out couples the parts; it is not
  free richness.
- v1 default: slot->output is a simple assignment (one output per slot, a permutation) since the
  common chord-tone case is 1:1. Fan-out (multi-output mask) is the opt-in power path.

### Worked example (the voice-16 case, resolved)
"Outputs 1-3 = chord A, 4-6 = chord B; voice 16 feeds one tone of EACH chord, only in scenes 4 &
6; other scenes that tone comes from other voices." Resolution: slot 3 -> output 3 (a tone of A),
slot 6 -> output 6 (a tone of B), GLOBAL. Then per-scene membership seats VOICE 16 into slots 3
and 6 in scenes 4 & 6; other scenes seat different voices there. Output 3/6 are always fed (stable
chord tones); WHO feeds them changes per scene, purely by who's seated -- no per-scene routing, no
fan-out even needed for this case (it's voice-substitution, handled by membership). Fan-out would
only be needed if ONE voice had to feed BOTH tones simultaneously -- then slot X -> outputs {3,6}.

### UI (replaces the scroll-a-number override -- SUPERSEDED)
The current sceneOutput[scene][voice] scroll-through-a-small-number override is superseded: it
hides the mapping, is slow, and cannot show fan-out (a single number can't say "3 and 6"). Replace
with TWO COUPLED GRIDS matching the two mappings:
- **Membership grid (per-scene):** 16 voices x 8 scenes (existing). Click seats a voice. Default
  auto-packs to slots in voice order; explicit slot seating is the opt-in.
- **Slot->output grid (GLOBAL): an 8x8, slots (rows) x outputs (cols).** A lit cell = that slot
  drives that output. FAN-OUT is simply TWO lit cells in a slot's row -- visible at a glance, no
  scrolling, no hidden state. Same idiom as Change Alley's pin matrix (a cell = a connection), so
  it is consistent with the instrument. Global + small, so it's a set-and-forget setup grid, not
  per-scene churn.
- Interaction split mirrors the data model: LEFT = per-scene arrangement (membership), RIGHT =
  global setup (slot->output routing). Seat voices per scene (fast); patch slots to outputs once.

### Refactor from current code
Current sceneOutput[scene][voice] flattens the two mappings into one (voice->output per scene,
skipping the slot layer) -- which is exactly why fan-out and scoping got tangled. Split it:
- sceneSlots[scene]  : per-scene voice->slot seating (membership WITH slot position).
- slotOutput[8]      : global slot->output bitmask (fan-out capable).
Migrate the per-scene single-output override into this two-layer form; drop the scroll-number UI.
Keep auto-pack as the v1 default so the simple path stays a one-grid click experience; the global
8x8 slot grid and explicit seating are the opt-in power layers, deferred until the simple path has
been played enough to know the matrix UI's exact feel.

## Per-output transpose (SETTLED)

Each of the 8 outputs has its own transpose knob: +/-24 semitones (2 octaves each way),
per-semitone DETENTED (49 detents). Chosen over +/-36 for knob usability -- 2 octaves covers
essentially all arrangement transposition, and 49 detents stay hittable vs 73.

Transpose is OUTPUT-mapping (post-generation), so it is LIVE under lock (LOCK_SEMANTICS 9,
same side as Monsoon's own transpose). It shifts the finished pitch on that output channel;
it changes nothing generated.

### Fan-out x per-output transpose = HARMONIZED doubling (upgrades the fan-out trade-off)
Because transpose is PER-OUTPUT and a fanned-out slot feeds MULTIPLE outputs, one voice via a
fan-out slot comes out at EACH output's own transpose simultaneously. So fan-out is lockstep in
RHYTHM but can be HARMONIZED in PITCH: voice on slot X -> output 3 at +0 and output 6 at +7 = the
same line doubled a fifth apart. This upgrades the earlier fan-out caveat ("same line in lockstep,
sometimes muddy unison") into a genuine chord-voicing TOOL: fan-out + per-output transpose builds
intervals/voicings from a single voice. Note it is still rhythmically locked (one line's timing),
so it thickens/harmonizes a part rather than creating independent counterpoint -- which is exactly
what you want for chord voicing.

### Placement (panel)
Transpose is per-output; the 8 outputs are the COLUMNS of the global slot->output grid. So the 8
transpose knobs sit in a row aligned to those output columns (knob N under output column N),
making the right-hand block a coherent "8 outputs + everything global about them" unit: the 8x8
slot->output routing grid plus its row of 8 output-transpose knobs.

## Panel layout (v3 -- align to Lantern, fit routing grid + transpose)

Constraints resolved:
- REPEAT strip stays ABOVE the scene grid (reads as belonging to the scene columns), reduced to
  MAX 4 repeats (8 -> 4) so each control is bigger/clearer. ~10mm tall horizontal strip.
- The repeat strip pushes the main grid top DOWN (below the brand + repeats). Alignment with
  Lantern is achieved by shifting LANTERN's LCD DOWN to the SAME grid-top + keeping the 6.0mm
  lane pitch -- NOT by forcing Intertropical's grid to 16mm. Both grids share top+pitch; Lantern
  has the headroom to move (Rodney: room to move Lantern LCD + jacks down a bit).
- Main MEMBERSHIP grid: 16 voices x 8 scenes, 6.0mm row pitch (= Lantern laneH = 96/16), 96mm
  tall. Voice-number gutter on the left (widget-drawn numbers).
- RIGHT of the membership grid: the global 8x8 slot->output routing grid + a row of 8 per-output
  transpose knobs aligned to its columns.
- JACKS at the bottom, below the 96mm grid; move down as needed within the 128.5mm budget.
- Panel WIDER (~24-25HP) to fit membership grid + gutter + routing grid + transpose without
  cramping. Grid cells must be comfortably clickable (not tiny).

## Lantern visualises Intertropical output -- via ROUTED ARTICULATION STATE, not the jacks (DECIDED)

Goal: a Lantern paired with an Intertropical shows, for Intertropical's output, the SAME
tie/legato/accent/slew per-voice detail it shows for Monsoon/Straits -- the detail that has been
invaluable for debug and understanding.

### Why NOT read the jacks (rejects the cable model) -- corrected reasoning
NOTE: Intertropical now HAS legato + sleg (step-legato) outputs in addition to gate/cv/accent, so
more of the articulation IS on the wire than Lantern.cpp:23's old comment assumed ("poly exposes
only gate+pitch+accent"). A scope reading all 5 could RECONSTRUCT much of the note type. So the
argument is NOT "the wire can't carry it." It is two sharper points:
1. GROUND TRUTH vs RECONSTRUCTION. The engine HAS the classification as a first-class value
   (GateState::NoteType: Single/Tie/Legato). Reading it directly is ground truth. Reconstructing
   it from jacks (gate + legato-flag + pitch-changed -> infer Tie/Legato) is a re-derivation that
   can DISAGREE with the engine at edges -- and edges are exactly where Lantern earns its keep as
   a DEBUG tool. A jack-reconstructing Lantern would show its OWN reconstruction, masking the very
   note-type-assignment bug you're hunting. Internal-state Lantern shows what the engine THINKS,
   which is what you need to debug the engine.
2. PROVENANCE (decisive for a re-router). Intertropical COLLAPSES and RE-ROUTES voices
   (slot->output, fan-out, per-output transpose). The jacks give the resulting voltages on output
   N but have LOST the provenance: you can't tell from the wire that output 3 is voice 7's line
   (not voice 12's), or that it's transposed +7. That routing detail exists ONLY internally,
   post-collapse the outputs are anonymous. For a re-routing module, internal state is the only
   place "which voice, which transpose, feeding which output" still exists.
So: not "honest but impossible" -- rather, internal state is ground-truth (not a bug-masking
reconstruction) AND carries the routing provenance the collapsed jacks have discarded.

### The model: Lantern reads Intertropical's ROUTED articulation, by adjacency
Lantern must read Intertropical's INTERNAL routed state, the same way it reads Monsoon's engine
state -- by expander adjacency, not a cable. The whole point of Lantern being an EXPANDER rather
than a scope is that it sees what the wire can't.
- Intertropical already computes, per output channel, which source VOICE is routed there
  (computeRouting: slot -> voice -> output). For Lantern it must ALSO expose, per output channel,
  that voice's GateState::NoteType (+ accent, + pitch, + transpose applied) for the current step.
- So Intertropical becomes a RE-ROUTER OF ARTICULATION STATE, not just of gate/CV voltages. It
  carries the NoteType through the routing so an adjacent Lantern can render tie/legato/accent
  per OUTPUT channel, exactly as it does per voice for Monsoon.
- Interface (small, shared): "give me the NoteType/accent/pitch routed to output N this step."
  Lantern gains a SOURCE mode (its existing viewMode enum): "Monsoon engine" (current) vs
  "adjacent Intertropical routed output" (new).

### Attachment = REACHABILITY, not claim (enables multiple pairs)
The claim system (isClaimedExpander, VisualExpanderHelpers.hpp) is one-of-each-type: Monsoon
caches THE Raffles, THE Junction, etc. -- a single cached pointer per type. Multiple
Intertropicals (and multiple paired Lanterns) break that assumption outright; you cannot have a
single cachedIntertropical slot for three of them. So the claim model is the WRONG tool here.
- Intertropical and Lantern are OBSERVER/OUTPUT modules (Lantern is already a "PURE OBSERVER",
  Lantern.cpp:61, NOT in the claimed list). They attach by being REACHABLE in the chain
  (findMonsoonEitherSide / adjacency), which is count-agnostic -- any number of instances works.
- CONNECT-MARK FIX falls out of this: Intertropical's connect mark never lights because
  isConnectedAndclaimed requires isClaimedExpander, and Intertropical isn't (and shouldn't be) in
  the claimed list. Its mark should light on REACHABILITY (a Monsoon/Straits system is findable
  upstream), like the observer it is -- not on being claimed. (Fix: give Intertropical's mark a
  reachability-based `connected` predicate rather than isConnectedAndClaimed.)

### Pairing = ADJACENCY (decided)
A Lantern reads the module immediately adjacent in the chain (the same mechanism it uses to reach
Monsoon). Multiple Intertropical+Lantern PAIRS work by sitting each pair together; the Lantern
shows the neighbour it's next to. No cable, no extra jack -- consistent with how Lantern already
attaches. (Chosen over a cable/patch model because Lantern's detail comes from internal state, not
the wire, so cabling would defeat the purpose anyway.)

### Honest cost
This is a TIGHTER coupling than a scope: Lantern must know Intertropical's exposed state
interface, and Intertropical must carry articulation (NoteType) through its routing, not just
voltages. That is real work on both sides. But it is the ONLY model that delivers the debug/
understanding value -- the cable model is honest and useless; the expander model is coupled and
valuable. Worth the coupling.

## Lantern pairing -- pair numbers + post-transposition CV (DECIDED)

### Post-transposition CV on Lantern's piano-roll view
Lantern shows Intertropical's output. The piano roll must show WHAT PITCHES ARE ACTUALLY SOUNDING
-- which means POST-TRANSPOSITION pitch: `voice CV + params[TRANSPOSE_FIRST + ch] / 12.0f`. Not
the raw engine pitch (which ignores Intertropical's per-output transpose). Consistent with Lantern
showing what the instrument produces, not a pre-output intermediate. This falls naturally out of
the exposed-state model: Intertropical includes the transposed pitch as a field in the per-output
state it exposes to the adjacent Lantern. No architectural complication -- the transpose is already
in Intertropical's params.

### Why the connect mark alone is not enough (Rodney)
With multiple Intertropical+Lantern pairs in a patch, the connect mark tells each module "you are
attached" but does NOT tell which Lantern goes with which Intertropical. The user cannot verify
association at a glance. A PAIR NUMBER shown on both modules of a pair solves this: scan the rack,
match the "2"s, know which Lantern is visualising which Intertropical.

### Pair number: user-assigned on Intertropical, read by adjacent Lantern
- ASSIGNMENT: USER-SET on the Intertropical (context-menu, 1..8 or similar). NOT auto-assigned
  from chain position -- that would be fragile to module reordering and wouldn't survive patches
  consistently. User sets "this is Intertropical pair 3" once.
- PROPAGATION: the adjacent Lantern READS the pair number from its neighbour's exposed state.
  No separate setting on the Lantern -- it inherits, so the pair is always in sync.
- DISPLAY: a small coloured numeral near the connect mark on BOTH modules. Visible at normal
  rack zoom, not competing with the content. The COLOUR encodes the pair identity using
  dot.modular's own palette (pair 1=red d4001a, pair 2=gold c8960c, pair 3=teal 26a69a, etc.)
  so you can match by colour AND number at a glance. When there's only one pair, the number
  can be omitted and just the colour shown.
- CONDITIONALITY: a Lantern in Monsoon-source mode (no adjacent Intertropical) shows NO pair
  number -- the pair concept only exists when Lantern is in Intertropical-source mode. The
  number display is conditional on source mode.

### Connect-mark fix (reachability, not claim) -- applies to both
Both Intertropical's mark AND a paired Lantern's mark should light on REACHABILITY: "can I find a
Monsoon/Straits system upstream?" via findMonsoonEitherSide. NOT on isConnectedAndClaimed (which
requires a claimed-expander slot that doesn't exist for these observer modules and can't scale to N
pairs). The pair number WITH colour serves the secondary function isConnectedAndClaimed was
providing (confirming which specific pair you're looking at); the mark itself just lights/greys on
reachability. Fix: give Intertropical's makeConnectMark a custom `connected` lambda -- see impl
notes below.

## Lantern pairing model -- automatic numbering, Intertropical picks (FINAL)

### Lantern self-assigns its pair number
- On FIRST PLACEMENT: Lantern scans reachable neighbours and picks the lowest number not already
  taken by another Lantern in the chain. It STORES this number in its own dataToJson -- the
  number is PERSISTED, not re-derived from chain position.
- On LOAD: Lantern restores its stored number. Only re-picks if the stored number COLLIDES with
  a neighbour (handles simultaneous placement edge case). Collision self-heals to the next free
  number.
- Result for the USER: "just place a Lantern, it gets a number" -- looks automatic. But the
  number is stable across save/reload and across reordering (each Lantern holds its own number;
  moving one doesn't renumber others unless there's a collision, which self-heals).

### Intertropical chooses which Lantern to pair with
- Intertropical's context menu shows "pair with Lantern N" for each Lantern it can find by
  scanning the chain (search stops at foreign modules -- the universal rule). Stores the chosen
  Lantern NUMBER (not a pointer or position) in its own state.
- On load: finds whichever Lantern currently holds that number and is reachable. If no reachable
  Lantern with that number, shows "unpaired" clearly on the mark.
- SCAN RANGE: whole reachable chain (search stops at foreign modules). Rack patches aren't always
  laid out linearly; the pair-number display on both modules is the visual confirmation of
  association, so distance doesn't need to be constrained.

### Coexistence -- the three attachment models don't interfere
Three models operate simultaneously and independently:
1. CLAIMED expanders (Raffles, Junction, Causeway, Sands, Change Alley, Changi, Shophouse):
   registered in isClaimedExpander, one-of-each-type cached slot in expanderManager. Unchanged.
2. PURE OBSERVER (Lantern in Monsoon-source mode): reads Monsoon engine by reachability, not
   claimed. Unchanged. Connect mark lights on reachability.
3. INTERTROPICAL + PAIRED LANTERN (observer/output pair): reachability-based, numbered pairing.
   Connect mark lights on reachability (already fixed). Pair number on both modules confirms
   which Intertropical a Lantern is paired with.
The claimed-list check and the reachability check are INDEPENDENT queries. Foreign-module search
termination is the GLOBAL rule applying to all three uniformly -- no special casing needed.

### Width / rack layout note (Rodney)
14+ modules at 20-42HP each is a wide rack. Accepted constraints:
- Other brands ship wide modules; this is not unusual for feature-rich instruments.
- VCV Rack v2 does not support expanders chaining across rack rows -- a PLATFORM LIMITATION the
  whole community accepts, not something dot.modular can solve. The connect mark (lit/greyed) is
  the patch-level signal that the chain is intact. Users lay out accordingly.

### Pair display (both modules)
A small coloured numeral near the connect mark on BOTH the Intertropical and its paired Lantern.
Colour encodes pair identity from dot.modular's palette (pair 1=red d4001a, pair 2=gold c8960c,
pair 3=teal 26a69a...) so you can match by colour AND number at a glance. A Lantern in
Monsoon-source mode (no paired Intertropical) shows no pair number. The display is conditional on
Lantern being in Intertropical-source mode and having a confirmed reachable pair.

## Per-scene voice->slot seating override (RETAINED)

The auto-pack default (voices fill slots 1..N in voice-number order) covers most cases. But the
override mechanism is RETAINED so the user can explicitly seat a specific voice in a specific slot
for a given scene -- essential when the global slot->output routing assigns a particular role to a
slot (e.g. slot 3 -> outputs 1+6, both chord tones) and you need to CONTROL which voice fills
that role per scene, not leave it to auto-pack order.

The earlier note that sceneOutput[scene][voice] was "superseded" by the new two-layer model was
wrong: it survives, but its SEMANTICS change. It is no longer a direct voice->output override
(which the new slot->output global grid covers); it is a per-scene voice->SLOT seating override.
sceneOutput[scene][voice] = slot (0..7) meaning "seat voice V in slot S for this scene."
-1 (default) = auto-pack in voice order.

This is the mechanism that makes the slot->output routing grid precise: you set "slot 3 ->
outputs 1 and 6" globally once, then per scene you control WHICH voice fills slot 3 (and
therefore which voice feeds those outputs) via the seating override. Without it the global
routing grid's slot assignments are approximate (you get whatever voice auto-pack puts there).

UI: the existing right-click cycle mechanism on membership cells is the natural home for this --
right-click a member cell to cycle its slot assignment (Auto -> slot 1..8 -> Auto). The current
scroll-a-number override should be replaced with a more legible display (the voice->slot
visualiser grid shows the result live, which helps).

---

## LANTERN "Intertropical source" mode -- concrete build plan (reverse-engineered, ready)

Confirmed with Rodney: once Lantern is connected (existing Lantern<->Monsoon/Intertropical discovery
spec), in this mode Lantern reads from Intertropical's OUTPUT side -- the routed poly outputs WITH
per-output transpose applied -- and renders it EXACTLY like it renders Monsoon (same cells[][] +
recordCell + grid/piano views). "Reuse the render, add a source."

### Data source (exact, from Intertropical.cpp process)
Per scene, Intertropical routes voice v -> output channel ch = routing[v] (computeRouting(activeScene)).
For each routed channel ch, the OUTPUT values (post-routing, post-transpose) are:
- CV/pitch:  straits->outputs[3].getVoltage(v) + params[TRANSPOSE_FIRST+ch].getValue()/12   (1V/oct)
- Gate:      straits->outputs[0].getVoltage(v)
- Accent:    straits->outputs[4].getVoltage(v)
- Legato:    straits->outputs[1].getVoltage(v)
- SLEG:      straits->outputs[2].getVoltage(v)
The per-output TRANSPOSE (TRANSPOSE_FIRST+ch, -24..+24 semis, snap) MUST be included in the pitch so
the display matches what physically leaves the CV_OUT jack. Voice identity for display = the OUTPUT
CHANNEL ch (not the source voice), so the picture is the arranged result, 8 channels.

### Sink (reuse Lantern's pipeline)
Lantern already maps engine state -> displayable cells via recordCell(voice, step, gs, dec, accented,
lenSteps, slur..., playDir): pitchV -> row, gate -> sounding, lengthSteps -> bar width. The IT-source
path fills cells[ch][step] from the routed output above instead of Monsoon's engine:
- sounding = gate high (same rule as recordCell's gs.gateHeld).
- pitch row from CV (incl. transpose) -- reuse the piano-roll's existing semi->row mapping.
- accent/legato/SLEG map to the same articulation the role-colour path already draws.
- length: derive from gate-high span like the Monsoon path (or the legato/SLEG gate), same as today.

### Timing
Read membership/routing AT THE PHRASE BOUNDARY, same as Intertropical's own routing (activeScene is
updated at the boundary in process). The display must switch WITH the audio, not ahead -- so sample
the routed output on the same boundary Intertropical uses, feeding recordCell at the step write like
the Monsoon path. Distinguish routed-IN vs muted-OUT channels (hollow/dim) echoing the IT grid cell
convention so display + sequencer read consistently.

### Source selection
A Lantern source toggle (store-backed, like viewMode/rollView): "Monsoon raw" vs "Intertropical
routed". When IT-routed and an Intertropical is found (same discovery as Monsoon), Lantern's record
path reads Intertropical's output; else falls back to Monsoon. Grid AND piano-roll both work off the
populated cells[][] with zero render changes (that's the whole point -- add a source, not a view).

### Scope / order (fresh session)
1. Source toggle param + Intertropical discovery/accessor (mirror findMonsoonEitherSide).
2. IT-source record path: sample routed+transposed output at boundary -> recordCell into cells[ch][].
3. Routed-in vs muted-out visual distinction (hollow/dim).
4. Verify BOTH grid and piano-roll render the arranged output correctly in Rack.
A few hundred lines, mostly the record path; renders are reused. Build on the finished de-param/lock
substrate (already in place).
