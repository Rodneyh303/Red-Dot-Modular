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
