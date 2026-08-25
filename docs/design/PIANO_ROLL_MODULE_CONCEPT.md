# New module concept: editable piano-roll pitch+gate source (Rodney, holiday)

A brand-new module: an editable piano roll that, in Monsoon's quantiser modes, BECOMES the gate + poly
CV input -- so you draw a deterministic pattern and the engine (dice/scrub/spread/probability/
correlation) gives you variations. Reference paradigm: Voxglitch Piano Roll (8-track poly piano-roll
sequencer, GPL-3, ~79k popularity) -- but ours is different in kind (see below).

## What makes it distinct from Voxglitch (and worth building)
Voxglitch = a self-contained sequencer (draw, it plays). Ours = a DETERMINISTIC PATTERN SOURCE feeding a
GENERATIVE TRANSFORMER. The key line (Rodney): "a way to put in a deterministic pattern and get
variations." The roll is the SEED; Monsoon's engine is the GARDEN. Not "editable Lantern" -- it's a
deterministic pitch+gate source that plugs into the generative engine so you compose a skeleton and let
the system flesh it out. On-brand for a "sequencer transformer."

## Architectural placement (uses the shared-resource criterion from this trip)
Rodney framed it two ways that must be reconciled: "editable Lantern" vs "becomes the gate+CV input."
Those are OPPOSITE dataflow:
- Lantern = an OBSERVER (shared, read-only display; watches output). Genuinely shareable.
- This = a SOURCE (drives the gate+CV input). Opposite direction.
So it is NOT "editable Lantern" (that would turn an observer into a driver and muddy Lantern's clean
read-only role). It's a NEW module that BORROWS Lantern's roll/grid visual language but is
architecturally a PER-MONSOON CV+GATE SOURCE -> by the shareability criterion, 1:1 with its Monsoon
(like Colonnades, like Sands panels), NOT shared (like Lantern). Fits Rodney's own "edit each voice like
Sands panels" -- Sands panels are per-Monsoon authoring surfaces. This is the Sands-panel pattern applied
to a piano roll, feeding quantiser-mode input.

## Spec (Rodney's sketch, structured)
- 16 on/off steps per voice, 8 voices max = a 16x8 gate grid (8 of Monsoon's 16 poly voices).
- One octave shown + a per-voice OCTAVE STRIP to set the octave. Smart compression: keeps the panel
  compact (full piano rolls are tall) by separating pitch-class from octave. A real UI win vs Voxglitch's
  big roll.
- 12-TET roll OR microtonal roll: the roll's ROWS are SCALE DEGREES, not fixed semitones. 12-TET mode =
  12 rows; microtonal mode = N rows = the loaded tuning's degrees. The roll INHERITS the current
  tuning/scale -> draw on the degrees of the maqam, not on piano keys. The microtonal piano roll almost
  nobody has, and the natural thing for THIS ecosystem.
- Per-voice FOCUS editing (Sands-panel style): edit one voice at a time, full attention, not a cramped
  all-voices grid.
- Syncs to Monsoon in quantiser modes, becomes gate + poly CV input: the roll's steps ARE the poly CV +
  gate Monsoon quantises/transforms. Deterministic pattern enters the engine, comes out varied.

## THE deep question to resolve before building (park, don't solve on holiday)
Where does the pattern live, and WHO CLOCKS IT? The roll has 16 steps; Monsoon's engine has its own
clock/counter/phase. Does the roll's playhead follow MONSOON'S signed step counter?
LEAN: YES -- and that's the magic. If the roll is INDEXED BY MONSOON'S COUNTER, then dice-scrub / reverse
/ phase navigate the DRAWN pattern too, and the probability/spread layer varies it. The roll becomes the
deterministic backbone the navigable-randomness system plays AROUND -- which is exactly "deterministic
pattern -> variations." It falls out of feeding the roll through the same counter-addressed engine.
Confirming the roll indexes off Monsoon's counter (vs its own independent clock) is THE design decision:
it's what makes this a sequencer-transformer INPUT rather than just another piano roll.

## Open questions for later
- Does the roll REPLACE the external poly CV in quantiser mode, or is it one selectable source among
  external CV inputs? (Probably: the roll IS a poly CV source, routed to the same quantise stage as
  external CV -- consistent with "swappable pitch source".)
- 16 steps fixed, or does it follow Monsoon's pattern length / phase window?
- How do melody PINS interact? (Pins reroute WHICH source each voice reads -- with a roll source, pins
  could route voice N to read roll-lane M, i.e. correlate the drawn voices. Consistent with pins as a
  source-agnostic N-to-M router.)
- Recording (Voxglitch has it)? Probably out of scope for v1 -- draw, don't record.
- Panel: borrow Lantern's roll aesthetic (grid, note blocks) but it's a per-Monsoon authoring surface,
  so 1:1 attach like Sands/Colonnades, not a shared observer.

## Status
Brand-new module concept, parked from holiday. NOT v1-critical -- a strong post-launch or v1.x addition.
The novel core (deterministic roll seed -> generative variation via the counter-addressed engine) is the
reason to build it; everything else is a well-understood piano-roll UI. Reference: Voxglitch Piano Roll
(clone45/voxglitch, GPL-3) for the draw-on-grid paradigm only; ours is architecturally different
(per-Monsoon source feeding the transformer, microtonal rows, counter-indexed for variation).

Cross-ref: QUANTISER_MODES_UNIFICATION (swappable pitch source; the roll is one), SHAREABILITY_ANALYSIS
(per-Monsoon source = 1:1, not shared like Lantern), the pins-as-source-router note, DICE_SCRUB_MODEL +
PHASE_ENGINE_AUDIT (the counter-addressed navigation the roll would ride on), Sands panels (the per-voice
focus-edit pattern), Lantern (visual language borrowed, role NOT shared).

## NARROWED (Rodney): the roll is PITCH CV ONLY, and it's built AFTER the 3 quantiser modes are final

Two corrections to the framing above:

### 1. Pitch CV only -- not gate+CV
The roll provides ONLY the PITCH CV for the 16 steps (the "what pitch" for each step position). Timing +
gating still come from Monsoon's engine in whatever quantiser mode. So the roll is NOT a self-contained
sequencer feeding what-AND-when -- it's JUST the pitch source; Monsoon supplies the "when". This fits the
quantiser unification exactly: the roll is simply the SWAPPABLE PITCH SOURCE. Three pitch sources into
one timing/gating/phrasing engine: external CV, internal draw, or (now) a drawn 16-step pitch pattern.
The roll is a third pitch source, not a parallel sequencer.

Consequence: "who clocks the roll" mostly DISSOLVES -- MONSOON clocks it. The roll isn't clocking
anything; it supplies pitch for each of the 16 step positions and Monsoon's counter decides which step
is read when. The roll is pitch DATA indexed by Monsoon's step position. (The nice dice-scrub/reverse/
phase-navigates-the-drawn-pattern behaviour still follows automatically, precisely because Monsoon's
counter does the indexing.)

