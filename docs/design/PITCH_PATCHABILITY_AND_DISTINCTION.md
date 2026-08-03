# dot.modular -- patchability & what's genuinely novel (pitch-ready material)

For the manual's "why this system" front matter and any reviewer pitch (Omri Cohen / CDM Peter Kirn).
Written as claims dot.modular can stand behind, with the honest reasoning.

## 1. Navigable probability space -- the headline novelty
CLAIM: no other generative sequencer (Rack) or MIDI-generation VST lets you freely ROAM probability
space back and forth, REPRODUCIBLY.
Why it's true: the dice/scatter RNG is an ADDRESSABLE Philox counter -- the random stream is a
COORDINATE space, not a one-way flow. Forward AND backward; land on a position and get the byte-
identical draw every time (dice scrub knob, reverse buttons, counter model). Contrast the field:
- Turing Machine / shift-register seqs: LOCK or loop a pattern; randomness knob PERTURBS. No reversible
  addressable position.
- Marbles: deja-vu = probability of REPEAT (a tendency), not a coordinate you traverse.
- MIDI-gen VSTs (Scaler/Riffer/etc): generate FORWARD; regenerate=new, undo=old. No continuous
  reproducible position IN the generative space.
This is architectural, not marketing: treating the RNG as addressable-by-POSITION (vs a stream you can
only advance) is what makes "roam back and forth reproducibly" possible. We know of no equivalent.

## 2. Modulate the pattern GENERATION itself, not just surface controls
Practically everything is modulatable -- the de-param arc existed because there were 1000+ mods to
begin with; modulation was the design SUBSTRATE, not a bolt-on. Sands exposes PROBABILITY, all of
LOR (length / offset / rotation), SPREAD, and DIRECTION as CV targets. Most gen seqs expose maybe
pattern/density/scale and stop. dot.modular exposes the SHAPE-GENERATION parameters, so you modulate
how the pattern is CONSTRUCTED, not just what plays. Qualitatively deeper input surface.

## 3. Phase-drivable, polymetric timing with external sync
Not just gate-clocked stepping: PHASE drive makes playback position a continuous, modulatable,
externally-syncable quantity. Drivable by Rack phase modules, LFOs, and EXTERNAL phase sources (e.g.
the Bitwig phase-in plugin). Plus polymetric via Sands and Intertropical. This timing model is more
flexible than gate-clocking and speaks to the growing phase-sequencing niche. LEAD WITH THIS in a
demo -- it's distinctive and looks compelling in motion.

## 4. Self-feeding + ecosystem-citizen patching (PROVEN)
Gates from one Monsoon can drive another's gate/phase drive input -> self-modulating generative loops
(the emergent-evolving-patch pattern reviewers love). And it takes gates from ANY source: tested with
Venom Rhythm Explorer driving it. So it's not a walled garden -- it integrates into anyone's patch,
cross-brand. That Venom test says a big thing: genuine modular citizen, not a closed system.

## 5. Change Alley correlation textures (poly) -- the compositional payoff
The CA correlation matrix turns poly voices from decorrelated into FAMILIES of related voices, every
intermediate reachable:
- HETEROPHONY: same melody source + different variation/range per voice = one contour ornamented
  differently per voice -- the texture of GAMELAN / Cantonese opera ensembles, from a single pin.
  (Also: same random REST data shared across poly channels but different REST settings per channel ->
  related-but-distinct rhythmic articulation of one underlying stochastic skeleton.)
- HOMOPHONY: a full column of pins on one source = voices move together.
- CALL-AND-RESPONSE: cross-block source offset (block i sources from block i+k) -> section A follows
  section B between GROUPS of voices.
- NESTED SUB-SEQUENCES: re-dicing a nested pair yields a new rhythm with the SAME hierarchy --
  structured variation, not just noise.
Note the cultural rhyme: gamelan / Cantonese opera are the regional ensemble traditions -- the
capability and the Singapore identity reinforce each other (not engineered; the philosophy is
consistent).

## 6. Flexible output + arrangement routing
Per-voice CV/gate/accent/step-gate/step-legato broken out to jacks (Changi T1/T2) -> every voice to
its own chain, with ARTICULATION downstream (the phrasing layer most gen patches lack). Intertropical
adds poly-budget scene ARRANGEMENT (voice->slot->output, fan-out chords, per-output transpose), and
that arranged output is itself a source (Lantern visualises it; Changi T3 jacks it out). Generate ->
shape -> correlate -> arrange -> visualise -> break out. One coherent signal journey.

## The one-line version
A polyphonic generative sequencing SYSTEM where you can navigate probability space in both directions,
modulate the pattern-generation itself, drive it by phase, correlate poly voices into gamelan-like
families, and route the result freely -- built as one host + expander ecosystem, with a Singapore
identity woven through the naming.
