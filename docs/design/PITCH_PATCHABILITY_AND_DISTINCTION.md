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
the Bitwig phase-in plugin). This timing model speaks to the growing phase-sequencing niche and is
distinctive and compelling in motion -- LEAD WITH THIS in a demo.

### Polymeter -- natively and deeply supported
Definition (Hetrick): two sequences with DIFFERENT LENGTHS running at the SAME clock speed; they share
a clock but diverge in effective length, realigning after LCM steps. This is a FIRST-CLASS feature:
- WITHIN ONE MONSOON: Sands' per-lane LOR (length/offset/rotation) gives each lane its own effective
  step-count. A 5-step melody lane against a 7-step rhythm lane against a 9-step accent lane -- all
  driven by the same 1/16 clock, realigning at different rates. One host, true multi-voice polymeter
  without patching multiple sequencers.
- AT THE ARRANGEMENT LEVEL: Intertropical's per-scene membership + scene sequencing means different
  scenes can represent different polymetric states -- the arrangement layer itself is polymetric.
- ACROSS MULTIPLE MONSOONS: gate or phase output of one drives another -> polymetric interaction
  between independent engines, with Change Alley correlation linking their content.
Pitch this explicitly and confidently. "Polymetric" is often what people mean when they say complex
generative rhythm, and most sequencers fake it; yours does it natively.

### Polyrhythm -- honest bound (cross-instance, available NOW)
Definition: different TIME BASES (a 3:2 ratio, e.g. triplets against straight eighths -- the two parts
are clocked differently, not just differently-lengthed). This is distinct from polymeter.
- CROSS-INSTANCE, RIGHT NOW: two Monsoons fed different gate inputs (e.g. a standard 1/16 gate to
  Monsoon A and a 1/16-triplet gate to Monsoon B from the same master clock) = genuine polyrhythm.
  The two engines run in a 3:2 time-base relationship -- exactly Hetrick's definition -- and each
  applies its full generative articulation (rest, legato, accent, LOR, Sands shaping) within its own
  metric grid. No special modules needed: any Rack clock with multiple subdivision outputs works.
  This is available TODAY, no new features required.
- NOTE: Change Alley does NOT link two separate Monsoon instances -- its correlation matrix operates
  on voices WITHIN one engine only. Each Monsoon has its own CA expander, no cross-instance
  correlation. The polyrhythmic relationship between two Monsoons is timing-only; the musical
  relationship between them comes from the patch (shared pitch material, Changi outputs feeding
  the same destination, etc.), not from CA correlation.