### 2. Build ONLY after all 3 quantiser modes are finalised
The roll IS a quantiser-mode pitch source, so its behaviour is defined by how the quantiser modes consume
pitch (poly CV routing, per-voice source handling, how pitch enters the quantise stage). Building the
roll before the quantiser modes (C/D, and E/F as they land) are pinned = building a source for a socket
whose shape isn't final. So the roll DEPENDS ON the quantiser modes being done; sequence it strictly
after them. It plugs into the FINISHED quantiser pitch-input contract.

### Net simplified concept
A 16-step x 8-voice grid of PITCH values (scale degrees, 12-TET or microtonal) that presents to Monsoon's
quantiser modes as poly pitch CV -- Monsoon does timing/gating/variation/correlation exactly as with any
external CV. The roll is a pitch-pattern EDITOR whose output is CV, sitting in the same slot external CV
sits in. Dependency-ordered AFTER quantiser-mode finalisation. Everything else in the concept above
(microtonal rows = degrees, one-octave + octave strip, per-voice Sands-style focus edit, 1:1 per-Monsoon
attach) stands.

## Companion concept (Rodney): a separate GATE sequencer editor for gate quantiser mode

Since the roll is PITCH-only, gate-driven quantiser mode (Mode D) needs its "when" from somewhere -> a
SEPARATE gate sequencer editor. Clean what/when split (the two concerns a normal sequencer fuses, each
given its own editor):
- Piano roll editor -> PITCH CV (the "what"), into the quantise stage.
- Gate sequencer editor -> GATE pattern (the "when"), into the gate input in gate quantiser mode (Mode D).

Together they can drive a Monsoon in gate-quantiser mode entirely from hand-drawn patterns: gate editor
says when, roll says what pitch, Monsoon's engine still does variation/correlation/transformation on top.
Two independent deterministic seeds (rhythm + pitch), both varied by the same engine. Decoupling what from
when yields all the combinations from two simple editors: fixed rhythm + generated pitch, fixed pitch +
rhythm elsewhere, or draw both and let dice/scrub/spread vary the pair.

### Same architectural class as the roll
Per-Monsoon authoring surface (1:1, Sands-panel pattern); a deterministic source feeding an input the
engine consumes; indexed by MONSOON'S counter so scrub/reverse/phase navigate the drawn GATE pattern too.
The gate twin of the pitch roll -- inherits the roll's placement, 1:1 attach, and build-after-the-mode
dependency.

### The fork to decide later: one module or two?
Gate grid and pitch grid are visually similar (16x8), tempting to FUSE into one "pattern editor" doing
both. But that fusion is exactly what the pitch-only narrowing just DE-fused. LEAN: keep them separate
(matches the committed what/when split) -- but it's a real design fork ("two narrow tools vs one combined
roll"); the combined version has a seductive convenience that might justify the coupling. Conscious
decision at build time, not now.

### Dependency
Gated on the GATE quantiser mode (Mode D) being finalised, same as the roll is gated on the quantiser
modes generally. Sequence after Mode B/D are pinned.

Cross-ref: the pitch-roll concept above (this is its gate twin), MODE_B_SPEC / MODES_C_D_QUANTIZER
(the gate quantiser mode this feeds), SHAREABILITY_ANALYSIS (per-Monsoon 1:1 source), DICE_SCRUB_MODEL /
PHASE_ENGINE_AUDIT (counter-addressed navigation of the drawn gate pattern).

