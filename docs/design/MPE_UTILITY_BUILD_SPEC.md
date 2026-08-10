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