- Phase inputs also work (drive Monsoon A and B from phase signals at different speeds, e.g. via
  Hetrick's Phasor Div/Mult), which gives additional control over phase alignment and reset.
- WITHIN ONE MONSOON: currently the 1/16 grid is the constraint -- everything snaps to 1/16, so true
  in-engine triplets-against-straight is not yet supported. The maybe-later triplet step model (per-lane
  triplet subdivision flag) would bring native in-engine polyrhythm between lanes. Deferred.
So: claim POLYMETER and POLYRHYTHM both -- polymeter within one Monsoon, polyrhythm across two with
different gate/phase inputs. Don't conflate the terms, but both are real and available today.

### External CLOCK vs external GATE
External CLOCK is table stakes. External GATE here is NOT: an incoming external gate still passes
through the engine's REST / LEGATO / ACCENT (mode B) articulation -- so an arbitrary/irregular/random
external gate stream (tested: Venom Rhythm Explorer) isn't just a metronome, it becomes raw rhythmic
material that Monsoon's own generative articulation layer rest-filters, legato-shapes and accents.
External structure and internal generation COMPOSE. [CODE-CHECK before publishing: confirm external-
gate-driven steps route through the SAME rest/legato/accent-mode-B path as internal steps, not a
reduced path.]

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

### Musical techniques the three-space mapping enables
Intertropical is really three independent mappings -- PER-SCENE membership (which voice sits in which
slot), GLOBAL slot->output (fixed structure), and per-output TRANSPOSE. That separation is what makes
these classic arrangement techniques fall out (they're consequences of the model, not bolted-on
features), all visible at a glance in the all-scenes voice->slot grid:

- VOICE SUBSTITUTION (the headline case): an output is a STABLE part (slot->output is global, so the
  output is always fed), but WHICH voice generates it changes per scene (membership reseats the slot).
  The harmonic/timbral skeleton holds while the source rotates. Read across a slot's row in the grid
  and you SEE the substitution. (Worked example: outputs 3 & 6 are fixed chord tones; voice 16 feeds
  them only in scenes 4 & 6, other voices elsewhere -- pure membership, no re-routing.)
- ORCHESTRATION / REGISTRATION CHANGE: because each output goes to its own synth voice, reseating a
  different global voice into the same output across scenes = reassigning a PART TO A DIFFERENT TIMBRE
  (flute -> oboe between sections; an organist changing stops). Arrangement as re-scoring.
- HOCKET: seat a slot in ALTERNATING scenes (voice A odd scenes, voice B even) -> one output LINE is
  passed back and forth between voices. The medieval/gamelan hocket technique, by scene pattern.
- ANTIPHONY / CALL-AND-RESPONSE: seat voices into output-GROUP A's slots in some scenes and group B's
  in others -> registral/spatial call-and-response between output banks (pairs with sending groups to
  different synth chains / pan positions).
- TEXTURE DYNAMICS (thin/build): a scene that seats FEWER voices leaves outputs unfed -> breakdowns,
  builds, drop-outs as an arrangement dimension. Density becomes a per-scene compositional control.
- FAN-OUT HARMONY: one voice -> multiple outputs (a slot's row lit at 2+), each with its own transpose
  -> chord/octave/unison DOUBLING from a single generated line (+0 unison, +12 octave, +7/+3/+4 chord
  tones). Turns the arranger into a harmoniser. (Trade-off: fan-out COUPLES the parts -- same line in
  lockstep -- intended for doubling, muddy if independence was wanted.)
- GENERATIVE vs COMPOSED arrangement: membership auto-packs voices to slots in order by default (let
  it arrange itself) OR you seat explicitly (compose the arrangement). Same grid, both workflows.

Together: Intertropical is a SCENE-BASED ARRANGER where the classic techniques of orchestration --
substitution, hocket, antiphony, doubling, textural dynamics -- are expressible as seat/route/transpose
choices over a polyphonic generative source. Few generative systems offer an arrangement layer at all;
fewer make it this legible.

## 7. The correlation matrix as a textural continuum -- the East/West axis
The single most important control (the CA correlation matrix) is a CONTINUOUS dial between textures,
not a set of modes:
- Full column of pins on one source = HOMOPHONY (voices move together; + Intertropical fan-out chords
  = melody-plus-accompaniment homophony).
- Shared source, different variation/range/rest settings per voice = HETEROPHONY (one contour,
  many simultaneous ornamentations).
- Identity / decorrelated, scale-constrained by Shophouse = INDEPENDENT POLYPHONY / contrapuntal
  TEXTURE (independent lines kept consonant by a shared scale -- modal-polyphonic, the way a lot of
  Renaissance/folk polyphony actually works).
HONEST BOUND: this produces the TEXTURE of counterpoint (independent, rhythmically distinct lines,
scale-constrained to consonance) -- NOT species counterpoint with enforced voice-leading. Claim the
texture continuum, not "it writes fugues"; a theory-literate reviewer will respect the precision.

The strong framing (better than a feature checklist): homophony, heterophony and independent polyphony
are ONE continuous control at different settings, traversable in REAL TIME. And the traditions sit at
different points on that axis -- heterophony is the gamelan / Southeast-Asian ensemble base; homophony
and independent polyphony lean Western common-practice. So the correlation dial SWEEPS BETWEEN AND
BEYOND the textural foundations of Eastern and Western ensemble music, through hybrid points no single
tradition parked at (so they have no names). Not "imitates two traditions" -- reveals they are
endpoints of one continuous space and lets you compose in between. (The East/West axis and the
Singapore identity are the same coherence again: the region's music IS the middle of the dial.)

CONCRETE BACKING -- the SCALE SET (openable in Rack, verifiable): 28 scales spanning Western
common-practice (Major, the 3 minors, all church modes, Whole Tone, Diminished, Blues, pentatonics)
AND the East/Southeast Asian traditions -- notably PELOG (the Indonesian GAMELAN scale, which pairs
directly with the heterophony/gamelan point above), the Japanese set (Hirojoshi, In-Sen, Iwato,
Kumoi), Bhairav (N. Indian raga), and the Eastern-European/Iberian seam (Hungarian Minor, Spanish).
So the pitch COLLECTIONS themselves span the same East/West axis the correlation matrix does -- two
independent expressions of one idea. For Singapore this reads as the city's musical demographics as a
scale list. And the scale system is PROBABILISTIC, not a hard quantiser: per-semitone probability
faders + a scale MASK that gates the READ (out-of-scale notes read zero prob when locked, faders keep
the user's values and merely DIM) -- so you can weight scale degrees, CV-modulate the weights, and
stay in-scale, which hard-quantiser scale systems can't do. HONEST NOTE: Pelog/Slendro etc. are 12-TET
APPROXIMATIONS of non-12-TET tunings (standard for a semitone-mask system). Label accurately.

## 8. Reversibility: Philox counter + phase drive = a scrubbable generative system
Two axes are independently reversible, and TOGETHER they make the generative output scrub like tape:
- PHASE drive makes TIME reversible/addressable (playback position is a continuous driven quantity --
  scrub it backward and forward).
- The addressable PHILOX counter makes RANDOMNESS reversible/addressable (position -> byte-identical
  draw, both directions).
Why the complement matters: normally these FIGHT. You can rewind a clock, but an ordinary RNG is a
one-way stream -- the random values are gone, so scrubbing backward yields DIFFERENT notes on the way
back than you heard going in; the music doesn't survive a rewind. dot.modular is the rare case where
BOTH are pinned to POSITION and phase IS position -- so rewinding phase and rewinding the counter stay
in lockstep, and the exact same stochastic material plays in reverse, note-for-note. A generative
sequence you can scrub, reverse, and replay deterministically. THIS is why "roam probability space
back and forth" (point 1) and phase drive (point 3) are not two features but one system: reversible
randomness + reversible time = a navigable generative timeline.
HONEST BOUND: this holds for the SAMPLING (RNG draw at a position) and phase. STATEFUL board transforms
(scatter's domain permutation) COMPOSE on the running board and are not automatically reversed by a
counter rewind -- true board-undo needs the inverse permutation (see feat/domain-reverse-inverse /
the CA reverse discussion). So: the sampling+time axes are fully reversible; the transform-composition
layer reverses with inverse-operation care. Claim the former cleanly; don't claim "the entire system
is perfectly reversible."

## The one-line version
A polyphonic generative sequencing SYSTEM where you can scrub probability space in both directions
(addressable Philox counter + phase drive = a reversible, replayable generative timeline), modulate
the pattern-generation itself, sweep poly voices along one continuous dial from homophony through
heterophony to independent polyphony (the textural axis of Eastern and Western ensemble music), and
route the result freely -- built as one host + expander ecosystem, with a Singapore identity woven
through the naming.