## CORRECTION + design points (Rodney): gate editor is MONO, clock/phase-linked, resolution matters

### Gate editor is ONE channel, not 8 (corrects the "gate twin" symmetry)
Gate/CV quantiser modes take a MONO gate in (confirmed earlier from code: Monsoon.cpp:527 getVoltage not
getPolyVoltage; the gate is the SHARED step boundary, one gate for all voices, poly-ness comes from Sands
per-voice rules at each step). So the gate sequencer feeding it is ONE channel = a single 16-step gate
lane, NOT a 16x8 grid. The roll is per-voice (8 pitch lanes); the gate editor is one shared rhythm lane.
They are NOT symmetric twins -- they match the actual input shapes: poly pitch CV (8 lanes) + mono gate
(1 lane). The architecture sets the gate editor's dimensions.

### Design point 1: link gate to clock or phase (not self-clocked)
The 16-step gate lane needs a time-base; same question/answer as the pitch roll -- driven by MONSOON'S
clock or phase, not its own. The lane is READ OUT by Monsoon's step counter (clock-driven) or by phase
(phase-driven). Clock mode: step advances per tick. Phase mode: phase position selects the step.
Consistent with the phase engine and with how the roll is counter-indexed. "Link gate to clock or phase"
= the gate lane is addressed by the same timing source the mode uses.

### Design point 2: resolution (the new consideration the gate lane raises)
"16 steps" is meaningless without step-length. Decide:
- Fixed division (each step = 1/16 -> 16 steps = one bar), OR settable step length (1/16, 1/8, triplet
  ...) so the same 16 steps span different durations.
- Gate WIDTH / per-step state: not just on/off. For the engine's legato/tie behaviour, a step likely
  needs TRIGGER / TIE(hold/sustain) / REST -- because Monsoon's engine cares about legato (tied steps)
  vs re-articulated steps. A pure on/off lane LOSES the tie information the whole legato/re-articulation
  system (Keppel within-legato gate, Monsoon step-legato) depends on. So the per-step vocabulary should
  probably be trigger / tie / rest (on / hold / off), not just on / off -- else it can't express the
  phrasing the engine is built around.

### Net (revised gate editor)
A SINGLE 16-step gate lane, each step trigger / tie / rest (not merely on/off), read out by Monsoon's
clock OR phase, with a settable step resolution. Mono (narrower than the roll) but with its own subtlety
(resolution + tie-state) the roll lacks. Still per-Monsoon 1:1, still built after the gate quantiser mode
(Mode D) is final. The "one module or two" fork above stands -- though the mono-gate vs poly-pitch shape
difference is now an argument FOR two separate editors (different dimensions + different per-step
vocabularies).

Cross-ref: MODE_B_SPEC (mono gate input, Monsoon.cpp:527), the Keppel within-legato gate + Monsoon
step-legato / re-articulation work (why tie-state matters), PHASE_ENGINE_AUDIT (clock vs phase readout),
the pitch-roll concept (poly pitch, the shape contrast).

## SCOPING (Rodney): gate editor is for GATE + PHASE quantiser modes only, NOT clock mode

Refinement: the gate editor is used ONLY in gate and phase quantiser modes -- NOT clock mode. In CLOCK
quantiser mode Monsoon GENERATES the gates itself (clock drives internal gate generation), so there's
nothing for a gate editor to supply -- it'd be redundant. The editor only applies where gate/timing comes
from OUTSIDE the internal clock generation:
- Gate quantiser mode (D): driven by an external gate; the editor supplies that gate pattern.
- Phase quantiser mode (F): driven by phase-in; the editor's lane is read out by phase position.
- Clock mode (C): Monsoon makes its own gates from the clock -> gate editor NOT used.

Maps onto the mode taxonomy: seq A(clock)/B(gate)/E(phase) <-> quant C(clock)/D(gate)/F(phase). The gate
editor belongs to the D + F side (external timing), never the C side (internally generated). The mode
taxonomy defines where the editor applies: modes that TAKE external timing, not those that GENERATE it.

### Readout per timing source (Rodney: "driven by gate and phase-in according to mode, assuming the gate
### is a regular clock")
The 16-step lane is READ OUT by the mode's timing input:
- Gate mode: the incoming GATE advances the lane -- ASSUMING that gate is a regular clock (steady
  pulses). Each pulse advances to the next step of the drawn lane; the external gate is the clock that
  walks the playhead through the 16-step pattern.
- Phase mode: phase-in selects the step (phase position -> step index).

KEY caveat ("assuming the gate is a regular clock"): if the incoming gate is IRREGULAR (not steady --
syncopated/arrhythmic), "advance one step per gate" RE-TIMES the drawn pattern rather than playing it at
its drawn rhythm. That may be a feature (rhythmic re-interpretation) or a footgun (doesn't sound as
drawn), by intent. So the editor is clean/predictable fed a regular clock-like gate, and becomes a
re-timing transformation fed an irregular one. Be explicit in the manual: the lane advances PER GATE
EVENT, not per unit time.

