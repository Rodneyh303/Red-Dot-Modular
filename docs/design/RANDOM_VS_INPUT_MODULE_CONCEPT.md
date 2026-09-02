# Module concept: per-voice random-vs-input probability (composed <-> generated freedom gradient)

Simple new module (Rodney). 16 knobs, modulatable but NOT params (the deparam pattern -- modulatable
values with edit-vs-modulated split, like the Sands per-voice/per-lane values, not raw VCV params). Form
factor a bit like Straits (compact multi-knob row).

## Function (quantiser modes)
Each knob is PER VOICE/CHANNEL (not per step). Knob[v] = the probability that voice v's note comes from
the RANDOM ENGINE instead of the NOTE INPUT. Per step, for each voice, roll against knob[v]: hit -> take
the random-engine note; miss -> take the input note. Everything downstream (mask, tuning, gate, Sands
rules) then applies to whichever note won.

## Why per-VOICE is the strong version (Rodney corrected from per-step)
Per-step = "at step N everyone is more/less random" = a temporal density of randomness, same for all
voices. Per-voice = each voice sits at its OWN point on the composed<->generated axis:
- Voice 0 at 0% -> plays the input pathway faithfully (the core line / cantus firmus / the given melody).
- Voices 1..n at increasing % -> progressively free, generating their own notes against voice 0.
= HETEROPHONY with a controllable FREEDOM GRADIENT across voices. One core melody held by one part while
others elaborate/diverge by dialable amounts -- exactly the texture of maqam heterophonic ensembles and
gamelan (core line + elaborating parts). A direct control for tradition-faithful heterophonic elaboration:
voice 0 holds the line, upper voices ornament/improvise with dialable independence, all within the mask.

## The conceptual centrality
This makes the instrument's CENTRAL tension -- authored INPUT pathway vs GENERATIVE engine -- a per-voice,
modulatable, continuous blend:
- knob 0 = pure INPUT (the given pole).
- knob 1 = pure RANDOM ENGINE (the generative/Change-Alley pole).
- between = per-voice probabilistic blend of authored and generated melody.
So the composed<->generated axis becomes a per-voice FREEDOM PROFILE across the polyphony -- some voices
given, some free, dialable per voice. A row of knobs you read as the freedom-gradient of the ensemble.
Simple to build, but conceptually central (not peripheral): it distributes the composed-vs-generated
tension across the voices.

## Design decisions
1. New module vs voice-lane: functionally it's a per-VOICE value over up-to-16 voices = the shape of the
   existing voice-lane machinery (Sands is per-voice). A dedicated Straits-like module makes the gesture
   TANGIBLE (see the freedom-gradient as a knob row); a voice-lane would be cheaper/consistent. Rodney's
   framing = dedicated module. Either way it reuses the deparam + voice-indexed patterns.
2. RANDOM-WITHIN-MASK (musical key): a voice going "random" draws WITHIN the current mask/tuning, so
   divergent voices stay in the scale/maqam = heterophonic elaboration, NOT chromatic noise. Almost
   certainly yes -- it's what makes it read as improvisation within the mode.
3. Distinct from fire-probability: existing per-step probability = does the note fire; THIS = input-vs-
   random GIVEN it fires (a SOURCE probability). Two distinct probabilities, they compose (fire first,
   then source). Name them clearly (fire-probability vs source-probability) to avoid confusion.
4. 16 vs 8 knobs: if poly is <=8, decide 16 (future-proof/general) or 8 (match current voice count).
   Minor.

