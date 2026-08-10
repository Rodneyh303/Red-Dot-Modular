# Direction: microtonal CV -> MIDI (MPE) utility module (post-V1)

A UTILITY module (not an expander) that converts poly microtonal pitch CV to MIDI, splitting each
voice into nearest-12-TET note + per-note pitch bend. Poly microtonal per-note bend REQUIRES MPE.
Rodney's call: utility, not expander. Post-V1 direction, not a build spec.

## Why MPE is required (not optional)
Standard MIDI pitch bend is PER-CHANNEL -- one bend affects every note on the channel. Poly microtonal
needs each voice to have its OWN bend (voice 1 = +30c, voice 2 = -15c). That is impossible on one MIDI
channel. MPE puts each voice on its OWN channel, giving independent per-note bend. So poly microtonal
-> per-note bend -> MPE, necessarily. (Mono microtonal->MIDI would NOT need MPE: one channel, one bend.
The poly case forces it.)

## The core conversion (Rodney's decomposition, correct)
For each voice's pitch CV:
- **nearest 12-TET note** = round(cv * 12) -> MIDI note number. The played note.
- **cents offset** = the remainder (how far the microtonal pitch is from that semitone) -> pitch-bend.
- Emit note-on on that voice's dedicated MPE member channel, with the bend applied to that channel.

The module measures where the CV voltage sits relative to 12-TET; the microtonal-ness is ALREADY in the
voltage, so it needs NO TuningTable -- just round(cv*12)/12 for the note and the remainder for the bend.
Tuning-agnostic by construction.

## The subtlety that makes or breaks it: PITCH-BEND RANGE
Pitch bend is a 14-bit value spanning +/- some semitone RANGE, and that range is a SETTING. MPE
commonly uses +/-48 semitones per-note (vs +/-2 for normal MIDI). The cents offset only maps to the
correct actual pitch if the RECEIVER's bend range matches what the converter assumes. Get it wrong and
every microtonal note is off by a scaling factor -- it LOOKS right (notes bend) but is quantitatively
wrong: the "compiles cleanly, returns a plausible wrong value" failure mode, in MIDI form.
So the module MUST either:
- assume the MPE standard (+/-48 per-note) and DOCUMENT it, and/or
- expose bend range as a parameter, and/or
- send the MPE Configuration Message / RPN 0 that SETS the receiver's per-note bend range.
Sending the MPE config (so the receiver is told the range) is the robust option. At minimum, document
the assumed range loudly -- this is the #1 thing that will make it "not work" for a user.

## Nearest vs floor (design choice: NEAREST)
- NEAREST 12-TET note + bend: bend goes either direction, max magnitude +/-50 cents. PREFERRED --
  smaller max bend, centered, and if the receiver ignores bend you still get the closest pitch.
- floor-to-semitone-below + always-positive bend (0..+100c): larger max bend, one direction. Rejected.

## Why UTILITY, not expander (Rodney's call, reinforced)
CV->MIDI conversion is GENERIC -- it should work on ANY poly microtonal 1V/oct CV (a Colonnades-fed
Monsoon, a Duo, or even a non-dot.modular source). An expander welds to a specific Monsoon and shares
its state; a translator should not be tied to one source. The module needs only the CV + gate (cables
in), not the shared TuningTable or expander chain. Utility = correct: CV in, MIDI out, standalone.

## Things easy to miss (the module must handle)
- **Gate -> note-on/off timing**: MIDI is event-based; CV is continuous. Watch each voice's gate for
  rising edge (note-on) / falling edge (note-off); sample pitch AT note-on to pick note+bend.