### Conceptual clarity
The gate editor is "a gate pattern that the external timing WALKS THROUGH." What "walking through" means
depends on the mode's timing source: steady gate -> plays as drawn; irregular gate -> re-timed; phase ->
scrubbed. Same lane, three readout behaviours, determined by the mode. Consistent with the roll
(counter-indexed) and the phase engine -- the editor is another addressed pattern; the ADDRESS SOURCE is
the mode's external timing.

Cross-ref: QUANTISER_MODES_UNIFICATION (the A/B/E <-> C/D/F taxonomy; C generates, D/F take external),
PHASE_ENGINE_AUDIT (phase readout), MODE_B_SPEC (external gate handling), the pitch-roll concept
(counter-addressed sibling).

## Gate editor: variable steps + RESOLUTION / triplets (Rodney, birthday, Berlin-bound)

Inspiration: Bitwig gate sequencer -- variable steps you can JOIN to get different note lengths; one
phase cycle mapped over a 16-res grid, 24-res grid, etc.

### Reframe: decouple "number of gates" from "grid resolution"
"16 gates of whatever length for one phrase" -> the 16 is the number of NOTE EVENTS, not grid cells. Up
to 16 notes, placed on a finer underlying grid, each with a LENGTH (joining cells = a held note, the
trigger/tie/rest state). Variable step count + "join for length" are the same idea: a note spans a
variable number of grid cells. 16 = a note budget; the grid underneath is separate and finer.

### Triplets = a divisibility question (the direct answer)
A 16-cell grid is 2^4 = pure binary: halves/quarters/8ths/16ths yes, TRIPLETS NEVER (no factor of 3).
- Triplets REQUIRE a resolution divisible by 3. Non-negotiable.
- 24/bar (6/beat): 16th-triplets, 8th-triplets, 8ths, quarters -- but NOT clean straight 16ths (4/beat;
  6/4 not integer).
- 48/bar (12/beat): the magic number -- holds BOTH straight 16ths (every 3rd cell) AND 16th-triplets
  (every 2nd cell) in the SAME grid. 12/beat = LCM(4,6).

### The fork to decide
- Selectable resolution per lane/phrase (16, 24, 48...): simpler, switch straight<->triplet, can't easily
  MIX in one phrase. Matches Rodney's "phase cycle over 16 or 24 grid" (Bitwig picks the grid the phase
  divides into).
- One high resolution (48): place straight AND triplet notes freely in one phrase; denser grid to edit.

### Readout mapping
- Phase mode: phase 0->1 sweeps the R cells; note high while phase in [start, start+length). Variable
  phrase length is free -- one phase cycle IS the phrase, whatever the cell count.
- Gate mode: refines the "regular clock" caveat -- the incoming clock should run AT THE RESOLUTION RATE
  (one pulse per cell), notes span multiple pulses. Triplets -> clock at 24 or 48 ppq-equivalent.

### Net
Variable-length notes (up to ~16) on a resolution grid; resolution must be divisible by 3 for triplets,
48/bar to mix straight+triplet cleanly. Selectable-resolution vs single-high-res is the open fork.
Phrase = one phase cycle (phase mode) or resolution-rate clock (gate mode). Still gate+phase modes only,
still per-Monsoon 1:1, still after quantiser-mode finalisation.

Cross-ref: the gate-editor scoping (gate+phase modes), the trigger/tie/rest per-step vocabulary (join =
tie across cells), PHASE_ENGINE_AUDIT (phase sweeps the grid), DICE_SCRUB_MODEL (counter-addressed).

## Gate editor layout: STACKED ROWS (mono unlocks it) + edgeless expander overflow (Rodney)

Two ideas for "more steps without resizing" (Rack modules are fixed-width; no drag-resize):

### A. Edgeless expander in 16s
Base holds 16 steps; an EDGELESS expander (seamless panel, no visible gap/edge with the host, so it
reads as one continuous module) adds another 16, and another. Idiomatic Rack ("more room = attach") with
the visual continuity of a resized module -- fakes Bitwig drag-resize via seamless expanders. On-brand
(expander-land).

### B. Stacked rows on one panel -- the MONO insight (stronger for the gate editor)
The gate lane is MONO (one channel) -> a single 16-step lane uses one horizontal strip, leaving the
panel's VERTICAL space free. So WRAP the pattern into rows: 3-4 rows x 16 = 48-64 steps stacked on ONE
fixed panel, like text wrapping to the next line. A POLY editor couldn't do this (voices want the
vertical axis); the mono gate editor CAN, precisely because it's mono. KEY INSIGHT: mono frees the
vertical axis for step-wrapping.

Advantages over the expander here:
- No attach step, no width sprawl. 64 steps in a compact rectangle vs a very wide strip (a 64-step
  horizontal line means constant sideways rack-scrolling; 4x16 stacked is glanceable).
- The phrase reads as a BLOCK -- see the whole pattern at once, better for drawing/editing rhythm than a
  long thin line.
- Resolution/triplet friendly: maps onto the 48-per-bar grid (a row per beat/bar).

### How they layer (not competing)
Rows FIRST (free, compact, mono-enabled) for the common cases -- up to 4x16 = 64 steps in one module
(64 gates is a lot of phrase). Edgeless expander as OVERFLOW if someone needs even more. Likely no
expander needed for v1 if 4x16 covers realistic phrase lengths.

