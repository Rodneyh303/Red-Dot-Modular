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