- **Mid-note pitch change (glide)**: MPE supports continuous per-note bend. Options: re-send bend
  continuously (true microtonal glide, more MIDI traffic -- the "correct" MPE behaviour) OR latch at
  note-on (simpler v1, a gliding CV won't glide in MIDI). Lean: latch v1, continuous as an option.
- **Voice -> channel allocation**: MPE uses member channels (2..16 with master ch 1, or the lower-zone
  convention). Map each poly voice to an MPE member channel; follow the MPE spec for master channel +
  zone so receivers understand it. <=15 voices = 1:1.
- **Note re-trigger / stealing**: if voices exceed member channels, or a voice re-triggers, standard
  MPE voice-allocation rules apply.

## v1 scope suggestion
- Poly CV in (pitch + gate), MIDI/MPE out.
- Nearest-note + per-note bend, latch-at-note-on.
- Assumed +/-48 bend range, SEND the MPE config so receivers match; expose range as a param if easy.
- Follow MPE lower-zone convention (master ch 1, members 2..16).
- Later: continuous bend (glide), configurable zone, velocity from a CV input.

## The payoff
Microtonal dot.modular patches become playable INTO external MPE gear / DAWs / MPE synths -- the
microtonal pitch survives the trip to MIDI via per-note bend. Opens the collection outward: author a
maqam scale on Colonnades Duo, sequence it, and send it to an MPE-capable synth with the tuning intact.

## Scope / sequencing
Post-microtonal-V1. Independent utility -- doesn't touch the engine or the expander chain, so it can
land any time after V1 without regression risk to the core. Captured as direction; not a build spec.

Cross-ref: the engine's degreeOf/round(cv*12) 12-TET logic (SequencerEngine.cpp:986 -- same rounding
idea, but this module works on absolute CV, tuning-agnostic); MPE spec (per-note bend, member channels,
RPN 0 bend-range); the poly voice model (SequencerEngine poly voices as the CV source).

## Considered expander, chose utility -- the tradeoff (Rodney asked "would it be easier as an expander")

Weighed honestly, because it's cheap to decide now and annoying to reverse later.

### Where an expander WOULD be easier (real advantages -- all convenience, not capability)
An expander could read the engine's internal voice state DIRECTLY instead of reconstructing it from CV:
- **Note-on/off timing** from the engine's actual gate decisions (clean events) vs edge-detecting a
  continuous CV gate.
- **Voice identity** -- the engine knows which voice is which; a utility treats each poly cable channel
  as a voice.
- **The note+cents split is ALREADY computed inside the engine** (it knows the microtonal pitch AND its
  nearest degree via degreeOf). An expander reads the split directly instead of re-deriving it from the
  summed CV voltage -- the strongest expander argument (get note+cents BEFORE they're flattened to CV).

### Why utility still wins (the advantages are convenience; the reconstruction is EXACT)
The key point: everything the expander reads directly, the utility reconstructs from CV WITHOUT loss.
note = round(cv*12), cents = cv - nearest-semitone -- this is EXACT, not approximate (the voltage IS
the pitch; the note+cents split is exactly recoverable). So the expander's main advantage buys
convenience, not correctness. Against that, utility keeps three things the expander gives up:
1. **Works on ANY source** -- Duo, Colonnades-fed Monsoon, third-party microtonal module, bare
   quantizer, anything emitting poly 1V/oct. An expander only works docked to a dot.modular Monsoon.
   Big reach difference for a MIDI bridge -- people point it at whatever they have.
2. **No engine coupling = no regression risk** -- an expander reads engine internals, riding along with
   engine changes (incl the M1 widening). The utility is downstream of everything, isolated.
3. **Placement freedom** -- expanders must be physically adjacent; a utility sits anywhere and takes a
   cable, which matters since a MIDI converter usually lives at the patch EDGE near the interface, not
   next to the sequencer.

### The middle path (if reconstruction gets painful)
If voice-identity or note-on/off reconstruction turns out genuinely painful, that's the signal the CV
interface isn't carrying enough -- fix that by improving what the Monsoon OUTPUTS (e.g. a clean
per-voice gate, which the poly gate cable already provides), keeping the converter GENERIC, rather than
reaching into internals via an expander.

### The one scenario that would flip it to expander
If glide-accurate MPE (continuously re-sent per-note bend that EXACTLY tracks the engine's microtonal
glide) turns out to matter AND reconstructing it from CV is jittery, the expander's direct read of the
engine's ongoing pitch could be cleaner. But that's a v2 optimisation, not a necessity -- v1 (latch at
note-on, or re-send bend from CV) doesn't need it. Even then it's an optimisation, not a capability gap.

### Decision
UTILITY. The expander saves some reconstruction code but costs universality, isolation, and placement,
and the reconstruction is EXACT -- so the trade is convenience for reach, and reach wins for a bridge
whose whole value is connecting the microtonal world to external gear.