### Open question (park): what do the rows MEAN?
(a) Pure wrapping -- 64 steps in sequence, rows are visual line-breaks only (simpler, flexible), OR
(b) Meaningful -- each row = a bar/beat, row boundary is rhythmic (readable, imposes structure).
Lean (a) with optional bar-line markers. Interacts with resolution: row length = your resolution unit
(e.g. row = one bar at 16th res -> 4 rows = 4 bars; triplets -> row length = the triplet resolution).

Cross-ref: the resolution/triplet note (48-per-bar, row length), the gate-editor mono correction (why
stacking works), the fixed-width Rack constraint (why not drag-resize), expander pattern (Changi/Causeway
/etc -- the overflow idiom).

## Row-length options (CORRECTED: 26 was a typo -> 16) -- ties to the triplet math (Rodney)

Rodney floated: 3 rows x up to 26 (=78 steps) OR 2 rows x up to 24 (=48 steps). Evaluated against the
resolution/triplet work:

### 26 is a musically awkward row length
26 = 2 x 13 (13 prime) -- doesn't divide into beats/subdivisions cleanly. If a row represents a musical
span (bar / two bars), 26 maps onto no standard meter or subdivision. Fine only if 26 is "as many cells
as fit the panel width" (headroom), not a musical length -- but then the pattern length users actually
pick should still land on musical numbers (16, 24, 32...).

### 24 is the strong number (triplet-friendly, from the resolution note)
24/row = 6/beat in 4/4 -> 16th-triplets, 8th-triplets, 8ths, quarters. A 24-step row = one bar of
triplet-grid. And 2 rows x 24 = 48 = the "magic" resolution: two bars of triplet-grid, OR the full mixed
straight-16th + 16th-triplet resolution for one bar. Musically coherent where 26 isn't.

### Steer
- Row length should be MUSICAL: 24 (triplet-capable, also covers straight via the 48-mix) or 16
  (straight-only). NOT 26.
- Want the higher step count of the 3-row layout? Use 3 x 24 = 72 (three bars triplet-grid, more steps
  than the 2-row option) or 3 x 16 = 48. Both musical.
- Rows count (2 vs 3) = a panel-height + phrase-length question. Step WIDTH is the UI constraint: 24
  cells across must stay a comfortable click target; if 24 is too narrow, drop to 16-wide rows.

LEAN: 3 rows x 24 = 72 if panel height AND step-width allow (triplet-capable, generous); fall back to
2 rows x 24 = 48 for a compacter module. Drop 26 -- it's the odd one out (literally, and non-musical).

Cross-ref: the resolution/triplet note (24 = triplet grid, 48 = mixed magic), the stacked-rows layout
(mono enables it), step click-target UI concern.

## CORRECTED options: 3x16 (straight) vs 2x24 (triplet) -- both = 48; maybe a TOGGLE (Rodney)
(The earlier "26" was a typo for 16.) Real options: 3 rows x 16 OR 2 rows x 24 -- BOTH = 48 steps total.
So it's not about step count (identical); it's about which MUSICAL GRID you offer:
- 3 x 16 = 48: three rows of STRAIGHT/binary grid (16 = 2^4). Binary rhythms; can't do triplets.
- 2 x 24 = 48: two rows of TRIPLET-capable grid (24 = 6/beat). Triplets + (via the 48-mix) straight too.
Same 48 cells, DIFFERENT rhythmic vocabulary: 3x16 = clean binary sequencer; 2x24 = triplet/mixed.

### The elegant move: make row-layout a TOGGLE (straight <-> triplet)
Because both totals are 48, the same module can RE-FLOW 48 cells between 3x16 (straight) and 2x24
(triplet) via a toggle. Cell count constant; only the grouping changes. Directly mirrors Rodney's "one
phase cycle over a 16-res grid or a 24-res grid" -- the phase cycle divides into either arrangement, the
toggle picks which. Most flexible answer, clean because the numbers cooperate (48 = 48).

Tradeoff: a FIXED layout (pick one, ship it) is simpler to build + read; a mode-switch that reflows the
grid is more code + a little "which mode am I in?" load.

### Steer
- Pick ONE, simplest, mostly straight time: 3 x 16.
- Pick ONE, want triplets: 2 x 24 (more CAPABLE grid for the identical footprint -- reaches triplets that
  3x16 can't, and 48 cells still express straight fine). The better single choice if choosing one.
- Max flexibility: offer BOTH via a straight/triplet TOGGLE reflowing 48 cells between 3x16 and 2x24.
LEAN: 48 cells, switchable 3x16 (straight) <-> 2x24 (triplet) -- the truest realization of the
phase-cycle-over-16-or-24-grid idea. If no toggle, 2x24 as the single more-capable default.

Supersedes the 26-based discussion above (26 was a typo).

## THE constraint: step counts must fit the 24-PPQN phase mapping (Rodney) -- can't just copy Bitwig 64/odd

Bitwig: up to 64 steps, arbitrary/odd counts -- because Bitwig's steps DEFINE the timing (each step
triggers; step count sets the division freely). OURS is the opposite: steps must LAND ON the existing
24-PPQN phase grid the phase engine reads. So not every step count is representable.