## Scope + status
Quantiser modes (where there's a note INPUT to blend against the random engine). PARKED as a concept, not
v1-critical; low build complexity (per-voice selector between two EXISTING sources: input note + random-
engine draw, mask-constrained). Depends only on quantiser modes being final (the note-input path).

## Musical payoff (East-pole faithful)
Hand-drawn/external melody on the input; voice 0 plays it straight (the tune); dial voices 1..n from
"double the tune" (0%) through "loosely follow" to "freely improvise within the mask" (high) -> maqam-style
heterophonic elaboration: one core line, multiple voices diverging by controllable amounts, all in the
tuning/mask. The heterophony Western polyphony ISN'T -- many voices doing versions of the same line at
different freedoms, not independent harmonic parts. A tradition-faithful texture from one per-voice knob.

Cross-ref: SHAREABILITY_ANALYSIS / QUANTISER_MODES_UNIFICATION (quantiser takes external input = the pole
this blends against random), PROBABILITY_MODIFIER_MODEL (fire-probability, distinct from this source-
probability; the composed-vs-generated axis), PIANO_ROLL_MODULE_CONCEPT / Esplanade (a native input source
to blend against random), LAUNCH_INTENT_AND_STORY (heterophony / the composed<->generated tension this
makes per-voice), Sands (per-voice lane machinery this reuses).

## Both sources go via CHANGE ALLEY -> the melody knobs are CA source-routing (Rodney)

Key clarification: BOTH the random-engine melody AND the CV-input melody flow through Change Alley as
melody sources. Confirmed CHANGE_ALLEY_DESIGN: CA operates on MELODY-stream draws (:18); defining decision
is SOURCE-TABLE remap (:87-91); "same melody source + different variation/range = heterophony" (:42). So:
- The input CV melody is NOT special-cased -- it's just another CA melody source, correlate/share/rotate/
  remap-able like the random draws.
- The per-voice random-vs-input knobs are PER-VOICE SOURCE-SELECTION WITHIN CA'S MELODY STREAM, not source-
  vs-bypass. Both sources are CA-native.
- Heterophony STRENGTHENS: voices diverging from the input don't leave the correlation structure -- they're
  CORRELATED VARIATIONS OF THE INPUT within CA. Same machinery, freedom gradient INSIDE Change Alley.
Module = a CA melody-source router (random <-> input, per voice), consistent with CA's shared-source +
per-consumer-variation model. Cleaner than "engine vs external".

## NEW MODULE NEED: reverse-Keppel -- external (MPE) MIDI IN -> microtonal CV (Rodney)
To feed an external melody into quantiser mode and get variations quantised to the scale: MIDI IN ->
microtonal CV. Plain MIDI note-in is 12-TET only; the MICROTONAL case needs the INVERSE of Keppel:
- Keppel: poly microtonal pitch CV -> MPE MIDI OUT (split each voice into nearest-12-TET note + per-note
  bend, one MPE member channel/voice).
- REVERSE-KEPPEL: poly MPE MIDI IN -> poly microtonal pitch CV (+ gate). Reconstruct per-voice true pitch
  from note + bend = Keppel's split run BACKWARD. (note,bend) -> (CV). Same math, opposite direction.
Plain non-MPE MIDI in = simpler (note->CV, 12-TET); the microtonal VALUE is handling MPE-in so an
expressive/microtonal controller's pitch survives into the engine.

### Composes with everything (the full loop)
MPE keyboard -> reverse-Keppel -> microtonal CV -> enters as a CHANGE ALLEY melody source -> per-voice
melody knobs route it against the random engine -> correlated microtonal variations of the externally-
played melody, quantised to the scale/maqam -> (optionally) Keppel back OUT to MPE. Reverse-Keppel + Keppel
BOOKEND the microtonal MIDI I/O (in / out).

### Naming (candidate)
Keppel = sea-export container terminal (CV->MIDI OUT). Inbound counterpart = a Singapore entry gateway.
WOODLANDS = the Causeway land-crossing INTO Singapore (external melody enters here). Keppel(sea/export) +
Woodlands(land/import) = a clean port-vs-crossing pair. (Alt: Tuas, newest port.) Low priority.

### Status
Reverse-Keppel = self-contained utility, ZERO engine coupling (like Keppel), buildable independently any
time; POST-V1. Enables external-melody-in for quantiser mode + completes the microtonal MIDI I/O pair.

Cross-ref: MPE_UTILITY_BUILD_SPEC / MICROTONAL_MIDI_MPE_DIRECTION (Keppel = forward direction), CHANGE_
ALLEY_DESIGN (input CV melody = a CA melody source), the per-voice module above, QUANTISER_MODES_
UNIFICATION (quantiser takes the external melody this feeds).

## CORRECTION (Rodney): MPE-in is probably an EXISTING Rack module -- reverse-Keppel likely NOT needed
Rodney: "might be able to use an existing rack module for mpe in." Correct -- and it likely obviates
reverse-Keppel.

VCV's core MIDI modules already do MPE. VCV MIDI-CV in MPE mode outputs per-voice pitch CV, gate, velocity,
aftertouch, and the per-channel pitch BEND -- AND (standard MPE behaviour) it folds bend INTO the note,
outputting combined note+bend as a SINGLE 1V/oct pitch CV per voice (the bend IS the pitch expression, so
the module sums them). If so, that combined CV IS the microtonal pitch, straight into quantiser mode. No
bend-reconstruction to build -- the stock module already does it.

### So reverse-Keppel is probably UNNECESSARY
The thing that made reverse-Keppel "not just plain MIDI-in" (reconstructing pitch from note+bend) is
handled INSIDE VCV MIDI-CV's MPE mode. So the MPE-in path is likely just: MPE controller -> VCV MIDI-CV
(MPE) -> poly pitch CV -> quantiser mode. No new module.

### The real asymmetry (why OUT needs Keppel but IN doesn't)
- OUT: Rack has NO microtonal-CV -> MPE-out that SPLITS per-voice bend for a DAW -> Keppel is a genuine
  needed build (the split isn't stock).
- IN: Rack DOES have MPE -> CV (VCV MIDI-CV sums note+bend) -> no build needed (the sum is stock).
So "Keppel has an inverse, build the inverse" was over-symmetrising. The directions aren't symmetric in
what Rack already provides.

### Narrow cases that could still want something (probably not)
1. If VCV MIDI-CV outputs note + bend SEPARATELY (not summed): a trivial note+bend adder -- but that's a
   stock offset/mixer, not a module worth building/naming.
2. Microtonal INPUT quantisation (interpret incoming pitch against the tuning on the way in): that's
   arguably QUANTISER MODE'S job anyway -- feed the combined CV in, quantiser mode snaps it to the scale.
   Collapses into "use stock MPE-in + let quantiser mode do the microtonal part".

### TO VERIFY (quick Rack test, can't confirm from here)
Does VCV MIDI-CV MPE mode output COMBINED note+bend pitch CV per voice? Almost certainly yes (standard MPE).
If yes: external-microtonal-melody-in = stock VCV MIDI-CV -> quantiser mode, and reverse-Keppel/Woodlands
is dropped. Keppel-OUT stays a real build.

Supersedes the reverse-Keppel section above: MPE-in is (almost certainly) a stock-module job; keep Keppel
(out) as the one microtonal-MIDI module that must be built. Woodlands naming parked unless a real in-module
need survives the Rack test.

Cross-ref: MPE_UTILITY_BUILD_SPEC (Keppel out -- still needed), the reverse-Keppel section above (now
superseded -- MPE-in via stock VCV MIDI-CV), QUANTISER_MODES_UNIFICATION (quantiser does the microtonal
interpretation of the incoming CV).

## CORRECTION 2 (Rodney + web check): CORE does NOT do MPE-in cleanly; THIRD-PARTY modules do
Earlier I wrongly said VCV CORE MIDI-CV sums note+bend into a clean microtonal pitch. WRONG. Core has an
MPE polyphony mode but does NOT cleanly fold bend into pitch -- community reports you must manually SUM
pitchwheel + note, landing on an awkward bend range (~60 semitones) to make it work; core V/OCT is just
the note. So core is NOT the MPE-in solution.

THIRD-PARTY modules DO it properly (a couple exist -- Rodney to pick/verify):
- alexandreleroux/MPE: 1V/oct output = note combined with 14-bit pitchwheel, adjustable bend range in
  semitones to match the controller (slides up to 96 semitones). This combined note+bend-as-one-CV IS the
  microtonal pitch reconstruction = exactly the "reverse-Keppel" job, already built.
- MIDIpolyMPE / MIDIPolyExpression: purpose-built MPE-in. (Caveat: a reported bug -- notes terminating
  mid-bend slew the bend from an arbitrary value on next channel rotation; test note-release-mid-bend.)
- Kilpatrick Toolbox MIDI-CV: poly mode + pitch bend in note modes, range 1-12 semitones settable.

### Corrected conclusion: reverse-Keppel STILL unnecessary, better reason
Not "core sums it" (wrong) but "a THIRD-PARTY module does the note+bend -> combined-microtonal-CV
reconstruction, already". So: pick a third-party MPE-in module -> its combined-pitch CV -> quantiser mode.
Don't build reverse-Keppel. Keppel-OUT still a real build (Rack has no microtonal-CV->MPE-out splitter).

### What to verify when picking the third-party module
1. Combined-pitch output preserves microtonal resolution (14-bit bend, bend range adjustable to match the
   controller -- alexandreleroux/MPE explicitly does this).
2. Note-release-mid-bend behaviour (the MIDIpolyMPE slew bug) -- pick one whose reconstruction is clean.

Supersedes CORRECTION above where it credited CORE. Net unchanged (no reverse-Keppel build) but the reason
is third-party-does-it, not core-does-it.

Cross-ref: MPE_UTILITY_BUILD_SPEC (Keppel out, still needed), external module choices (alexandreleroux/MPE,
MIDIpolyMPE, Kilpatrick), QUANTISER_MODES_UNIFICATION (quantiser consumes the combined-pitch CV).

## CORRECTION 2 (Rodney): VCV CORE does NOT do MPE-in cleanly -- THIRD-PARTY modules do (a couple, TBC)
Correcting my prior claim: I wrongly asserted VCV CORE MIDI-CV covers MPE-in. Per Rodney, core does not;
there are a couple of THIRD-PARTY modules that can do MPE -- to be checked. My core-does-it claim was
wrong; Rodney knows the Rack landscape.

The conclusion is mostly unchanged and actually cleaner. Whether MPE-in comes from a third-party module or
a build, the deciding questions are the same:
1. Does the MPE-in module output COMBINED note+bend as one pitch CV per voice (-> straight into quantiser
   mode, no reverse-Keppel), or note+bend SEPARATELY (-> a trivial adder needed)?
2. Is a suitable third-party MPE-in module available (-> no dot.modular build) or does a gap remain (->
   reverse-Keppel / Woodlands justified after all)?

Core-vs-third-party does NOT by itself revive reverse-Keppel: a THIRD-PARTY module that does MPE-in still
means "use the existing one, don't build". A build is justified ONLY if NONE of the third-party MPE modules
give clean per-voice microtonal pitch CV in the form quantiser mode wants. Rodney's check settles it.

### TO CHECK (Rodney, in Rack)
- Which third-party modules do MPE-in (a couple exist).
- Do they output combined note+bend per-voice pitch CV (the form quantiser mode wants)?
- If yes -> external-microtonal-melody-in = that third-party module -> quantiser mode; reverse-Keppel/
  Woodlands DROPPED. If no clean option -> reverse-Keppel/Woodlands becomes a real (post-V1) build.

Supersedes CORRECTION 1's "VCV core MIDI-CV" specifics (core doesn't do it); keeps the shape (MPE-in is
likely an existing THIRD-PARTY module, not a dot.modular build) pending Rodney's check. Keppel-OUT stays a
real build regardless (no stock microtonal-CV->MPE-out exists).

Cross-ref: CORRECTION 1 above (superseded on the core-module point), MPE_UTILITY_BUILD_SPEC (Keppel out,
still needed), QUANTISER_MODES_UNIFICATION (quantiser does microtonal interpretation of the incoming CV).

## V1 + the ROUND-TRIP TEST as acceptance gate for MPE-in (Rodney)
Two corrections to my filing:
1. This is V1, NOT post-V1. The external-melody / MPE round-trip (melody router + MPE-in + Keppel) is in
   scope for V1. Retag from post-V1.
2. Verification method = a ROUND-TRIP TEST (empirical acceptance gate, not "does a module exist"):
   Write a pattern OUT to the DAW via Keppel (CV -> MPE out), read it back IN (MPE -> CV via a third-party
   module), and check the returned CV matches -- or is acceptably close to -- what went out.
   - Round-trip preserves pitch within a small discretisation error -> third-party MPE-in is good enough,
     NO reverse-Keppel.
   - Error too big -> BUILD reverse-Keppel.
   The round-trip IS the acceptance test; the decision rule is quantitative.

### What "small discretisation error" should mean (threshold)
Ear pitch-discrimination is ~5-6 cents melodically (tighter sustained/simultaneous). So the pass criterion:
round-trip error WELL UNDER ~5 cents, ideally <1-2 cents across the tuning = musically transparent. Under
that -> third-party path is transparent, reverse-Keppel unnecessary. Over (esp. systematic) -> build.

### Where round-trip error comes from (what you're measuring)
- MPE bend RESOLUTION: 14-bit bend over the range (~0.006 cents/step at +-48) = negligible; 7-bit or
  coarser would grow it. Error ~ bend bit-depth x range per hop.
- BEND-RANGE AGREEMENT (the real risk, not resolution): Keppel encodes microtonal offset as bend assuming
  a specific range (e.g. +-48). If the third-party MPE-in assumes a DIFFERENT range, reconstructed pitch is
  SCALED WRONG = systematic multiplier error, not small discretisation. The round-trip catches this LOUDLY
  (wrong pitch, not slightly-off) -- a strength of the test.
- Note+bend recombination: return module must sum correctly; round-trip catches if not.
- The DAW in the middle may re-quantise/re-time -> test THROUGH the actual DAW, not just Rack->Rack (the
  DAW is part of the real path).

### Why the test is well-designed
You don't audit whether the third-party module's internal format matches Keppel's -- you MEASURE the end-
to-end result. Different internal representations are fine if the round-trip pitch matches within
tolerance. Test the composition, don't audit the parts.

### Reverse-Keppel = CONTINGENT V1 item
Spec it, keep it ready, BUILD it iff the round-trip fails the cents tolerance (esp. an unconfigurable range
mismatch). Not "build/don't" -- "build iff the measurement says the third-party path isn't transparent". If
third-party round-trips clean -> save the build; if not -> you already know you need it and by how much.

Supersedes the post-V1 tags + the "probably not needed / needs a module-exists check": it's V1, the gate is
the round-trip test (<~5 cents, ideally <1-2), reverse-Keppel is contingent on failing it. Keppel-OUT
needed regardless (it's half the test rig too).

Cross-ref: MPE_UTILITY_BUILD_SPEC (Keppel out = the test's OUT leg), CORRECTION 1 & 2 above (superseded:
MPE-in is third-party, validated by round-trip not by inspection), ROAD_TO_RELEASE (move these to V1).

## How to source the per-step per-voice input-vs-generated decision: a FOURTH PHILOX STREAM (Rodney)

Question: per-channel probability of using the generated note in quantiser mode conceptually needs per-STEP
per-VOICE random samples to decide input-CV vs generated. How to manage? Generate an extra 16-step draw
per voice each melody-draw? And by keeping it out of Sands we treat it differently.

### Answer: NOT a stored side-array, NOT in Sands -- a fourth addressable Philox STREAM
The decision randomness MUST be reproducible/reversible like everything else -- Philox draws are a pure
function of (counter, key) (PhiloxRng.hpp:10-13), so any draw at index N re-derives without generating
0..N-1 = what makes reverse/scrub/dice work. If the input-vs-generated decision isn't equally addressable,
scrubbing backward would decide differently and the melody wouldn't reproduce. So it must be an addressable
Philox draw, not an ad-hoc RNG or a generated-and-stored array.

The code already has the exact pattern: per-stream keys by additive offset (PhiloxRng.hpp:96)
  STREAM_RHYTHM=0, STREAM_MELODY=1, STREAM_CA=2  (documented S, S+1, S+2 model; Philox decorrelates).
-> ADD STREAM_SOURCE_SELECT = 3. The input-vs-generated decision at (voice v, counter n):
  draw = philox.atUniform(key = deriveKey(seed, STREAM_SOURCE_SELECT), counter/nonce encoding (n, v));
  useGenerated = (draw < knob[v]).

### This answers all three sub-questions
1. "Extra 16-step draw per voice each melody-draw?" -> NO storage, NOT tied to generate-time. Philox is
   ADDRESSABLE: compute the decision on demand at (counter, key) whenever needed, for any counter position.
   Forward/reverse/jump/scrub -> query the same (counter, key) -> same draw. Free to re-derive, zero
   storage, automatically reversible. Not "an array generated per melody-draw" -- "a stream sampled at the
   current counter", exactly like the melody stream.
2. "How to manage?" -> Identically to the melody draw, on stream 3. Same counter, same key model, same
   addressability. Source-select and melody draws are queried at the SAME counter but DIFFERENT streams ->
   independent (Philox decorrelates) yet both reproducible. Scrub to N -> re-derive both the melody note
   AND the source decision at N. No state.
3. "Out of Sands = treated differently" -> Correct to keep it out (Sands = per-voice ROUTING/rules; this =
   per-voice stochastic SOURCE-SELECTION, a different kind of thing). But the "different treatment" is a
   first-class fourth Philox stream, NOT a special side-mechanism -> consistent with the rhythm/melody/CA
   randomness architecture while functionally distinct from Sands. Distinct role, uniform mechanism.

### Per-voice independence: use the reserved NONCE, not a stream offset
Need per-voice independence (voice 1's dice != voice 2's). Two options:
- Per-voice stream offset (STREAM_SOURCE_SELECT + v): risks colliding with other streams' key regions --
  sloppy.
- BETTER: fold voice into the NONCE. The counter is packed into ctr[0..1]; ctr[2..3] carry a fixed nonce
  (0) with "the full 128-bit counter space available if ever needed" (PhiloxRng.hpp:109-110). Put VOICE in
  the nonce (ctr[2]) and STEP/counter in ctr[0..1], on the single STREAM_SOURCE_SELECT=3 key. Per-voice AND
  per-step addressable from one stream, no offset collision, using the space the architecture ALREADY
  reserved for exactly this.

### Precise recommendation
STREAM_SOURCE_SELECT = 3; voice in nonce (ctr[2]); step in counter (ctr[0..1]); sample on demand at the
current counter to decide input-vs-generated per voice per step. Addressable, reversible, zero-storage,
per-voice independent, consistent with rhythm/melody/CA, correctly OUTSIDE Sands.

Cross-ref: PhiloxRng.hpp:10-13 (addressable/reversible), :96-103 (per-stream additive key model -> add
STREAM_SOURCE_SELECT=3), :109-110 (ctr[2..3] reserved nonce -> put voice here), PHILOX_KEY_DERIVATION_AND_
CA_SEED.md (the identical-derivation bug this pattern already fixed -- follow it), the per-voice module
concept above, PROBABILITY_MODIFIER_MODEL (fire-probability is a separate decision; this is source-select).

## NOT a shaping gap -- a LEVEL distinction (Rodney): Sands shapes; mix merely selects
Clarification resolving the "CA/mix lack Sands treatment" worry: it's NOT a coverage gap to fill -- it's a
principled difference in KIND.

- SANDS items are FUNDAMENTAL: they directly SHAPE the melody and rhythm themselves (LOR, spread,
  variation, legato determine WHAT the notes and rhythm ARE). Rich, per-voice, deserve the full machinery.
- MIX PROBABILITY is NOT that: it's a SIMPLE BLEND between two ALREADY-FORMED sources (generated stream vs
  external input). It doesn't shape a melody -- it CHOOSES between two finished melodies, per voice. A
  per-voice Bernoulli selector.

### The level distinction
- Sands = the GENERATIVE/SHAPING level (constructs the melody/rhythm). Fundamental -> rich complexity.
- Mix = the ROUTING/SELECTION level (chooses between constructed melodies). A blend -> simple by nature.
Different levels, different appropriate complexity. The mix being SIMPLER isn't a missing feature -- it's
CORRECT SCOPING for what a blend is. Giving it Sands-level LOR/spread would be over-engineering a selector
into a shaper (a crossfader doesn't need its own EQ).

### Validates the raw fourth-Philox-stream recommendation
The mix-draw SHOULD be raw (no LOR/spread) precisely BECAUSE it's a simple selection, not fundamental
shaping. The earlier "but main draws get Sands and this doesn't" worry applied shaping-level thinking to a
selection-level op = a category error. Blends are simple by nature.

### Temporal evolution comes from MODULATING the knob, not from Sands-ifying the draw
If you want the blend to evolve (commit to generated for a phrase, drift back to input), you get that from
MODULATING the mix knob (it's deparam/modulatable), NOT from baking run-length into the draw. Time-shaping
of the blend = external modulation of the knob, same as any modulatable value. Keeps the level distinction
clean: mix stays a simple per-step selector; any evolution is modulation on top. (This is the right fix for
the earlier flicker concern -- automate the knob, don't promote the selector to a shaper.)

### Change Alley
CA is not an unshaped gap either: it has its OWN control surface (pinning + spread = correlation-structure
across voices), orthogonal to Sands' value-shaping. CA correlates HOW VOICES RELATE; Sands shapes HOW EACH
VOICE'S VALUES PERSIST/VARY. Different jobs, different (already-present) controls. So CA doesn't want Sands
treatment either.

### Principle (stated)
Sands = fundamental melody/rhythm shaping (rich, per-voice). Mix probability = simple per-voice blend of
two finished sources (raw selector, simple by design). CA = correlation-structure (its own pin/spread).
Temporal evolution of the blend = modulate the knob, not Sands-ify the draw. NO gap -- a correct level
distinction. Supersedes the "shaping-coverage gap / three options" framing from the prior turn.

Cross-ref: SANDS_ARCHITECTURE_CONSOLIDATION (combineLOR/combineSpread = the fundamental shaping machinery),
CHANGE_ALLEY_DESIGN (CA's own pin/spread), the fourth-Philox-stream section above (raw is correct for a
selector), the per-voice module concept (the modulatable knob = where temporal evolution lives).

## REFINEMENT (Rodney): mix shares the SANDS-ITEM IDIOM (knob vs per-step probability); CA's outcome shows on PINS
Sharpening the prior "level distinction" -- I over-flattened the mix to "a dumb raw selector". Correction:

### The mix is structurally a Sands-item idiom, not a special raw thing
Sands note-probability = per-position weights/knobs compared against a per-step draw to decide an outcome
(PROBABILITY_MODIFIER_MODEL:25 "per-position probabilities/weights governing which notes fire"). The
quantiser mix is the SAME SHAPE: a modulatable per-voice KNOB compared against a per-step PROBABILITY draw
to decide input-vs-generated. So the mix shares the SANDS-ITEM CONTROL IDIOM (knob vs per-step probability)
-- it should be built/presented in that idiom, NOT as a categorically-simpler special case.

### Reconciling with the prior "level distinction"
Both hold, at different layers:
- WHAT IT CONTROLS (semantic level) -- still distinct: Sands SHAPES the melody; the mix SELECTS between two
  finished melodies. The level distinction stands.
- MECHANISM / IDIOM -- SHARED: both are knob-vs-per-step-probability. My earlier mistake was letting the
  level distinction imply an IMPLEMENTATION distinction ("keep it raw, un-Sands-like"). Wrong: same idiom,
  different outcome.
The raw fourth-Philox-stream still stands -- that's about WHERE THE RANDOMNESS COMES FROM (reproducible
draw), NOT about how the control PRESENTS. The refinement is at the presentation/idiom layer: the mix is a
knob-vs-per-step-probability control LIKE a Sands item.

### Change Alley's idiom is different: outcome on the PINS
CA draw is visualised in terms of its OUTCOME ON THE PINS -- the correlation-matrix pin state IS CA's
outcome display. So CA's idiom is pin-outcome-visualisation, distinct from the knob-vs-probability idiom.

### The three-way idiom map
- SANDS items      -> knob vs per-step probability  -> SHAPING outcome.
- QUANTISER MIX    -> knob vs per-step probability  -> SOURCE-SELECTION outcome. (SAME idiom as Sands items.)
- CHANGE ALLEY     -> draw                           -> CORRELATION outcome, visualised ON THE PINS. (Own idiom.)
So: mix aligns with the Sands-item probability-knob idiom; CA is the one with the distinct pin-outcome
idiom. (Corrects the prior "mix is simple / CA has its own controls" -- more precisely: mix ~ Sands-item
idiom, CA = pin-outcome idiom.)

Supersedes the "mix is a dumb raw selector, deliberately un-Sands-like" implication of the prior section
(the level distinction there is right about WHAT it controls; this fixes the IDIOM: mix presents like a
Sands item).

Cross-ref: PROBABILITY_MODIFIER_MODEL:25 (Sands note-probability = weight vs per-step draw = the shared
idiom), SANDS_ARCHITECTURE_CONSOLIDATION (Sands-item controls), CHANGE_ALLEY_DESIGN (pins = CA outcome
surface), the fourth-Philox-stream section (raw draw = the SOURCE of randomness, unchanged; idiom is a
separate layer).

## PANEL PLACEMENT (Rodney): q-mix as a SANDS LANE, position 3 (data-flow adjacency to rows 1-2)

### Integrate on Sands, NOT a separate panel (decisive)
A separate panel would NOT inherit Sands' features (per-voice ownership, Mono/Macro editing, send/blend,
the LOR-style per-step editing surface). q-mix NEEDS those, so it must be a Sands LANE. This settles the
integrate-vs-separate fork decisively toward integrate. (It also matches the idiom: q-mix is knob-vs-per-
step-probability = a Sands-item idiom.)

### Lane inventory (context)
- Sands Macro: melody, octave (CV-related pitch, rows 1-2), rest, accent.
- Sands Mono / East: those + extra mono rhythm lanes (the densest -- the geometry constraint).

### Position = THIRD, primary reason = DATA-FLOW ADJACENCY
q-mix in quantiser mode mixes the GENERATED MELODY CV -- which IS rows 1 and 2 (melody + octave together =
the full generated pitch) -- against the CV INPUT, per voice. So q-mix's OPERANDS ARE ROWS 1-2. Put the
control next to what it acts on -> q-mix belongs immediately AFTER rows 1-2 = position 3, directly beneath
the two lanes it consumes; its output (chosen source) then feeds articulation (rest, accent) below.

Corrects my earlier "first for signal-flow": q-mix is NOT upstream of melody generation -- it CONSUMES the
generated melody (rows 1-2), so it's DOWNSTREAM of them and belongs AFTER, not before. Rows 1-2 must
generate the melody CV first for q-mix to have something to blend. The true pipeline:
  rows 1-2 (melody, octave) generate the melody CV
  -> row 3 (q-mix) per voice blends that generated CV with external CV input = the effective pitch source
  -> rows 4+ (rest, accent) articulate whatever pitch won.

### Supporting reasons (align with third)
- Mode-conditional tidiness: q-mix is quantiser-mode-only (dims in sequencer mode). At row 3, the always-
  live melody/octave stay at the TOP in both modes; the conditional q-mix sits below them (not occupying
  the prime top slot while dark half the time). First-position would put a dimmed row at the top in
  sequencer mode.
- Grouping: rows 1-3 = the PITCH-SOURCE block (generate -> blend); rows 4+ = the ARTICULATION block. q-mix
  CLOSES the pitch-source block by resolving which source wins; articulation acts on the result. Layout
  tells the story.

### Implementer precision
q-mix blends the COMBINED melody+octave CV (rows 1-2 together = the full generated pitch) vs input CV, per
voice -- not melody alone. Sitting at row 3, directly under BOTH contributing lanes, makes it visually
obvious it blends the pair above it.

### Geometry
Shrink lane heights a bit to fit the extra lane. GATE: the densest panel (Mono/East with the extra rhythm
lanes) must stay usable across ALL lanes after the shrink -- check there first. If Mono/East is too tight,
either accept a per-panel layout difference (its own cost) or find headroom elsewhere. One shrink working
across all three panels is preferable.

### Level marking (from the idiom refinement)
q-mix shares the Sands-item IDIOM (knob vs per-step probability) but differs in LEVEL (it SELECTS a source;
the shaping lanes SHAPE the melody). Mark it as distinct-in-kind (separator / accent / grouping gap at the
pitch-source/articulation boundary) so it's not read as another shaping lane.

Phase-2 (panels) item -- reasoning captured now; pixel work waits for the panels phase.

Cross-ref: SANDS_PANEL_LAYOUT (where this lands), the idiom-refinement section above (Sands-item idiom +
level marking), the fourth-Philox-stream section (the draw under the knob), QUANTISER_MODES_UNIFICATION
(mode-dimming = q-mix dims in sequencer mode), ROAD_TO_RELEASE Phase 2 (panels).

## BUILD ORDER (Rodney): MVP the probability+blend first, THEN add Sands refinements
Staged plan (de-risking):
1. FIRST -- generate the probability + experiment with blending. Fourth Philox stream (STREAM_SOURCE_
   SELECT=3, voice in nonce) producing the per-voice source-select draw; basic per-voice mix knob; wire
   the input-vs-generated blend; and LISTEN. Minimal viable form: stream + knob + blend, testable with
   basic knobs, NO panel surgery.
2. THEN -- add the Sands refinements: integrate as the row-3 Sands lane (per-voice ownership, Mono/Macro
   editing, level-marking, the lane-height geometry).

### Why this order (not just "simple bit first")
The UNCERTAIN part is the MUSICAL question -- does per-voice input-vs-generated blending sound good? Does
the heterophony gradient work? Is random-within-mask the right divergence? Does knob-modulation handle
temporal shaping (vs flicker) as predicted? You can't answer these from a doc -- you have to HEAR it. The
Sands integration is KNOWN machinery (fully specced above). So build the minimal mechanism first to
VALIDATE THE MUSICAL PREMISE CHEAPLY, before committing panel real-estate / lane-shrink / level-marking. If
the blend needs adjusting (different random source, per-voice behaviour, a run-length after all), you find
out BEFORE building the polished lane. Prove the idea, then dress it.

Mirrors the project's macro "function first, then panels" at the micro scale: blend mechanism = function;
Sands lane = panel/refinement. The module's MVP (stream + knob + blend) is small, self-contained, and
answers the only real open question (does it sing?) at minimal cost.

Cross-ref: the fourth-Philox-stream section (the MVP mechanism), the panel-placement section (the THEN
step -- row-3 Sands lane), ROAD_TO_RELEASE (function-first-then-panels, mirrored here).

## FULL FEATURE SCOPE + the unifying symmetry (Rodney): q-mix completes the pitch side of external-input modification

### Layout distribution (big surface, all well-trodden -- each piece lands on the module whose role fits)
- Big 5 -> BIG 6 on Monsoon: q-mix (per-voice mix probability) becomes a headline modulation target,
  joining probability et al. (anyBig5Modulated -> Big6). Consistent with the idiom: q-mix is a knob-vs-
  per-step-probability like the other Big-5, so it belongs in that headline group.
- Monsoon cramped -> use more HP (accept a wider Monsoon to house the 6th headline control).
- Poly knobs -> expanded STRAITS (Straits is already the poly-voice expander -- the natural home for
  anything per-voice-poly; the per-voice q-mix knobs live here).
- Additional CV -> expanded CAUSEWAY (Causeway is the CV/mod path; q-mix mod CV inputs live here).
- Mix-in tap knob group -> SANDS MACRO (the row-3 blend/tap lane refinement).
"A lot of scope but all well-trodden": not inventing new homes -- extending established roles along their
grain (headline mod->Monsoon, poly->Straits, CV->Causeway, per-voice shaping-adjacent->Sands). Big in
surface, LOW in novelty-risk. The good kind of big feature.

### THE unifying symmetry (the conceptual crown)
Rhythm / legato / rest / accent can MODIFY EXTERNAL GATES.  Q-MIX can MODIFY EXTERNAL MELODY CV.
| Input stream        | Modifier(s)                  | What it does                          |
| External GATES      | rhythm, legato, rest, accent | modify / re-articulate incoming gates |
| External MELODY CV  | Q-MIX                        | modify / blend incoming melody        |
q-mix is to external melody CV what the articulation lanes are to external gates. SAME KIND of operation on
the two different external input streams: rest/accent modify external RHYTHM; q-mix modifies external PITCH.

### Why this reframes q-mix from a feature to a COMPLETION
The gate/rhythm side ALREADY had its external-input modifiers (legato/rest/accent modifying external
gates). The melody/pitch side was MISSING its equivalent. q-mix IS that missing equivalent. So the feature
isn't "add a blend knob" -- it's "give external melody CV the SAME modification suite external gates
already have." The instrument becomes SYMMETRIC in how it treats its two external input streams: both can
be TRANSFORMED, not just consumed. q-mix completes the pitch side.

### This justifies the whole distribution
- q-mix as Big-6: it's a headline external-input modifier like the gate-side ones.
- q-mix on Sands next to rest/accent: same-KIND operation, different STREAM (pitch vs rhythm).
- q-mix modulatable via Causeway CV: like the other modifiers.
The layout falls OUT of the symmetry -- q-mix goes everywhere its gate-side counterparts already are,
because it IS the pitch-side counterpart.

Cross-ref: EXTERNAL_GATE_ARTICULATION_CHECK (Big-5 headline mod targets incl probability; the gate-side
modifiers on external gates), the panel-placement section (q-mix on Sands Macro next to rest/accent = the
symmetry, visually), MODE_B_SPEC (external gate handling = the rhythm-side counterpart), QUANTISER_MODES_
UNIFICATION (external melody CV = q-mix's input), the idiom-refinement section (q-mix = Sands-item idiom =
why it sits with the gate-side modifiers).

## q-mix is ENOUGH (Rodney) + octave-quantise behaviour checked
q-mix is enough as the pitch-side addition -- the pitch side ALREADY has its richness (scale + fader-
control modulation + octave). q-mix just adds the source-blend on top; no fuller pitch-modifier suite
needed. q-mix + existing scale/fader/octave modulation IS the pitch-side suite.

### Octave-quantise behaviour (Rodney asked: fader octave-range vs nearest octave?) -- CODE CHECKED
Answer: NEAREST active degree within the INPUT'S OCTAVE +-1 -- NOT a fader octave-range, NOT naive single-
octave. SequencerEngine.cpp:1024-1054:
- octave = (int)vIn  (start from the incoming CV's own octave).
- For each active degree, search candidates in octave-1, octave, octave+1; pick the nearest, WEIGHTED by
  fader weight (radius = w * 1/12, score = w/dist -> a heavier fader has a wider capture radius).
- Fallback: globally nearest active degree across those 3 octaves if none within radius.
- degV(s) = s/12 at 12-TET default (byte-identical legacy), else tuning.degreeVolts(s) (snaps to ACTUAL
  tuning degrees under a custom/Micro-24 tuning).

### Why this is the right behaviour (better than either option asked)
- PRESERVES the input's register: centred on (int)vIn, only +-1 octave -> quantised output stays near
  where the input was. An external melody keeps its contour/octave; quantise fixes the PITCH CLASS to the
  scale without transposing the octave. Exactly right for feeding an external melody in.
- +-1 octave window handles boundary cases (snap to a degree in the octave above/below if genuinely
  nearest) -> no getting stuck snapping the wrong way.
- Fader weight widens capture -> a prominent degree attracts nearby input pitches (emphasis affects
  quantise capture, not just loudness). Nice.

### Implication for q-mix (octave asymmetry, by design -- not a bug)
The octave comes from (int)vIn = the INPUT's octave, NOT the octave lane. So when q-mix blends generated
vs input: the GENERATED pitch uses the OCTAVE LANE (octave randomisation/control); the INPUT pitch keeps
its OWN octave (input-driven). The two blended sources may sit in DIFFERENT octaves -- input = its own
register, generated = octave-lane-driven. Probably desirable (input keeps contour, generated wanders per
the octave lane), but worth knowing when tuning the blend: q-mix mixes two sources with different octave
logic.

Cross-ref: SequencerEngine.cpp:1024-1054 (the octave-search quantise), :990 nearestDegree, TuningTable
degreeVolts (custom-tuning snap), the octave lane (drives generated pitch, not quantised input), the
q-mix sections above (the blend inherits this octave asymmetry).

## Why the blend is COOL (musical rationale) + Melodicer note (Rodney)
Melodicer = VERMONA (hardware random melody generator), NOT Vult (who make DSP/Rack modules like the Freak
filters). Its exact quantise internals weren't gleaned -- and needn't be: it's hardware (not open Rack code
to read), and more importantly we're NOT matching it. What q-mix does is different in kind from a standalone
random-melody box: PER-VOICE BLEND of an EXTERNAL INPUT melody with GENERATED notes, both in the same
scale, same-or-different octave. The blend is the differentiator, not something to copy from a reference.

### Why it's cool (the "why a user will love this" -- launch material)
Mixing generated notes IN THE SAME SCALE, SAME OR DIFFERENT OCTAVE:
1. SAME SCALE = coherent by construction. Both input and generated are quantised to the same active scale
   (same MICROTONAL scale if a tuning source is attached). The blend NEVER clashes tonally -- generated
   notes are from the same degree set as the input. Not "input + random pitches" but "input + generated
   notes from the same scale". Coherence for free.
2. SAME OR DIFFERENT OCTAVE = textural range. Input keeps its own register; generated follows the octave
   lane -> a blended generated note can sit in the input's octave (close in-register variation, doubling-
   ish) OR a different octave (octave-displaced answer, low drone, high ornament). One degree of freedom
   spanning "subtle in-register variation" -> "octave-leaping counter-voice", all from the same scale, and
   controllable via the octave lane.

Together: coherent-by-construction (same scale, never clashes) + texturally wide (octave freedom) =
SAFE SURPRISE. Variation that's always harmonically safe yet register-wide -- surprise without dissonance,
register play without atonality. For the heterophonic elaboration the instrument targets (maqam/gamelan
core-line-plus-elaboration), that's the sweet spot: voices elaborating WITHIN the mode (same scale) ACROSS
registers (octave freedom), diverging from the input by controllable amounts (the q-mix probability).
Three composing degrees of expressiveness: HOW MUCH generated (q-mix), WHICH scale (tuning), WHICH octave
(octave lane) -> coherent-but-varied heterophony.

The coolness is STRUCTURAL (coherent + wide-range by construction), and it's a thing a standalone random-
melody generator doesn't do (no external-input blend, no microtonality, no per-voice octave logic). That's
the differentiation.

Cross-ref: the octave-quantise finding above (same-or-different octave = the input-octave vs octave-lane
asymmetry), MICROTONAL_MASTER (same scale incl microtonal via attached tuning source), the heterophony
sections (core-line + elaboration), LAUNCH_INTENT_AND_STORY (safe-surprise heterophony = a user-facing
selling point).

## Vermona Melodicer manual (C/D quantizer modes) -- convergences + octave unresolved (Rodney)
From the Melodicer manual, modes C (Quantizer 1) and D (Quantizer 2):

### What it establishes -- and how it compares to our engine
1. FADER HEIGHT = QUANTIZATION RANGE/WIDTH ("the higher a fader is raised, the wider its quantization
   range"). This is EXACTLY our fader-weight-widens-capture (SequencerEngine.cpp radius = w * 1/12, heavier
   fader = wider capture). CONVERGENT (we couldn't read the hardware -> independently arrived at the same
   idea): fader height controls BOTH membership AND capture width. Validates our weighted-capture quantise.
2. 0-5V input range (CV IN 2). Matches our clamp (pe_clamp(vIn, 0, 5)). Both = a 5-octave window.
3. Quarter-note quantise (Melodicer clocks on quarter notes) -- a Melodicer timing detail, not relevant to
   our per-step engine.
4. C vs D split by TRIGGER SOURCE: C = clock-driven, D = GATE-driven (gate at GATE IN 2 -> quantize CV IN
   2). Striking parallel to OUR quantiser modes also splitting clock vs gate. Convergent (possibly why our
   modes are C/D too, or coincidence -- either way aligned).

### Octave: manual is NOT conclusive (Rodney)
The manual only says 0-5V and refers to the melody faders -- it does NOT specify nearest-octave vs input-
octave-preserving vs fader-range folding. Silent on octave. So there's NO reference behaviour to defer to,
which is fine: our engine already has a well-defined, reasoned octave behaviour (nearest active degree
within input-octave +-1, register-preserving -- code-verified above). We don't need Melodicer to decide it;
ours is already good. The manual's silence suggests Melodicer may just do the simple within-0-5V thing with
no special octave logic, whereas our input-octave+-1 search handles boundary cases gracefully -- plausibly
MORE refined, not behind.

### Overall
Where Melodicer specifies, we ALIGN (fader-width, 5V range, clock-vs-gate split -- convergent with a
respected reference). Where it's silent (octave), our reasoned behaviour stands unchallenged. And it
clarifies the DIFFERENTIATION boundary: Melodicer = single-channel quantise (one CV in, faders = scale,
quantize out) = roughly OUR QUANTISE CORE (which we match/exceed). q-mix + poly + microtonal + per-voice
octave = everything BEYOND Melodicer = our contribution. Melodicer is the baseline quantiser; dot.modular
is that baseline plus the blend/poly/microtonal layers.

Cross-ref: the octave-quantise finding above (input-octave +-1, fader-weighted -- now confirmed convergent
with Melodicer on fader-width + 5V, and our independent choice on octave), MICROTONAL_MASTER (microtonal =
beyond Melodicer), the why-cool section (the blend = beyond Melodicer).

## Octave: keep default, OFFER a context-menu "clamp/fold to octave fader range" (Rodney)
Default stays as-is: nearest active degree within input-octave +-1, register-preserving, fader-weighted
(best-guess, benefits discussed -- preserves the input's contour). Add an OPTIONAL context-menu toggle.

### The option: "clamp to octave fader range"
Instead of preserving the input's own octave, fold/clamp the input into the octave range the octave
lane/fader defines. Default: input at 4V stays near 4V (register preserved). Option ON: input at 4V is
brought into the octave-control's range (register IMPOSED by the octave setting, not preserved).

### Why it's a real option (two valid intents, hence a CHOICE not a baked default)
- Register-as-INPUT (default): "play my melody, corrected to scale, WHERE I played it." Good when the
  input's octave matters (bassline stays low, lead stays high).
- Register-as-CONTROL (option): "take my melody's PITCH CLASSES, put them in THIS register." Good when the
  input is a pitch-class source and the octave control places it (compress a wide input into a tight
  octave, or relocate it). "I care about the notes, not where they were played."
Both valid -> user choice. Default to register-preserving (safer/faithful-to-input); offer range-fold for
the "fold to my octave range" intent.

### Semantics to pin (make it a real spec)
1. What defines the range? The octave lane/fader -- CHECK whether it's a contiguous min-max range or a SET
   of enabled octaves (decides fold-into-window vs fold-into-enabled-octaves).
2. FOLD vs CLAMP:
   - Clamp: input above range -> pinned to top octave; below -> bottom. Flattens out-of-range notes to the
     boundary.
   - Fold (octave-reduce): octave-wrap the input INTO the range (mod), preserving pitch class + interval
     structure, relocating octave. Keeps the tune, octave-wrapped.
   Rodney's phrase says "clamp", but FOLD is usually the more musical choice (clamp flattens a melody that
   exits the range into a monotone at the boundary; fold keeps the melodic shape). LEAN: fold (octave-
   reduce). Could offer clamp as the literal option + note fold as the musical alternative.

### Bonus: this option ALSO aligns q-mix octaves
Recall the q-mix octave asymmetry (input keeps its octave; generated follows the octave lane -> can sit in
different octaves). With "fold to octave range" ON, the INPUT is also brought into the octave-lane range ->
input and generated share the same octave world. So the option DOUBLES as the fix for "I don't want input
and generated in different octaves": turn it on and they align. It unifies the octave logic across both
blend sources -- a nice bonus beyond the input-fold intent.

### Status
Context-menu toggle on the quantise octave behaviour. Default OFF (register-preserving). Pin fold-vs-clamp
+ the octave-range semantics before building. Small, self-contained UI option.

Cross-ref: the octave-quantise finding (the default behaviour + the q-mix asymmetry this option resolves),
the octave lane (defines the range -- check its min-max vs enabled-set semantics), the why-cool section
(same-or-different octave -- this option lets the user CHOOSE same).

## Octave-range option PINNED (Rodney): min/max via 2 Monsoon faders, FOLD not clamp
Two decisions resolving the open semantics above:
1. RANGE = contiguous MIN/MAX window, set by 2 dedicated MONSOON faders (octave-min + octave-max). Not an
   enabled-octave set -- a contiguous [min, max] window. First-class Monsoon controls (two faders on the
   host).
2. FOLD, not clamp: out-of-range input is octave-REDUCED (wrapped) into the [min, max] window, preserving
   pitch class + the melody's interval structure (tune survives, octave-relocated). NOT pinned-to-boundary.

### Fully-pinned behaviour
"Fold to octave range" (context-menu option, DEFAULT OFF):
- OFF (default): register-preserving (input keeps its own octave; nearest degree within input-octave +-1).
- ON: input CV octave-folded into the [min-fader, max-fader] window -- wrap by octaves into the window
  span until inside, preserving pitch class.

### Implementation notes
- Fold is a modulo over the WINDOW SPAN, not a fixed 1-octave mod. For window width W = (max - min + 1)
  octaves: folded octave = min + ((octave - min) mod W), keeping the within-octave pitch class intact.
- Edge case min == max (W = 1): everything folds to that single octave (the tightest "force everything
  into octave K" setting -- valid). Confirm the fold math handles W=1 with no divide/off-by-one.

### q-mix octave-alignment bonus -- one thing to confirm
With fold ON, the INPUT folds into [min, max]. Whether this fully unifies q-mix octaves depends on whether
the GENERATED notes' octaves ALSO respect these same min/max faders (vs a separate octave control):
- Same min/max govern generated too -> fold-ON gives FULLY unified octave behaviour (input + generated
  both in [min, max]). Cleanest.
- Generated uses a different octave control -> fold-ON aligns the input to the window but generated may
  still roam per its own control.
CONFIRM: do the min/max octave faders bound the generated pitch too, or only the folded input? (Not
blocking; decides how complete the octave-alignment bonus is.)

Cross-ref: the octave-range option section above (this pins its two open questions), the octave lane /
Monsoon octave faders (min/max = the 2 faders; confirm they bound generated too), the q-mix octave
asymmetry (fold-ON is the alignment switch).

## UNIFIED octave window: same min/max faders for BOTH generation AND quantisation (Rodney)
Confirmed: the same min/max octave faders govern BOTH generated notes AND folded quantised input. So the
octave range is a SINGLE UNIFIED control over the whole pitch output -- generated notes are placed within
[min,max], and (fold ON) input is folded into [min,max] too. One window, both sources -> octave behaviour
COHERENT BY CONSTRUCTION (everything lives in [min,max], generated or quantised-input). This resolves the
earlier "confirm whether generated respects the same faders" question: YES, same faders -> FULLY unified.

### The octave-behaviour space (small controls, wide musical range)
Three composing controls: min/max faders (the window), q-mix (how often generated vs input), fold on/off
(is input confined to the window or keeps its own register).
- Narrow LOW window + fold ON + low q-mix = tight BASSLINE with occasional generated bass variation.
- Narrow low window, max nudged UP + moderate q-mix = BASSLINE WITH HIGHER-OCTAVE FLOURISHES (Rodney's
  example): folded input stays anchored low (the bassline), generated blend reaches into the higher octave
  the widened window allows (the occasional higher notes).
- WIDE window + fold OFF + high q-mix = input keeps its register, generated roams widely = a melody with
  free octave-leaping counter-voices.
- Wide window + fold ON = everything (input + generated) spread across the full range = full-register
  heterophony.
Each is a distinct, musical behaviour from a couple of fader/knob settings. Few controls, wide range, every
setting musically useful -- and coherent throughout because the SAME window bounds both sources (generated
can't wander outside the set register).

### "Same or different octave" is now a TUNABLE AXIS, not just an asymmetry
The earlier q-mix "same or different octave" freedom is now precisely controllable: the min/max WINDOW WIDTH
+ fold toggle IS the dial for how much octave spread the blend has. Narrow window -> generated + input share
the register (same-octave blend); wide window -> generated can leap octaves (different-octave blend). So the
coolness (coherent-but-wide heterophony) is DIALABLE via the octave window width. Completes the "why it's
cool" picture: safe-surprise heterophony with a continuous octave-spread control.

Cross-ref: the octave-range-option PINNED section (min/max = 2 Monsoon faders, fold; now confirmed they
bound generated too = fully unified), the why-cool section (same/different octave = now the window-width
axis), the q-mix octave asymmetry (resolved: one window governs both, fold aligns, width sets spread).

## Source-select dice: SEPARATE, not keyed off the melody dice (Rodney) -- and the 4th-stream spec already does this
Decision: the source-select (input-vs-generated) uses its OWN dice, independent of the melody draw. NOT
keyed off the melody dice.

### Rodney's deciding argument (correct): two orthogonal creative gestures
- Separate -> try DIFFERENT BLENDS of the SAME generated melody: hold melody dice fixed, roll ONLY the
  blend dice -> same generated notes, different interleave pattern with the input. "I like these generated
  notes -- now audition WHERE they weave in."
- Separate -> SAME BLEND of DIFFERENT generated melodies: hold blend dice fixed, roll ONLY melody dice ->
  same interleave pattern, different generated content in those slots. "I like this interleave rhythm --
  now hear different generated notes in the same slots."
Keying-off-melody DESTROYS both: it welds "which generated notes" and "where they interleave" into one
dice -> can't hold one fixed and vary the other. Separate = two orthogonal axes (WHAT generated / WHERE
used), each independently explorable = exactly the dice/scrub independent-exploration philosophy.

### Why the counter-argument (keyed-off gives coupling for free) is weak
- The coupling you'd actually want ("take generated more when it's interesting") is CONTENT-dependent, not
  shared-seed. Shared-seed coupling is ARBITRARY (blend pattern happens to derive from the same numbers) =
  meaningless correlation, a loss of independent control dressed as a feature.
- Real correlation is PATCHABLE anyway (the mix knob is modulatable -> patch the blend to track something).
  So separate loses nothing you actually want (real correlation via a cable) and gains both gestures.

### The clean statement
Melody dice and blend dice control ORTHOGONAL things -- WHAT the generated notes are vs WHERE they get
used. Welding them removes control without adding musically-meaningful coupling. Separate keeps the axes
independent = more expressive AND more conceptually honest (they ARE different decisions -> different dice).

### It's what the 4th-Philox-stream spec ALREADY implements
STREAM_SOURCE_SELECT = 3 is a SEPARATE stream from STREAM_MELODY = 1 (different keys -> decorrelated). So:
- Roll melody dice (stream 1), hold source-select (stream 3) -> different generated notes, same interleave.
- Roll source-select (stream 3), hold melody dice (stream 1) -> same generated notes, different interleave.
The two independent streams DELIVER the two gestures directly. And it's CONSISTENT with the existing
architecture: rhythm(0)/melody(1)/CA(2) are already separate streams so each independent decision varies
independently; source-select(3) as a separate stream is the SAME principle. Keying-off would have been the
INCONSISTENT choice.

### Verdict
SEPARATE dice, decisively: Rodney's two-gestures reason + shared-seed coupling is arbitrary (real
correlation is patchable) + consistent with the per-decision-stream architecture + the 4th-stream spec
already implements it. (Had it right in the question.)

Cross-ref: the fourth-Philox-stream section above (STREAM_SOURCE_SELECT=3, separate from melody=1 -- this
IS the separate dice), PhiloxRng per-stream keys (rhythm/melody/CA already separate = the same principle),
DICE_SCRUB_MODEL (independent-exploration philosophy = why orthogonal dice matter).

## Panel growth + CA pin colours corrected + third-pin constraint (Rodney)

### CA pin colours -- CODE is authoritative (CHANGE_ALLEY_DESIGN.md is STALE)
Code (MonsoonChangeAlleyV2.hpp:5,761-762): WHITE = rhythm (nvgRGBf 0.95,0.95,0.94), RED = melody (nvgRGBf
0.83,0,0.10), rendered CONCENTRIC when both (white peg + red inset dot in one cell). So: red + white
(Rodney), with white=rhythm, red=melody. CHANGE_ALLEY_DESIGN.md:163 is wrong on BOTH ("rhythm red, melody
teal") -- fix it: rhythm=WHITE, melody=RED, concentric. (Doc drifted from code.)

### Reinforces: source-select should NOT be a full third pin plane
The two pin types already use the cell's two natural concentric layers (outer white peg + inner red dot).
A THIRD pin type has no clean concentric slot -- it'd need a third ring/position, cramping the cell
rendering (beyond just needing a third colour legible against red+white). So the concentric-cell geometry,
not only the palette, argues against a co-equal third pin plane. -> favour the earlier OPTION 2: source-
select correlation FOLLOWS the melody pin by default, with an UNLINK toggle for independent blend-
correlation when wanted. Gets the orthogonality capability (consistent with the separate dice) without
straining the pin cell. (Full third plane only if blend-correlation proves central enough to redesign the
cell.)

### Panel growth (accepted) -- the external-input symmetry has a PHYSICAL cost
The feature grows the panels: more KNOBS (q-mix per-voice knobs + the min/max octave faders) AND GATE
INPUTS. The gate inputs = Mode B external-gate handling (MODE_B_SPEC: external gate as note-length source,
bridge-to-next-gate, reusing rest/legato) = the RHYTHM side of the external-input symmetry. So the
symmetry (external gates <- rhythm/legato/rest/accent; external melody CV <- q-mix) now has a physical
consequence: BOTH external input streams need input JACKS -- external melody CV (pitch side, for q-mix) +
external gates (rhythm side, Mode B). Panel budget must cover both, across Monsoon (more HP, Big-6) +
Straits (poly knobs) + Causeway (CV). Accept bigger panels; all well-trodden.

Cross-ref: CHANGE_ALLEY_DESIGN.md:163 (STALE colours -- correct to white=rhythm/red=melody/concentric),
MonsoonChangeAlleyV2.hpp:5,761-762 (authoritative colours + concentric cell), the third-pin question above
(concentric-cell constraint -> option 2 default-follow-with-unlink), MODE_B_SPEC (external gate inputs =
the rhythm-side jacks), the full-feature-scope section (Big-6/Straits/Causeway distribution -- now + gate
input jacks).

## DECISION: source-select gets a FULL third pin plane (green) -- EMS precedent dissolves the objection (Rodney)
Rodney's lean: full-blown option 1 (a co-equal third pin plane), not option 2 (follow-melody-with-unlink).
And the EMS point is the deciding factor.

### The EMS precedent removes the palette/aesthetic objection
Real EMS VCS3/Synthi matrices used MULTIPLE pin colours -- white and red (standard values), GREEN, and
occasionally YELLOW (colour encoded the pin's resistance/attenuation). CA's lineage IS explicitly the EMS
matrix (CHANGE_ALLEY_DESIGN cites VCS3/Synthi). So a THIRD pin colour is NOT a departure -- it's MORE
faithful to the source. A red/white/GREEN matrix is more EMS than red/white. My earlier "third colour
strains the palette / departs the aesthetic" caution was BACKWARDS: green is the historically-correct
third pin, and it makes CA more authentic, not less. Two of my three objections fall:
- "no third legible colour" -> GREEN (EMS-authentic, distinct from red+white).
- "departs the aesthetic" -> MORE faithful to EMS, not less.

### What remains is only cell rendering -- solvable, and also EMS-ish
The cell currently renders two concentric layers (outer white peg + inner red dot). A third pin needs a
slot. But EMS matrices were DENSE with multi-coloured pins -- three pins in/around a cell is very EMS.
Rendering options: a third concentric ring (outer/mid/inner), or a third pin variant. Busy but not
impossible; a solvable PANEL-PHASE rendering problem, not a blocker.

### Why full-1 is right (objections thinned)
1. CONSISTENCY: we separated the DICE (source-select = own stream) for orthogonality. A full third PIN
   plane makes the correlation structure equally orthogonal -> blend cross-voice correlation as
   independently controllable as its dice. Option 2 was a COMPROMISE forced by a UI constraint EMS-green
   REMOVES. Constraint gone -> the principled choice (full orthogonality, matching the dice) wins.
   Separate dice => separate pins.
2. FULL TEXTURE SPACE: independent control of WHICH generated notes (melody pins), WHERE voices break to
   generated (source-select pins), WHICH rhythm (rhythm pins) -- three separate correlation structures =
   the complete heterophony control.
3. MORE EMS: three pin colours strengthen the homage; the module's identity ("the EMS matrix reborn for
   correlation") is better served by the historically-accurate palette.

### Decision + the one open item
DECISION: full-blown option 1 -- source-select = a third co-equal pin plane, GREEN (EMS white/red/green).
Supersedes the prior option-2 lean (which rested on a palette/geometry constraint the EMS heritage
dissolves). OPEN (panel phase): the three-pin CELL RENDERING (concentric tri-ring or a clean alternative)
-- resolve at panel time; if three pins in a cell proves unparseable in practice, option 2 is the fallback,
but design for full-1.

Cross-ref: CHANGE_ALLEY_DESIGN (EMS VCS3/Synthi lineage -- now white/red/GREEN, more faithful),
MonsoonChangeAlleyV2.hpp (two-concentric-layer cell -- extend to three), the separate-dice decision (full-1
is the pin-plane consistent with it), the third-pin question above (resolved: full-1, EMS green)." 

## CORRECTION -> FULL third pin plane (Rodney): EMS matrices had white/red/green (+ occasional yellow)
My "concentric cell only holds two layers, so no third pin" caution was WRONG ON THE PREMISE. Real EMS
matrices (VCS3/Synthi -- the lineage Change Alley explicitly cites) used WHITE, RED, GREEN pins, and
OCCASIONALLY YELLOW (colour-coded by resistance/patch function). So a third pin type is not a violation of
the idiom -- it RETURNS to the idiom's real richness. I mistook the CURRENT two-colour concentric
implementation for a constraint; the SOURCE MATERIAL is 3-4 colours.

### Decision: FULL third pin plane for source-select (Rodney's lean = option 1)
Withdrawing the earlier option-2 (follow-melody-with-unlink) hedge. Full independent third pin plane is
right:
1. AUTHENTIC to the EMS matrix -- green/yellow have real precedent; a green source-select pin alongside
   white-rhythm + red-melody is MORE faithful to the VCS3/Synthi heritage, not less. Concentric rendering
   was an implementation choice, not a law.
2. CONSISTENT with the separate dice -- source-select already has its own independent dice (separate
   stream) because "what generated / where interleaved" are orthogonal; a full third pin plane makes the
   CORRELATION STRUCTURE equally independent = the honest completion. Every independent decision gets BOTH
   its own dice AND its own pin plane. Dice and pins match: rhythm, melody, source-select = three streams,
   three pin planes.
3. PANEL GROWING ANYWAY -- bigger panels already accepted (knobs + gate inputs), so "third pin adds UI
   cost" loses force; a third plane on a larger CA matrix is affordable within the expansion.

### Rendering (how, not whether)
- COLOUR: white=rhythm, red=melody, GREEN=source-select (the natural third EMS colour). Yellow held in
  reserve for a possible FOURTH pin plane later (CA's own / a future stream). Palette stays historically
  grounded, not an arbitrary third hue.
- LAYOUT: EMS boards placed pins POSITIONALLY in the cell field, not strictly concentric. Three pins per
  cell scale better positionally (small offsets) than as three concentric layers (peg+dot+ring gets busy).
  Lean: positional-in-cell, EMS-authentic, scales to 3-4 pins.

### Meta
The HISTORICAL SOURCE MATERIAL resolved a tension my implementation-bound reasoning got wrong. The module
is a tribute (Singapore, and here the EMS matrix lineage); "what did the real thing do?" is a legitimate,
clarifying design authority. Real EMS boards had green pins -> so can Change Alley.

Supersedes the "option 2 / no full third plane / concentric-cell constraint" conclusion above (premise was
wrong: EMS matrices held 3-4 pin colours).

Cross-ref: CHANGE_ALLEY_DESIGN.md:24 (EMS VCS3/Synthi lineage cited), MonsoonChangeAlleyV2.hpp (current
2-colour concentric -- to extend to a 3rd green positional pin), the separate-dice section (independent
dice -> independent pin plane = consistency), the panel-growth section (bigger panels absorb the 3rd pin).

## Pinning CA's OWN draws (the scatter) -- buys vs costs (Rodney): probably NOT worth the reserved yellow
"CA's own draws" = the SCATTER (corrKey, STREAM_CA=2): the randomness that decides the pin REALLOCATION/
scatter TOPOLOGY itself when CA re-scatters. So CA could pin three things: rhythm (rerouted), melody
(rerouted), and its own SCATTER draw. Pinning the scatter = making the RESTRUCTURING correlate across
voices (voices re-scatter TOGETHER) rather than each reallocating independently.

### BUYS
1. Correlated RESTRUCTURING, not just correlated values. Pinned-scatter voices move their source-
   assignment in lockstep when CA restructures (phrase boundary) -> the ensemble REORGANISES as a bloc. A
   second-order correlation: not "same notes" (that's rhythm/melody pins) but "change their relationships
   together".
2. Reproducible FAMILIES of restructuring -- a recognisable "way of reorganising" across a group vs pure
   per-phrase chaos.

### COSTS
1. SECOND-ORDER, hard to hear/reason about. Pinning rhythm = immediately audible ("same rhythm"). Pinning
   scatter = a correlation OF a correlation-change -- abstract, effect only indirect (via how value-
   correlations shift) and only AT re-scatter moments. A knob whose effect is a derivative of a
   derivative; users can't easily predict/perceive it.
2. REDUNDANT-ish: the rhythm/melody pins ALREADY control correlation topology directly. Correlating HOW
   the auto-scatter reassigns is a subtle refinement of a thing you can mostly set directly. Small
   marginal gain over "just pin the sources you want".
3. CONCEPTUAL MUDDINESS: CA exists to set correlation; pinning its own scatter = CA applying its mechanism
   to ITSELF. Near-self-referential; risks confusing the mental model.
4. AGAINST THE GRAIN of a settled decision: PHILOX_KEY_DERIVATION_AND_CA_SEED = "Change Alley has no seed
   and needs NONE OF ITS OWN" (scatter seeds FROM Monsoon, same source as rhythm/melody, deliberately no
   independent randomness identity). Pinning CA's own scatter pushes the opposite way (treating scatter as
   a first-class independently-correlatable stream). Not fatal, but a tension.

### Verdict
WEAKEST of the pin candidates -- much weaker than source-select (first-order, directly audible: "these
voices break to generated together"). Source-select as the GREEN third pin plane is justified (direct +
musical). Pinning CA's own scatter is INDIRECT + abstract, with questionable marginal value over direct
source-pinning, and half-contradicts "CA needs no seed of its own". So the RESERVED YELLOW fourth plane
should probably NOT be CA's own scatter draws. If a fourth plane ever earns its place, more likely a fourth
first-order MUSICAL decision stream (audible) than CA self-pinning.

Cross-ref: PHILOX_KEY_DERIVATION_AND_CA_SEED (STREAM_CA=2 scatter; "no seed of its own" -- the grain this
pushes against), the third-pin section (green source-select = first-order, justified; yellow reserved but
NOT for CA-self-pinning on this analysis), CHANGE_ALLEY_DESIGN (pins already set topology directly = the
redundancy point).

## CORRECTION (Rodney): point 4 WRONG -- CA DOES have its own draw (I misread "no seed of its own")
My cost-4 ("against the grain of 'CA needs no seed of its own'") was based on a MISREAD and is WITHDRAWN.
CA absolutely has its own draw. Code (MonsoonChangeAlleyV2.hpp:62-87):
- 8 SCATTER DRAW streams: Intra/Inter x rhythm/melody x domain/codomain. Each a SIGNED int64 addressable
  POSITION in its own domain-separated Philox (scatterCounter[8]).
- The scatter draw builds a transient PhiloxRng from corrKey[ci] and reads at(scatterCounter[ci]) -- CA
  ACTIVELY draws from its own keyed streams to decide scatter.
- SIGNED/addressable -> CA's own scatter is REVERSIBLE/reproducible like the other draws (survives dice/
  scrub).
- Keys derived from MONSOON's seed: corrKey[i] = deriveKey(seedValue, STREAM_CA + i).

What "Change Alley has no seed and needs none of its own" ACTUALLY meant: CA needs no separate external
seed JACK -- it derives its scatter keys FROM MONSOON. That is about WHERE THE SEED COMES FROM (Monsoon,
not a dedicated input), NOT about whether CA has its own draw. I conflated "no separate seed input" with
"no own draw". Entirely different. CA has a rich own-draw system (8 independent signed addressable
reversible scatter streams), seeded from Monsoon.

### Revised buys/costs
- COST 4 (grain): VOID. CA's own draw IS the grain; pinning it isn't against anything.
- The phantom "needs new reversible machinery" worry: ALSO VOID -- the scatter is already signed/
  addressable/reversible, so pinning CA's own scatter inherits reversibility FOR FREE, like the other pins.
- Already 8 structured streams (Intra/Inter x r/m x dom/cod) -> pinning across CA's scatter is far more
  NATURAL than I implied; the streams exist, structured, addressable.
- COSTS 1-3 STILL STAND as real (musical, not architectural): second-order/abstract (correlation of a
  correlation-change), redundant-ish (rhythm/melody pins set topology directly), conceptually self-
  referential.

### Revised verdict
The MECHANISM fully SUPPORTS pinning CA's own scatter -- own draws, reversible, structured, seeded from
Monsoon. It does NOT fight the architecture (my earlier claim was a misread). The only remaining caution is
MUSICAL: is correlated-restructuring (second-order) worth its abstractness vs directly pinning sources?
That's a weaker, judgment-call objection -- not an architectural block. So pinning CA's own scatter is MORE
VIABLE than my prior "weakest candidate / probably not the yellow" conclusion implied: mechanically clean,
musically a question mark. Reserve yellow for it IF the second-order restructuring texture proves wanted;
the architecture is ready either way.

Supersedes point 4 and the "against the grain / needs new machinery" reasoning in the prior CA-own-draws
section (both were misreads of "no seed of its own").

Cross-ref: MonsoonChangeAlleyV2.hpp:62-87 (8 signed addressable scatter streams, seeded from Monsoon =
CA's own draw), PHILOX_KEY_DERIVATION_AND_CA_SEED ("no seed of its own" = no external JACK, seeds from
Monsoon -- NOT no own draw), the prior CA-own-draws buys/costs (costs 1-3 stand, cost 4 withdrawn).

## Re-rating CA-self-scatter-pinning (Rodney): thematic pull + pin-4 is cheap. Mull, not high priority.

### The self-reference is ON-THESIS (reframes the "self-referential" cost as partly a feature)
dot.modular is about MIXING ORDER AND CHAOS. CA pinning its own scatter is exactly that idea applied to
ITSELF: imposing ORDER (correlation) on the mechanism that generates DISORDER (the scatter). The scatter
IS the chaos (randomly reorganises the correlation topology); pinning it IS the order (makes that
reorganisation cohere) -> controlled chaos of the chaos-controller = a second layer of the exact order/
chaos dialectic the instrument explores. Modular AS A FORM has always had this reflexive streak (modulating
a modulator, self-patching, LFO modulating its own rate; the EMS matrix invited self-patching), so it's
IDIOMATIC to modular's spirit, not a novelty. And it's a discovery-pleasure ("I can pin the scatter's own
reorganisation?") that makes an instrument feel deep. So the earlier "conceptual muddiness / self-
referential" COST is, through the order/chaos lens, partly a FEATURE: it muddies the mental model (real UX
cost) but CLARIFIES the artistic identity (order and chaos all the way down). Cognitive cost vs thematic
payoff -- worth mulling.

### Pin-4 marginal cost is SMALL (pin-3 already forces the mechanism)
Even with THREE pins (white/red/green), left/right-click no longer addresses all types -> a third pin
ALREADY forces a MODIFIER KEY or EDIT-MODE SELECTOR ("now placing green pins", or modifier+click). That's
a FIXED cost paid at 2->3. Once that selection mechanism exists, 3->4 is nearly free (one more mode/colour
on an existing selector). So:
- 2->3: costs the whole selection mechanism (modifier/edit-mode). Significant, front-loaded.
- 3->4: one more mode/colour on the existing selector. Small.
-> the reserved YELLOW fourth pin is CHEAPER than it looked: pin-3 bought the infrastructure pin-4 rides
on. CA-self-scatter becomes "we have the slot + mechanism; question is whether the MUSICAL payoff justifies
wiring it" -- a much lower bar than "build a whole new interaction".

### Combined re-rating
The two points COMPOSE: self-reference has THEMATIC appeal (order/chaos reflexivity, idiomatic to modular)
AND is CHEAP to add (rides pin-3's necessary selector). Together they shift CA-self-scatter-pinning from
"weakest candidate, probably not worth it" to "quietly appealing, cheap to slot in, worth mulling". Not a
must; a real re-rating. Status: MULL, NOT HIGH PRIORITY -- but do not dismiss on the stale "weakest
candidate" verdict; the mechanism supports it (own draws, reversible), the cost is low (pin-3's selector),
and the thematic fit is strong (order/chaos on itself).

Cross-ref: the prior CA-own-draws sections (mechanism supports it; costs 1-3 were musical not architectural
-- and the self-referential one is partly a feature here), the third-pin section (green pin forces the
selector that makes pin-4 cheap), LAUNCH_INTENT_AND_STORY (order/chaos thesis -- the self-reference
embodies it).

## REALITY CHECK (Rodney): the CA draw dimension is 8 (side/type/direction), NOT per-voice-16
Before getting carried away with self-referential pinning: checked the CA scatter draw's actual dimension
(MonsoonChangeAlleyV2.hpp:62-67). It is NOT per-voice-up-to-16. It is 8 STREAMS = Intra/Inter x rhythm/
melody x domain/codomain (CA::SIDES * CA::TYPES * 2). Indexed by (side, type, direction), NOT by voice.
Fixed at 8; does NOT scale with poly voice count.

What the 8-stream draw PRODUCES is the 16x16 pin-matrix reallocation (rows 0-15 = consuming voices, cols
0-15 = source voices), but the DRAW ITSELF is 8 streams generating that scatter -- not 16 per-voice draws.

### This DEFLATES the "pin CA's own draws across voices" idea (I conflated dimensions)
The "pin CA's scatter -> voices re-scatter as a bloc" framing assumed the CA draw was PER-VOICE (so voice
2's scatter could correlate with voice 3's). It ISN'T -- it's 8 (side/type/direction). So "pin CA's own
draws across voices" does NOT map onto the pin-matrix idiom: the matrix correlates VOICES (row v reads col
src), but the scatter draw isn't a per-voice quantity -- there's nothing per-voice to pin to another voice.

Correction -- two dimensionally DIFFERENT things I conflated:
- SOURCE-SELECT: IS per-voice (each voice's input-vs-generated decision) -> CAN be a pin plane on the 16x16
  voice matrix. Green pin: valid, per-voice, fits the idiom. STANDS.
- CA's OWN SCATTER draws: 8 (side/type/direction), NOT per-voice -> CANNOT be "a pin plane on the voice
  matrix" the same way. No per-voice scatter quantity to correlate voice-to-voice. The self-referential-
  pinning-as-a-fourth-matrix-plane idea was DIMENSIONALLY CONFUSED.

### What CA-self-scatter-correlation could mean (given the real 8-dim), if anything
Correlating the 8 scatter streams WITH EACH OTHER (rhythm-domain like melody-domain; Intra tracking Inter)
-- but that's NOT a voice-matrix pin, it's a different, much smaller (8-element) correlation with no natural
home on the 16x16 grid. So "yellow fourth pin plane on the matrix = CA self-scatter" does not fit.

### Verdict
Answer to "still linked to poly up to 16?": NO -- the CA draw is 8 (side/type/direction), fixed, not poly-
scaled. So the self-referential-pinning-as-a-fourth-matrix-plane idea we got enthusiastic about does NOT
fit the mechanism (it assumed a per-voice CA draw that doesn't exist). The THEMATIC appeal (order/chaos
reflexivity) is still conceptually nice, but the concrete "fourth pin plane on the voice matrix" is
dimensionally wrong -- rethink or drop. Good instinct to check the mechanism before the concept ran off.

Supersedes the re-rating's implicit assumption that CA-self-scatter could be a voice-matrix pin plane like
green source-select. Green source-select (per-voice) stands; CA-self-scatter-as-matrix-pin does not.

Cross-ref: MonsoonChangeAlleyV2.hpp:2-4 (16x16 voice matrix) + :62-67 (8-stream side/type/direction scatter
draw = the dimension mismatch), the re-rating section above (its voice-matrix-pin assumption for CA self-
scatter was dimensionally confused; the thematic point survives, the implementation doesn't), the source-
select third-pin section (per-voice -> valid, unaffected)." 

## CA scatter draw: 8 -> 12 dim with green q-mix pins; self-scatter = "Change Change Alley" (Rodney)

### Dimension update: adding green q-mix pins takes the scatter draw 8 -> 12
Current CA scatter draw = 8 = Intra/Inter x {rhythm, melody} x domain/codomain (2x2x2). Adding the GREEN
q-mix pins makes q-mix a THIRD pin type -> Intra/Inter x {rhythm, melody, q-mix} x domain/codomain =
2x3x2 = 12. So the scatter draw grows 8 -> 12 as a CONSEQUENCE of the source-select pin plane. Consistency
check: the green pin isn't just a per-voice pin on the 16x16 voice matrix -- it ALSO adds a scatter-stream
family (its own Intra/Inter x domain/codomain), taking CA's own-draw 8 -> 12. The two views (per-voice
green pins on the voice matrix; +4 scatter streams for q-mix) are the SAME addition at two levels. Hangs
together.

### "Change Change Alley" -- self-scatter is a SEPARATE 12-D matrix / module, not a 4th pin on CA
Correlating the 12-dim scatter draw = a different animal, requiring a SEPARATE 12-D matrix, NOT a fourth
pin on the 16x16 voice matrix. Because:
- The 16x16 voice matrix correlates VOICES (row v reads col src) = per-voice quantities.
- The scatter draw is NOT per-voice; it's 12 (side/type/direction). Correlating THOSE with each other needs
  a matrix whose rows/cols are the 12 SCATTER STREAMS -- a distinct 12x12 surface, a distinct module,
  one meta-level up.
Name: "CHANGE CHANGE ALLEY" (Rodney) -- Change Alley applied to Change Alley; recursion made literal, the
self-patching spirit of modular taken to its logical (delightfully absurd) conclusion. It re-scatters CA's
own scatter.

### Status: mull / if-done-at-all -- but now dimensionally COHERENT, not confused
- It's a whole separate module (12-D matrix) for a THIRD-ORDER effect (correlating the correlations-of-
  correlations). Abstraction stacks fast: CA correlates voice draws; CCA correlates CA's SCATTER draws.
  Two meta-levels from anything directly audible.
- Payoff even MORE indirect than the (already second-order, already-deflated) CA-self-scatter idea.
- BUT conceptually clean now it's correctly homed: separate module, separate matrix, right dimension (12,
  not shoehorned onto the 16-voice grid). Not confused anymore -- just far out on the priority horizon.
The self-referential idea isn't wrong (my dimension-confusion made it look wrong-ON-THE-MATRIX); it belongs
in its OWN module with a 12-D matrix. That's the correct shape. Whether ever built = a far-future musical
question; IF built, we now know exactly what it is: a meta-CA scattering CA's 12-dim scatter draw.

Cross-ref: MonsoonChangeAlleyV2.hpp:62-67 (the 8-dim scatter draw, -> 12 with q-mix pins), the reality-check
section above (why CA self-scatter is NOT a voice-matrix pin -- dimension mismatch; this gives it its
correct home), the green source-select pin (the +q-mix type that takes 8->12), LAUNCH_INTENT_AND_STORY
(order/chaos-on-itself -- CCA is the literal recursion, a far-future module)." 

## Is the scatter draw really 12-D, or a 2x3x2 PRODUCT of orthogonal axes? (Rodney) -- the latter
Rodney's instinct: "12-D or 3 separate 2d/3d/2d?" Answer: NOT a flat 12-D space -- a STRUCTURED PRODUCT of
three ORTHOGONAL axes: Intra/Inter (2) x {rhythm, melody, q-mix} (3) x domain/codomain (2) = 12.

### Why product, not flat 12-D
The three factors are DIFFERENT QUESTIONS about a scatter, answered independently:
- Intra/Inter = the SCOPE/kind of scatter.
- rhythm/melody/q-mix = WHICH stream-type's topology is scattered.
- domain/codomain = WHICH SIDE of the pin matrix (consuming vs source).
A scatter is characterised by a value on EACH axis independently = the signature of a Cartesian PRODUCT
(2x3x2), not 12 arbitrary dimensions. The "12" is a COUNT of the product, not a claim of one entangled
space.

### As a DRAW: already correct (separate addressable streams)
The 12 are already implemented as SEPARATE addressable Philox streams (8 now, 12 with q-mix pins). That's
right -- it's the same "separate streams for orthogonal decisions" principle as the dice. They're not one
entangled 12-D thing; they're 12 independent streams = the product's cells. The "12-D" was just their
NUMBER, not their entanglement.

### As a CORRELATION target (Change Change Alley, IF ever): per-axis, NOT a flat 12x12
A flat 12x12 correlation matrix would treat all 12 as peers and allow ARBITRARY cross-correlation -- but
most cross-axis cells are MEANINGLESS ("rhythm-intra-domain correlates with q-mix-inter-codomain"?? no
musical meaning). The meaningful correlations are STRUCTURED BY the axes:
- correlate the 3 stream-types (a 3x3), and/or
- correlate the 2 sides (a 2x2), and/or
- correlate Intra/Inter (a 2x2),
- possibly a few meaningful axis-PAIRINGS.
So CCA, if it existed, is a SMALL SET OF PER-AXIS correlations (3x3, 2x2...), NOT a 12x12 meta-matrix. Much
more modest AND more meaningful (each correlation along a semantically coherent axis). This makes CCA less
absurd than "a 12x12 meta-matrix" sounded -- possibly just a couple of small per-axis toggles/matrices.

### Net
Not "12-D" as one space -- a 2x3x2 product of orthogonal axes, already correctly implemented as separate
addressable streams. Preserve the product structure; don't collapse to a flat 12-D. Any correlation (CCA)
is per-axis (small), not flat 12x12. Rodney's instinct to question the 12-D flattening was right: the
structure IS the product.

Cross-ref: the 8->12 dimension section above (the count; now understood as a 2x3x2 product), MonsoonChange
AlleyV2.hpp:62-67 (the streams as product cells -- Intra/Inter x r/m[/q] x dom/cod), the separate-dice
principle (orthogonal decisions -> separate streams = why the product is separate streams not one space),
the CCA section (revised: per-axis small correlations, not a 12x12 matrix)." 

## RESOLVED FORM (Rodney): CCA = 3 small per-axis grids on one side of CA, not a separate 12-D module
Rather than a separate "Change Change Alley" module with a 12x12 matrix, put scatter-correlation as a SMALL
ADDITION on ONE SIDE of the existing CA panel: three tiny grids matching the 2x3x2 product structure:
- two small 2x2 grids (one for domain/codomain, one for Intra/Inter),
- one small 3x3 grid (for rhythm/melody/q-mix stream-types),
- NO OTHER CONTROLS -- purely the three per-axis correlation grids.

### Why this is exactly right
Direct physical expression of the 2x3x2-product insight: the meaningful correlations are PER-AXIS, so you
need three little grids (one per axis), NOT a 12x12 (144-cell) monster. Sizes: 2x2=4, 2x2=4, 3x3=9 = 17
structured cells total, vs 144 flat. So:
- RESPECTS the product structure (one grid per orthogonal axis = the "correlate per-axis, don't flatten"
  conclusion).
- PHYSICALLY SMALL (17 cells across three mini-grids) -> a panel STRIP, not a module.
- "No other controls" keeps it clean -- reads as "correlate the scatter along its three axes", nothing
  else.

### Fixes the last CCA objection
The "separate module / third-order / too heavy" concern was really about the 12x12-matrix / separate-module
framing. As three small per-axis grids on CA's own side panel: not a separate module (part of CA, no
module-count cost); not a 12x12 (no meaningless-cross-term problem, each grid = one meaningful axis); small
enough to be a strip (minor physical cost, and CA's panel is growing anyway). So CCA stops being a
far-future absurd meta-module and becomes "a small three-grid scatter-correlation section on CA's panel" --
much more buildable, much less speculative. The recursion (correlating CA's own scatter) is still what it
does; the FORM is now proportionate to the modest per-axis reality.

### How a cell correlates (reuses CA's pin idiom, one meta-level up)
Each mini-grid correlates along its axis using CA's OWN pin idiom: e.g. in the 3x3 stream-type grid, a pin
at (rhythm-row, melody-col) = "rhythm-scatter follows melody-scatter" (rhythm's reallocation correlates
with melody's). Same pin-follows-source idiom as the main CA matrix, applied to SCATTER STREAMS instead of
VOICES. Not a new interaction -- CA's pin idiom turned on CA's scatter. That IS "Change Change Alley" made
concrete + small: the same pin-correlation gesture, turned on the scatterer.

### Status
MULL on the MUSICAL question (is correlating scatter worth it?), but the IMPLEMENTATION is now small +
clean: three per-axis grids (2x2, 2x2, 3x3) on one side of CA, no other controls, reusing the pin idiom.
Moves CCA from "far-future speculative meta-module" to "a modest optional panel section, if the scatter-
correlation texture proves wanted."

Supersedes the "separate 12-D matrix module" framing (heavier + implied a flat 12x12). Correct form = three
small per-axis grids on CA.

Cross-ref: the 2x3x2-product section above (per-axis correlation -> the three grids), MonsoonChangeAlleyV2
(CA's pin idiom -- reused one meta-level up; the side-panel strip lives here), the CCA/self-reference
sections (order/chaos-on-itself -- now a small buildable strip, not a meta-module)." 

## Scatter-correlation grids: counts + semantics + NOW IN V1 (Rodney)
Decision: the scatter-correlation grids (the "Change Change Alley" strip) are added to CA in V1 -- no
longer a deferred mull. Promoted to V1 scope.

### Counts (one-per-row = each consumer reads exactly one source, like CA's main matrix)
- 3x3 stream-type grid (rhythm/melody/q-mix): each row picks ONE source -> 3^3 = 27 configurations.
- 2x2 domain/codomain: each row picks one -> 2^2 = 4.
- 2x2 Intra/Inter: 2^2 = 4.
Rule: "each of melody/rhythm/q-mix needs ONE source only" (Rodney) = one pin per row, same "each consumer
reads one source" idiom as CA's main voice matrix -> idiom-consistent, and keeps the space tidy (27, not
the unconstrained 2^9 = 512 free-cell alternative).

### Semantics: rows = consumers, cols = sources; DIAGONAL = independent/default
- Rows = the scatter streams needing a source (rhythm/melody/q-mix scatter). Cols = streams that can be
  the source. One pin per row.
- DIAGONAL (rhythm->rhythm, melody->melody, q-mix->q-mix) = the IDENTITY / uncorrelated state: each stream
  reads its own source, scatters independently = the neutral DEFAULT. Off-diagonal pins = cross-
  correlation (rhythm-scatter follows melody-scatter, etc.). So each grid has a natural home state
  (all-diagonal = no scatter-correlation) and a clear meaning for departures.

### Total, but never thought of as one number
27 x 4 x 4 = 432 total across the three axes -- but you NEVER think of it as 432; they're three INDEPENDENT
per-axis choices, each small and legible (27, 4, 4), each with a diagonal default. That's the payoff of
respecting the 2x3x2 product: three small comprehensible grids, not one 432-state (or 12x12) space.

### Confirms buildable
Small, tractable, idiom-consistent (one source per consumer, like main CA), each grid with an obvious
neutral diagonal default. "Change Change Alley as three small grids on CA's side" is genuinely modest and
buildable -- confirmed by the counts. IN V1.

Cross-ref: the RESOLVED FORM section above (three per-axis grids on CA's side = where these counts live),
the 2x3x2-product section (why per-axis grids not a flat matrix), MonsoonChangeAlleyV2 (main matrix's one-
source-per-row idiom, reused here), ROAD_TO_RELEASE (now V1)." 
