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