### The divisor math (assuming 24 PPQN, 4/4 bar = 96 pulses)
A step count N works cleanly iff the phrase's pulse total divides evenly by N (else steps fall BETWEEN
phase-pulses -> unrepresentable or drift).
- 16 steps -> 96/16 = 6 pulses/step ✓ (16th)
- 24 steps -> 96/24 = 4 ✓ (16th-triplet -- THIS is why 24 works, it's the triplet grid at 24 PPQN)
- 32 steps -> 96/32 = 3 ✓ (32nd)
- 48 steps -> 96/48 = 2 ✓
- 64 steps -> 96/64 = 1.5 ✗ DOES NOT DIVIDE (Bitwig's max doesn't fit our mapping!)
- most odd counts (7,13,26): 96/N non-integer ✗
Clean counts per 4/4 bar = divisors of 96: 1,2,3,4,6,8,12,16,24,32,48,96. (16 and 24 both present -> our
two layout options work; 64 is NOT a divisor.)

### FINDING: cannot copy Bitwig's "up to 64, any odd count" as-is
The 24-PPQN phase mapping only represents step counts that divide the phrase's pulse total.

### Resolution options (a real fork)
1. Restrict step counts to DIVISORS of the phase resolution (...16,24,32,48...). Cleanest, most faithful
   to the phase-driven arch; every step exactly on grid, no drift. Loses arbitrary odd counts.
2. Raise the PPQN. 64/bar needs a resolution divisible by 64; 96 isn't, 192/bar (48 PPQN) -> 192/64=3 ✓.
   Cost: finer phase counter, still won't cover EVERY odd count.
3. Decouple step-count from grid: N steps map step k -> phase k/N (even division of the phrase), ROUND to
   nearest 24-PPQN pulse at readout. Gets Bitwig's freedom at the cost of ROUNDING drift (odd/64-step
   patterns snap to nearest pulse -- often fine for gates, a pulse or two jitter, but breaks
   "exactly-on-grid").
4. Per-PHRASE phase (not per-bar): if the PHRASE is one phase cycle, N steps just divide the phrase into
   N -> 64/odd counts work, as long as phase is phrase-relative and not aligned to a bar's pulse grid.
   The Bitwig model (phrase = the loop); sidesteps the divisor problem.

### THE question to resolve (don't guess): is our phase cycle the BAR or the PHRASE?
- Bar-locked (fixed 96 pulses @ 24 PPQN): step counts MUST divide 96 -> option 1 (restrict) or 2 (raise
  PPQN). No free 64/odd.
- Phrase-relative (phrase length flexes, is itself one phase cycle): arbitrary/odd counts work -> option
  4, closest to Bitwig.
This is the crux and depends on how the 24-PPQN->phase mapping is actually defined in the engine. Confirm
against the phase engine before choosing.

Cross-ref: PHASE_ENGINE_AUDIT (how 24-PPQN maps to phase -- the bar-vs-phrase question), the resolution/
triplet note (24 = the triplet divisor of 96), the 3x16-vs-2x24 options (both divisors of 96, why they
work).

## CORRECTION (Rodney + code): PPQN is a settable dial; "bar vs phrase" was a false question

I overcomplicated this. Confirmed from code (SequencerEngine.cpp): ppqnSetting = 24 (:131); comment
:322-324 "PPQN is now always 24/48/96, all of which resolve every note value to an integer pulse count
(24 already covers 1/32 and all triplets). So every value is legal." Rodney: 24 PPQN -> 96 per bar was
chosen precisely to handle ALL note lengths in clock mode, then phase mode is a convenient discretisation
of that same grid.

### Scratch the "bar vs phrase" question -- it doesn't map to anything real
The engine has no bar-vs-phrase ambiguity; it has a PPQN that DISCRETISES PHASE. Phase is discretised to
the PPQN grid (96/bar at 24 PPQN); note lengths + steps are integer pulse counts on it. There is no
separate "is the cycle a bar or a phrase" axis -- that was invented. Remove it from consideration.

### 64 steps: raise the PPQN (it's settable), don't fight a fixed 24
PPQN is ALREADY settable ("always 24/48/96", extensible) -- it's a DIAL you set to make your desired step
count land on integer pulses, not a fixed constraint. My "64 doesn't divide 96" had it backwards: for 64
steps, pick a PPQN where 64 divides. Rodney: "64 PPQN would be divisible by 64" -- e.g. 48 PPQN -> 192/bar
-> 192/64 = 3 ✓, or a higher PPQN as needed. The resolution isn't fought, it's chosen.

### The real (simple) picture -- already in the architecture
- PPQN is settable (24/48/96, extensible).
- Chosen so every note length resolves to integer pulses (24 already covers 1/32 + triplets).
- Step counts work when they divide the per-bar pulse total; since PPQN is SET, raise it to accommodate
  the desired step resolution (64 -> a PPQN where 64 divides).
- Phase mode = a convenient discretisation of that same pulse grid.
So the gate editor's step-count support = "pick/allow the PPQN that makes the chosen step count land on
integer pulses." Bitwig's 64/odd is reachable by choosing PPQN accordingly; the earlier 4-option fork +
crux question is superseded by this.

