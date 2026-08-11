# BUILD SPEC: dot.modular CV->MPE utility module (for CC)

Build direction for the CV->MPE-out utility. Design rationale + resolution + complexity are in
MICROTONAL_MIDI_MPE_DIRECTION.md; THIS is the build shape. Post-V1, but a self-contained module with
ZERO engine coupling -- can be built independently any time.

## What it is
A utility module: poly microtonal pitch CV + poly gate IN -> MPE MIDI OUT. Splits each voice into
nearest-12-TET note + per-note pitch bend, one MPE member channel per voice, so poly microtonal
patterns play out to a DAW / VST / external MPE synth with the tuning intact.

## Inputs (already available on dot.modular outputs -- Rodney confirmed)
- **Poly pitch CV** (1V/oct, up to 16 channels) -- from a Monsoon/Colonnades/Duo poly pitch output.
- **Poly gate** (up to 16 channels, matching the pitch cable's voices) -- ALREADY output by dot.modular
  alongside the poly CV. So the module needs NO engine changes -- two poly cables in, MIDI out.
- Channel count = pitch cable's getChannels(); voice i = pitch channel i + gate channel i.

## Output
- MIDI via the VCV SDK's `midi::Output` class (public, how all MIDI-out modules send).
- RESOLVED (Rodney found the source): Core CV_MIDI.cpp IS source-available in the GPL Rack repo
  (github.com/VCVRack/Rack/blob/v2/src/core/CV_MIDI.cpp). The "free not open source" library tag refers
  to the PANEL GRAPHICS (proprietary), not the module code. So CV_MIDI.cpp IS a readable reference --
  and a strong one for everything EXCEPT MPE. Key things it confirms:
  - `struct MidiOutput : dsp::MidiGenerator<PORT_MAX_CHANNELS>, midi::Output` -- the SDK provides
    `dsp::MidiGenerator`, which does the CV->MIDI note/gate/velocity bookkeeping. Don't hand-roll it.
  - Note math IDENTICAL to this spec: `note = round(pitchV*12 + 60)`, `gate = getPolyVoltage(c) >= 1.f`,
    `setNoteGate(note, gate, c)` per channel. Poly loop: `setChannels(getChannels()); for c in channels`.
  - **The monophonic-bend limitation is visible in the source**: `setPitchWheel(pw)` is called ONCE,
    outside the per-channel loop, from `PW_INPUT.getVoltage()` (channel 0 only). THIS is exactly the gap
    the MPE module fills -- global bend vs per-voice bend.
- So CV_MIDI.cpp is the STRUCTURAL TEMPLATE for note/gate/velocity/poly plumbing (copy its shape). The
  MPE delta: instead of one global setPitchWheel, put EACH VOICE ON ITS OWN MIDI CHANNEL with per-channel
  bend, + the MPE config handshake. OPEN QUESTION for CC: does `dsp::MidiGenerator` support per-channel
  output MIDI channels / an MPE zone, or does the MPE module need its own channel-routing layer on top
  of `midi::Output` (likely the latter -- MidiGenerator looks single-zone)? Check the SDK's
  dsp/midi.hpp / MidiGenerator before deciding whether to extend it or route channels manually.

## The conversion (per voice, per sample or per control-block)
For each active voice i (0..channels-1):
1. pitchV = inputs[PITCH].getVoltage(i); gateHigh = inputs[GATE].getVoltage(i) >= 1.f (or >0.f).
2. On gate RISING edge (note-on):
   - note = clamp(round(pitchV * 12) + 60, 0, 127)   // 0V = C4 = MIDI 60 (confirm octave convention)
   - centsOffset = (pitchV * 12) - round(pitchV * 12)  // in semitones, range [-0.5, +0.5]
   - bend14 = round(8192 + centsOffset * (8192 / bendRangeSemitones))  // 14-bit, 0..16383, centre 8192
   - assign voice i an MPE member channel (see allocation below)
   - SEND bend FIRST, THEN note-on (so the note starts at pitch, not sliding in):
       pitchBend(memberCh, bend14); noteOn(memberCh, note, velocity)
3. On gate FALLING edge (note-off): noteOff(memberCh, note); free the member channel.
4. (v2 optional) while held, if pitchV changes, re-send pitchBend(memberCh, bend14) for glide.
   v1: latch bend at note-on (simpler; a gliding CV won't glide in MIDI). Make continuous a menu option.

## MPE specifics (the real work -- match the reference implementations)
- **Zone / channels**: MPE Lower Zone convention -- master channel 1, member channels 2..16 (up to 15
  voices). Confirm the exact member-channel set + master-channel role against the GPL inbound MPE
  modules (moDllz MIDIpolyMPE, alexandreleroux/MPE) -- they implement the same spec; match their
  channel/zone handling in reverse.
- **Bend range**: use a NARROW range matched to the +/-50c working span. DEFAULT +/-2 semitones (safe
  headroom over the guaranteed +/-50c from nearest-note; ~0.024c resolution). Expose as a param/menu.
  Nearest-note rounding guarantees |centsOffset| <= 0.5 semitone, so +/-2 never clips.
- **MPE config handshake**: on init / zone-change / bend-range-change, SEND the MPE Configuration
  Message (RPN) that sets: the zone (member channel count) AND the per-note pitch bend range. This is
  the MAKE-OR-BREAK step -- without it the receiver uses its own default range and every note is off by
  a scale factor. Get the exact RPN sequence from the reference modules. Emit:
    - RPN for MPE Config (zone member count) on the master channel,
    - RPN 0 (pitch bend sensitivity) = bendRangeSemitones, on the member channels (or per MPE spec).
- **Channel allocation**: map each active voice to a free member channel; on note-off free it; handle
  voices > available members (steal oldest) and re-trigger. Keep a voice->channel table.
- **Message ordering**: bend before note-on (above). Note-off frees channel AFTER sending note-off.

## Params / UI (minimal)
- Bend range (semitones): default 2, small range (1..12). Menu or knob.
- Velocity source: fixed default (e.g. 100), OR an optional poly velocity CV input (v2).
- MIDI device/port: the Core CV-MIDI output UI pattern.
- (menu) Latch vs continuous bend (v1 latch).
- (menu) MPE zone size if not fixed at 15 members.

## What NOT to do
- Do NOT make it an expander (see MICROTONAL_MIDI_MPE_DIRECTION "considered expander, chose utility"):
  it must work on ANY poly 1V/oct + gate source, not just docked to a Monsoon.
- Do NOT touch the engine, TuningTable, or expander chain -- zero coupling is the point (and zero
  regression surface). The microtonal-ness is already in the CV voltage; the module is tuning-agnostic.
- Do NOT reinvent MIDI device I/O -- reuse the Core CV-MIDI output plumbing.

## Verify
- Poly microtonal CV + gate in -> an MPE monitor / MPE synth shows each voice on its own channel with
  the correct note + bend; the sounding pitch matches the microtonal CV (within the resolution).
- Chord of microtonal notes: each note bends independently (the thing standard MIDI can't do).
- Bend range param change re-sends the MPE config; receiver tracks.
- Test against >=2 real MPE receivers (a DAW's MPE track + a hardware/soft MPE synth) -- the validation
  tail; MPE implementations vary.

## References (read BEFORE writing -- collapses the MPE risk into "match the reference")
Readable references (read BEFORE writing):
- **Core CV_MIDI.cpp** (github.com/VCVRack/Rack/blob/v2/src/core/CV_MIDI.cpp, GPL): the STRUCTURAL
  TEMPLATE -- note/gate/velocity math, poly-channel loop, dsp::MidiGenerator + midi::Output usage,
  dataToJson/fromJson, MidiDisplay, panic menu. Copy this shape; it shows the monophonic-bend gap to
  fix (setPitchWheel outside the loop). ~200 lines, directly applicable.
- **VCV SDK dsp/midi.hpp + midi.hpp**: dsp::MidiGenerator + midi::Output/midi::Message -- exact
  signatures, and whether MidiGenerator can be extended for per-channel/MPE or needs a routing layer.
- **moDllz MIDIpolyMPE (GPL)** + **alexandreleroux/MPE (GPL)**: MPE zone / member-channel / bend-range
  RPN handling (inbound -- mirror the conventions in reverse for outbound).
- **Kilpatrick-Toolbox (GPL-3.0)**: pitch-bend with settable 1..12 semitone range (readable example).
- **MPE spec (MMA)**: member/master channels, RPN 0 (pitch-bend sensitivity), MPE Configuration RPN.

## Note to CC on API
The exact midi::Output / midi::Message method names + signatures (setChannel, setNote, setValue,
sendMessage, the pitch-bend message construction) should be confirmed against the Rack 2 SDK +
Core CV-MIDI source -- the algorithm above is API-independent; wire it to the real calls. Do not
hand-write MIDI message bytes if the SDK provides a midi::Message helper; use the helper.

Cross-ref: MICROTONAL_MIDI_MPE_DIRECTION.md (rationale, resolution, complexity, expander-vs-utility),
the dot.modular poly pitch+gate outputs (the two inputs), Core CV-MIDI + the GPL MPE modules (refs).

## The 15-vs-16 boundary: why members cap at 15, and voice-16 handling

The module ("Keppel") caps at 15 SIMULTANEOUS MPE voices. This is not arbitrary -- it falls out of the
MIDI/MPE channel layout:
- MIDI has 16 channels total.
- An MPE zone needs ONE MASTER channel (zone-wide messages: the MPE config, common controllers) + the
  rest as MEMBER channels (one per voice: that voice's note + pitch bend + pressure).
- Lower Zone (default): master = ch 1, members = ch 2..16 = 15 members. (Upper Zone mirrors: master
  ch 16, members 15..1.)
- So member count = 16 - 1 master = 15.

WHY one channel per voice: pitch bend is PER-CHANNEL in MIDI. Independent per-voice bend (which the
microtonal cents offsets REQUIRE) means each voice needs its own channel. 15 members = 15 voices each
independently bendable. Two voices on one channel would share a bend -- the exact problem MPE solves.

### The off-by-one to handle deliberately: VCV poly = 16, MPE members = 15
VCV poly cables carry up to 16 voices (PORT_MAX_CHANNELS); MPE carries only 15 independently-bent
voices (one channel is the master). So a 16-voice patch has one more voice than the zone has members.
CC must handle this ON PURPOSE, not discover it:
- **Voice 16 policy** (pick one, document it):
  (a) STEAL oldest -- a 16th voice reassigns the oldest member channel (MPE-friendly, usual choice), or
  (b) DROP the 16th voice (simplest), or
  (c) cap input handling at 15 and document "15-voice max".
- **Must NOT** collide the 16th voice onto the master channel (ch 1) or wrap it onto a member already in
  use -- that corrupts the master or double-books a channel. No crash, no misroute.
- Lean: STEAL oldest (or cap at 15 with a clear note). 15-voice microtonal polyphony is already ample.

### Optional: two-zone / configurable
The MPE spec allows splitting the 16 channels into TWO zones (lower + upper), each with fewer members --
that's for running two controllers/instruments at once, NOT relevant here. Keppel uses ONE zone (lower,
15 members) by default. A configurable zone size is a later nicety, not v1.

Cross-ref: the MPE channel-allocation section above (this makes the 15 cap + voice-16 policy explicit),
MPE spec (zones, master/member channels).

## Validation + diagnosis (Rodney's Bitwig test: "bend on only C") -- code verified sound

Rodney captured Keppel -> Bitwig (Monsoon+Sikit microtonal, all 12 semitones microtuned) and saw pitch
bends on only ONE note (C). Investigated the code -- KEPPEL LOOKS CORRECT end to end:
- Per-voice member-channel allocation (allocMember per voice, lower-zone members 2..16). NOT collapsed
  to one channel.
- Per-voice bend math (MpeMath.hpp: noteFor = round(pitchV*12)+60; centsOffsetSemis = the signed
  within-semitone remainder; bend14 maps it around 8192). Different offset per voice -- correct.
- Send order: bend BEFORE note-on, per voice on its own channel. Correct.
- MPE config handshake (sendMpeConfig: RPN 6 member count on master + RPN 0 bend-range on master+members)
  fires on init (needHandshake) and on bend-range change. Correct.
- midiOut.channel = -1 re-asserted every block so the Port doesn't overwrite per-voice channels.
So no Keppel bug found. "Bend on only C" is almost certainly RECEIVER-SIDE.

### The scarcity insight (Rodney): few MPE-GENERATING plugins exist
The ecosystem is receiver-side (MPE controllers -> software), not sender-side. Keppel is the rare
outbound direction, so DAW MPE-INPUT handling is the immature, unpredictable part -- this is the
validation tail the spec warned about, now real. Same behaviour across "all channels into same",
"different track", "same track" (Rodney's CLAP attempts) points at collapse AT/BEFORE the DAW's MIDI
interpretation, not track routing (routing differences would show DIFFERENT behaviour) -- i.e.
receiver-side.

### The three receiver-side suspects (in order)
1. **DAW not in MPE mode** (most likely). Non-MPE collapses all member channels to one -> only one
   note's bend survives = "only C bends". Keppel sends correct MPE; the DAW flattens it.
2. **Bend-range mismatch.** Keppel defaults +/-2 semitones AND sends the RPN -- but if the DAW ignores
   the RPN and uses its own default (often +/-48), bends are scaled ~24x too small -> nearly invisible.
   Match the receiver's per-note bend range to 2 semitones.
3. **Forced MIDI channel** on the VCV plugin output in the DAW. If set to a fixed channel (not "All"),
   it overrides Keppel's per-voice channels and collapses them. Set the VCV-plugin MIDI out to All.

### RECOMMENDED VALIDATION PROTOCOL (do this BEFORE fighting any DAW)
Take the DAW out of the loop entirely -- test Keppel's OUTPUT BYTES directly:
1. **VCV standalone + MIDI loopback**: Keppel -> a virtual/loopback MIDI port -> an MPE-to-CV module in
   the SAME VCV instance -> scope the recovered CV. Loopback is transparent (passes bytes, no
   reinterpretation), so this isolates Keppel's output from any DAW's interpretation.
2. **Receiver = moDllz MIDIpolyMPE in MPE MODE** (explicit toggle; shows per-channel activity). Confirm
   MPE mode ON first (off -> collapse even if Keppel is perfect).
3. **Also watch a MIDI MONITOR on the loopback port** -- the ground truth. If the monitor shows notes on
   channels 2,3,4... each with its own pitch bend, KEPPEL IS CORRECT and every DAW failure is a receiver
   config issue. If it shows one channel, that contradicts the code review -- then it IS Keppel.
Outcome: round-trip CV matches the original microtonal CV per-voice -> Keppel proven correct, move to
DAW config. This is also a permanent REGRESSION test (self-contained in Rack, no external gear).

### DAW notes
- Bitwig: may need a "fake MPE hardware instrument" / MPE-enabled instrument track to make Bitwig treat
  the input as MPE (Rodney's plan). The MPE-enable ritual differs per DAW.
- Live (>=11) and Cubase (>=12) have MPE input but each has its own enable + default bend range -- expect
  the same per-DAW fiddliness. Once loopback proves Keppel, DAW work = "find each DAW's MPE-enable +
  bend-range", a COMPATIBILITY-NOTE deliverable, not a bug hunt.

### Usability suggestion (not a bug): show bend range on the panel
Bend-range agreement is make-or-break and invisible when wrong. Display the bend-range value on the
Keppel panel (it's already a param) so the user can match it in the receiver without guessing. Cheap,
removes the #2 suspect's guesswork.

Cross-ref: src/Keppel.cpp (verified-sound allocation/handshake/send-order), src/dsp/MpeMath.hpp
(verified-sound note/bend math), the gap analysis above (few MPE-generating plugins = receiver-side
ecosystem = the validation tail).
