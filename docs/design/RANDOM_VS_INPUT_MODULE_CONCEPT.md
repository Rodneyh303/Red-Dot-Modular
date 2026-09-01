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