Supersedes the "THE constraint / bar-vs-phrase" section above (the divisor math there is still correct
FOR A FIXED 24 PPQN, but PPQN is not fixed -- it's the dial).

Cross-ref: SequencerEngine.cpp:131 (ppqnSetting=24) + :322-324 (always 24/48/96, every value integer),
NoteValues.hpp (allowedPPQN, single source of truth), ClockEngine::pulsesPer16th, PHASE_ENGINE_AUDIT
(phase discretised to the PPQN grid).

## HARD CONSTRAINT (Rodney + code): lane length is 1..16, baked into the counter. 16 gates = a bar.

Confirmed from SequencerEngine.cpp -- and it's load-bearing, not a UI choice:
- cachedLength = 16 (:80); length defaults to the "full 16-step window" (:84); lane lengths run 1..16
  (:20-21).
- DNA_LCM = 1441440 = LCM(1..16) x 2 (:30) -- the engine precomputes the LCM of ALL lane lengths 1..16
  (x2 for the 2*16 pingpong period) so every lane period divides the counter wrap and all lanes stay
  continuous across it (:41-47, documented len-16-pingpong edge case needing the factor of 32).
So 16 is baked into the counter arithmetic: the signed reversible counter, reproducible wrap, pingpong/
pendulum periods, the whole navigable-randomness system, all assume lane length in 1..16.

### Consequences for the gate editor (overrides some earlier notes)
1. "Up to 64 steps" is NOT a pick-a-PPQN problem -- it's a "counter is built for 1..16" problem. A
   64-length lane would need LCM(1..64)x2 (astronomically larger, blows up the counter range) or a
   different long-pattern mechanism. So Bitwig's 64-length lane is against the engine's grain.
2. This DECIDES the earlier 3x16-vs-2x24 question (which I'd left as taste): 3x16 = three 16-step LANES,
   natively supported. 2x24 = 24-LENGTH lanes, which exceed the 1..16 max and fight the lane-length
   machinery. So 3x16 is WITH the grain; 2x24 is AGAINST it. Flips my earlier "2x24 more capable" lean --
   2x24 may be more capable musically but it fights the engine; 3x16 is native.
3. Natural gate-editor shape = MULTIPLE 16-STEP LANES/ROWS, not longer lanes. Want a 48-step phrase? =
   three 16-step lanes, each a row, each natively counter-supported. The stacked ROWS aren't just UI --
   they're HOW you get length while respecting the 16-max: each row is its own 16-lane. Architecture and
   UI agree.
4. Triplets come from PPQN WITHIN a 16-lane, not from 24-length lanes. A 16-step lane at a triplet PPQN
   gives triplet TIMING; no 24-length lane needed. LENGTH stays 16 (engine constraint); RESOLUTION is the
   PPQN dial (settable). Orthogonal -- stop tangling them.

### Revised gate-editor shape (respecting the 16-max)
Rows = stacked 16-step lanes (e.g. 3 rows = 48 steps as 3x 16-lanes). Length per lane 1..16 (native).
Triplet/subdivision via the PPQN dial, not via lane length. This resolves the layout AND the step-count
question via the real constraint: you don't make lanes longer than 16 -- you add more 16-lanes (rows).

Supersedes: the 2x24 option and the "up to 64 steps" framing (both assumed lanes could exceed 16; they
can't without rebuilding the counter). 3x16 stacked rows is the engine-native answer.

Cross-ref: SequencerEngine.cpp:30 (DNA_LCM=LCM(1..16)*2), :80/:84 (cachedLength=16, full-16 window),
:20-26 (lane lengths 1..16, pingpong periods), the stacked-rows layout (now engine-justified, not just
mono-justified), the PPQN dial (triplets via resolution not length).

## THE hard part: gate mode is EVENT-driven, a grid editor is QUANTIZED -- how to reconcile (Rodney)

Rodney: in gate mode we take a step when the external gate goes HIGH, hold it high for an ARBITRARY
number of pulses until it goes low, then maybe REST an arbitrary number of pulses before the next high.
"Not sure how to map that to a visual gate editor."

### The mismatch (named precisely)
Gate mode is EVENT-DRIVEN, not grid-driven: a step = gate goes high; hold duration + rest duration are
arbitrary continuous pulse counts dictated by the INCOMING gate's rhythm. Gate mode has NO inherent grid.
A visual gate editor IS a grid (discrete equal cells = a quantization). So you're trying to draw an
unquantized, arbitrary-length signal on a quantized grid. The mismatch is fundamental -- not something
missing.

### The key insight: if you DRAW the gate, you BECOME the source -> you choose the quantization
The editor doesn't represent an arbitrary EXTERNAL gate -- it PRODUCES a gate. A produced gate's
vocabulary is whatever you decide. So the question isn't "visualize arbitrary gates" -- it's "what gate
vocabulary do I let the user draw, at what grid resolution." Once you're the source, you CHOOSE the
quantization (the PPQN dial) and the arbitrariness goes away: hold/rest durations become INTEGER numbers
of grid cells, not arbitrary pulses. You lose "arbitrary 37 pulses" but gain "drawable" -- and lose almost
nothing musically (nobody hand-draws 37-pulse gates; they draw quarter/rest/two-eighths).

### Visual vocabularies (all solved UIs, borrowable)
1. BAR model (best fit to Rodney's description): each gate is a horizontal BAR drawn across cells. Bar
   LEFT EDGE = gate goes high; bar LENGTH = cells held high; GAP to next bar = rest. Maps the description
   exactly (bar start = high, length = hold, gap = rest). How piano-rolls/Bitwig show note length. A gate
   is not "step on/off" -- it's "a bar with a start and a length."
2. TRIGGER/TIE/REST per cell (the step-sequencer idiom for the same thing): each cell = trigger (go high),
   tie/hold (stay high, extend previous), or rest (low). "High for 3 cells then rest" = trigger,tie,tie,
   rest. Grid-native; decomposes arbitrary hold into trigger + N holds. Feeds the engine's existing
   legato/tie machinery.
3. Gate-length-per-step: each step on/off + a length (covers X cells). More granular, more fiddly.
(1) and (2) are the SAME THING at different granularities -- (1) draws the bar directly, (2) expresses it
as trigger+ties. Both reconcile grid with gate mode: both produce "high for integer cells, rest for
integer cells" = the arbitrary-pulse behaviour QUANTIZED to the chosen grid.

### Steer
Visual = BARS (matches "high, hold a duration, rest" directly); data model = TRIGGER/TIE/REST per cell
(feeds the existing legato/tie machinery). Two views of one thing. Grid resolution = the PPQN dial.

### Honest subtlety: the editor REPLACES the external gate, it doesn't visualize one
In EXTERNAL gate mode the rhythm comes from outside. If the editor PRODUCES the gate, it's not "external
gate mode" -- it's "editor-as-internal-gate-source." The gate analog of the piano roll being an internal
PITCH source. Be clear: you can't visualize an arbitrary external gate on a grid; you AUTHOR a
grid-quantized gate to use INSTEAD of one. May be a distinct sub-mode (internal gate gen switched in
where the external gate would go).

Cross-ref: the trigger/tie/rest per-step vocabulary (earlier note -- now the DATA MODEL under the bar
view), the 16-lane constraint (bars live within a 16-cell lane), the PPQN dial (grid resolution),
gate-editor scoping (this is the gate-mode authoring surface), the piano-roll internal-pitch-source
parallel (this is its gate twin as an internal source).

## Which PPQN without major code change (Rodney) -- answer from code: 24 / 48 / 96

Two different "PPQN" things in the code:
- allowedPPQN bitmask (NoteValues.hpp:24,30): note-value legality is tracked at only THREE tiers --
  bit1=PPQN 1, bit2=PPQN 4, bit4=PPQN 24. At PPQN 24 every note value (incl 1/32 + all triplets) resolves
  to an integer. So the note-length legality system knows 1/4/24; 24 is what everything is designed +
  tested around.
- ppqnSetting (SequencerEngine.hpp:496 "master PPQN pulse grid (24/48/96)"): settable via right-click
  menu (MonsoonWidget.cpp:1341), persists (MonsoonPersistenceManager.cpp:247). 24/48/96 are the intended
  selectable set.

### Answer: 24, 48, 96 are handled with NO major change
They're the designed set -- ppqnSetting accepts them, menu offers them, they persist, and each is a clean
multiple of 24, so every integer-pulse guarantee the note table gives at 24 still holds (48 = 24x2, 96 =
24x4 are just finer). "Free" because multiples of 24.

### This settles the 64-steps question: use PPQN 48, no new PPQN needed
24/48/96 -> per-bar pulses 96/192/384. 64 divides: 96/64 ✗, 192/64 = 3 ✓, 384/64 = 6 ✓. So 64 steps/bar
works at PPQN 48 (192/bar, 3 pulses/step) or 96 (384/bar) -- ALREADY supported, no code change. You don't
ADD a PPQN for 64 steps; you use 48. (Rodney's "64 PPQN divisible by 64" pointed the right way, but 48 is
already enough and already supported.)

### What WOULD be a major change
Any PPQN NOT a multiple of 24: the allowedPPQN bitmask only certifies note-value integer-resolution at
1/4/24, so a PPQN introducing a factor the note table doesn't account for (e.g. 32, or a 64 base) means
revisiting NoteValues.hpp legality masks + re-verifying every note value resolves. Stay in the 24-family
(24/48/96) = tested ground.

### For the gate editor
Grid resolution = pick from 24/48/96 (all divisible by 3 -> triplets fine at any of them). Step counts
that need finer than 96/bar -> go to 192 (PPQN 48) or 384 (PPQN 96). 16-step lanes at any of these are
trivially clean (96/16=6, 192/16=12, 384/16=24). No new PPQN needed for the editor.

Cross-ref: NoteValues.hpp:24,30 (allowedPPQN 1/4/24), SequencerEngine.hpp:496 (ppqnSetting 24/48/96),
MonsoonWidget.cpp:1341 (menu), the 64-steps thread (settled: use 48), the 16-lane constraint, the PPQN
dial (this IS the dial: 24/48/96).
