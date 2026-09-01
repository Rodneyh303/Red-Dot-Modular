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
