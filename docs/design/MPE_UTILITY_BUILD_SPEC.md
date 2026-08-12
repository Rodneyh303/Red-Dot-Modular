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

## Reverse-calculation MONITOR output (Rodney) -- internal ground truth for testing

Add a diagnostic poly CV output that RECONSTRUCTS each voice's pitch from the (note, bend14) Keppel
actually assigned -- so you can scope input-CV vs reconstructed-CV and prove Keppel's forward path
correct WITHOUT trusting any external receiver. Especially valuable given DAWs mangle MPE (below).

### Why (and why now more than ever)
Rodney's "bend on only C" is almost certainly receiver-side, and there's now expert corroboration that
DAWs mangle MPE (see the Ahornberg finding). So an EXTERNAL receiver can't be a trusted reference. An
INTERNAL reconstruction makes Keppel self-verifying: input CV vs "what I'm actually sending, decoded
back" CV. If they overlay, Keppel's forward path (note + cents + 14-bit quantise + bend range + channel
assignment) is proven correct and any downstream failure is 100% transmission/DAW.

### What to reconstruct (from the ASSIGNED 14-bit bend, not the float offset)
For each active voice, from the values ACTUALLY sent:
  pitch_recon = (note - 60) / 12  +  ((bend14 - 8192) / 8192) * (bendRangeSemis / 12)
- Uses the quantised bend14 (post 14-bit rounding + clamp) and the actual note -- so it catches
  quantisation/clamp/range errors, not just the float math. This is the most diagnostic form: it
  verifies the ENTIRE forward path as a unit.
- Output as a poly CV, one channel per active voice, matching the input voice layout.
- It will match the input within the bend RESOLUTION (~0.02c at +/-2), NOT bit-identical -- the tiny
  residual IS the 14-bit quantisation error, and seeing it be tiny confirms resolution is fine.

### UI / behaviour
- A dedicated MONITOR poly output (labelled diagnostic, e.g. "MON" / "pitch echo") -- NOT a musical
  signal path. For scoping against the input.
- Reconstructs from the per-voice state Keppel holds AFTER assignment (note, lastBend14, memberCh
  active), so it reflects exactly what went to MIDI.
- Optional: also a "max error" light/readout (max |input - recon| across voices) that lights if the
  residual ever exceeds the expected quantisation bound -- a live self-check.

### Doubles as a regression test
The reconstruction should ALWAYS equal the input within the bend resolution. A standalone test
(test_MpeMath / test_keppel_reverse) can assert: for a sweep of pitches, |pitchV - reconstruct(noteFor,
bend14For)| <= (bendRange / 8192 / 12) + eps. Pure math, no MIDI, fits the test harness. Add to
run_all.sh. This converts "the forward math is correct" into a checked fact (like test_12tet_identity
did for tuning).

### This does NOT make Keppel a receiver
Reconstruct-from-own-state only (interpretation B). Do NOT add a MIDI INPUT / MPE decode to Keppel --
that would duplicate moDllz/Ahornberg MPE-to-CV and bloat the utility. The monitor reconstructs from
Keppel's OWN assigned values, not from received MIDI.

## DAW MPE-preservation finding (Ahornberg, VCV forum) -- test in Cubase/Ardour not Bitwig

Ahornberg (author of a serious LinnStrument MPE-to-CV module) reports, from direct experience:
- **Bitwig Studio: does DATA REDUCTION** on MPE after recording to a MIDI clip.
- **Ableton Live 11: sometimes REARRANGES MIDI CHANNELS + some data reduction** (less than Bitwig; v12
  unknown).
- **Cubase and Ardour: preserve ALL MIDI data** (do a good job).
- He STAYS IN VCV STANDALONE to avoid DAW MPE mangling -- independent corroboration of the
  loopback/standalone validation protocol above.

Implications for Rodney's "bend on only C":
- Bitwig data-reduction is a KNOWN behaviour, not a Keppel bug. "Bend on only C" is consistent with
  Bitwig reducing/collapsing per-channel bend data.
- ACTION: test Keppel in **Ardour (free, open-source) or Cubase** -- they preserve MPE. If Keppel works
  there but not Bitwig, that's DEFINITIVE: Keppel correct, Bitwig mangles. Ships as a compatibility note
  ("for MPE capture use Cubase/Ardour; Bitwig/Live reduce MPE data").
- Ahornberg's MIDIPolyExpression.cpp is a THIRD GPL reference (inbound MPE-to-CV) for channel
  conventions (setChannels per output, per-channel iteration, gate-edge note-on) -- alongside moDllz +
  alexandreleroux.

Cross-ref: src/Keppel.cpp (per-voice state to reconstruct from), src/dsp/MpeMath.hpp (the forward math
to invert), the validation protocol above (loopback), Ahornberg MIDIPolyExpression (github, GPL ref +
the DAW-preservation finding).

## DAW MPE-out compatibility map (corroborated by Fluid Chords, a commercial MPE plugin)

Fluid Chords 2 (Pitch Innovations) is one of the few commercial MPE-GENERATING plugins. Its per-DAW
setup docs are effectively a compatibility map for MPE-out -- written by people who solved this for a
shipping product. It CORROBORATES the Ahornberg findings and Rodney's Bitwig suspicion:

- **Ableton Live**: does NOT support MPE MIDI routing BETWEEN TRACKS. Fluid Chords' only Ableton method
  is the STANDALONE app + a virtual MIDI port (enable "Track & MPE" on the port, set a MIDI track's
  input to it). Confirms Rodney's "Ableton has to record from standalone app." For Keppel-in-VCV this
  means: VCV-as-plugin into an Ableton track likely won't carry MPE; VCV STANDALONE -> virtual MIDI port
  -> Ableton (port MPE-enabled) is the path.
- **Cubase (VST3)**: works with straightforward VST3 MIDI routing (plugin on one track -> instrument
  track's MIDI input). No standalone, no special mode. Matches Ahornberg's "Cubase preserves all data."
  -> Keppel-in-VCV-as-VST3 into Cubase should work via normal routing. CLEAN REFERENCE DAW.
- **Bitwig**: NOT documented by Fluid Chords AT ALL (they cover Logic, Ableton, Cubase, Pro Tools,
  Studio One, FL, Reaper -- 7 DAWs, but not Bitwig). A commercial MPE plugin omitting Bitwig, plus
  Ahornberg's "Bitwig does data reduction," = TWO independent expert sources flagging Bitwig as the weak
  link. Strongly corroborates that Rodney's "bend on only C" is a BITWIG problem, not Keppel.

### Consolidated DAW compatibility (for the Keppel compatibility note / manual)
| DAW      | MPE-out status                                    | Source(s)                         |
|----------|---------------------------------------------------|-----------------------------------|
| Cubase   | Works, normal VST3 MIDI routing                   | Fluid Chords; Ahornberg (preserves)|
| Ardour   | Preserves all MIDI data                           | Ahornberg                         |
| Ableton  | Only via STANDALONE app + virtual MIDI port (MPE) | Fluid Chords; Ahornberg (rearranges)|
| Bitwig   | Undocumented; known data reduction                | Ahornberg; Fluid Chords omission  |

### What this means for Keppel (turns debugging into documentation)
1. Keppel is very likely CORRECT (code verified sound; two experts say Bitwig mangles MPE).
2. The deliverable is a COMPATIBILITY NOTE, not a bug fix: "For MPE capture, use Cubase or Ardour
   (preserve all data). Ableton requires VCV STANDALONE + a virtual MIDI port with Track & MPE enabled.
   Bitwig applies data reduction and may not preserve per-note bend -- not recommended for MPE capture."
3. Validation order: (a) the reverse-calc MONITOR output (internal ground truth); (b) VCV standalone +
   loopback + moDllz MPE mode + MIDI monitor; (c) Cubase or Ardour as the clean external reference.
   Only after those confirm Keppel should any Bitwig-specific behaviour be considered -- and even then
   it's a known DAW limitation, not a Keppel bug.

Cross-ref: the Ahornberg finding above (Bitwig/Live reduce, Cubase/Ardour preserve), the reverse-calc
monitor (internal test), Fluid Chords 2 docs (the per-DAW MPE-out map).

## RELEASE DELIVERABLE: per-DAW microtonal-recording guide + demo patches (Rodney)

Ship with the release manual: a "how to record the microtonal (MPE) outs" section for the big 3 DAWs,
plus demo PATCHES. Rationale: the DAW friction is real, DAW-specific, and NOT obvious (Rodney had the
Cubase record setting right and pitches still didn't show; Bitwig was a knowledge gap not a guessable
config). Documenting it means no one else rediscovers it. This is a DIFFERENTIATOR -- most microtonal
tools don't reach outward at all, let alone ship a DAW-capture guide. It says "we know this is fiddly,
we did the work."

Raw material already in hand:
- **Bitwig**: cracked (Rodney, this session) -- V6 expression overlay shows pitch as semitone-fraction
  note offsets; the data's there, reconcilable to the Sikit Scala degrees. Rodney's first-hand notes =
  the guide. (Note the V6 manual lag.)
- **Ableton**: Fluid Chords' documented path -- VCV STANDALONE -> virtual MIDI port -> enable "Track &
  MPE" on the port in Live's MIDI prefs -> a MIDI track's input set to it -> record -> view under the
  clip's MPE tab. Adapt this recipe (Ableton does NOT do inter-track MPE routing from a plugin).
- **Cubase**: still being cracked (Rodney) -- "record MIDI as note expression" is the capture setting,
  but pitches showing depends on Cubase INTERPRETING the incoming per-note bend as VST Note Expression
  tuning (instrument + routing in note-expression/MPE mode, not just the record toggle). Recipe TBD when
  solved; likely a routing/interpretation config, not Keppel (bytes verified correct via Bitwig).

Demo patches to bundle (patch + matching manual step = fastest install-to-"it works"):
- One patch per DAW-recording scenario (load, follow steps, see microtonal MPE in the DAW).
- Musical demo patches that SHOW THE INTENDED USE (what the manual can't): a Change Alley East/West
  sweep, a maqam modulation via Shophouse Micro, a cross-tuning canon. Let people HEAR what it's for.

Cross-ref: the DAW compatibility map above (Cubase/Ardour preserve, Ableton standalone-only, Bitwig
reduces but viewable), LAUNCH_INTENT_AND_STORY (the release framing), the reverse-calc monitor (to
confirm bytes independent of any DAW when cracking Cubase).

## Cubase 15 diagnosis: bend received (live -2 shows) but not converted to Note Expression (Rodney)

Symptom (Rodney): Cubase 15 key editor showed a LIVE pitch bend figure (~-2) while Rack played, but
would NOT show per-note amounts on the notes the way Bitwig does.

Diagnosis -- this NARROWS it cleanly:
- The live -2 means Cubase IS RECEIVING the pitch bend. The MIDI arrives, bend messages land, Cubase
  displays the live value. So NOT a routing/send failure -- the pipe is open. (Keppel confirmed working
  again.)
- "Won't show amounts ON THE NOTES" means Cubase is treating the incoming bend as CHANNEL pitch bend,
  NOT per-note VST Note Expression. Two different things in Cubase:
  - Channel pitch bend: global bend on the channel, shown as a live value / controller lane, NOT
    attached to notes (= the -2).
  - VST Note Expression (VST-NE) tuning: per-note pitch, shown ON the notes (= what Bitwig showed).
- So Cubase is interpreting the MPE as ordinary channel bend instead of CONVERTING per-channel bends
  into per-note expression. The -2 is the current note's channel bend; it isn't attached to notes
  because Cubase hasn't been told the input is MPE.

The fix is a Cubase INTERPRETATION setting (Cubase-side, not Keppel). Leads to try when cracking it:
1. Declare the MIDI INPUT as MPE / Note Expression source (Cubase 12+ has MPE support -- Studio Setup
   MPE input, or the track's MIDI-input MPE toggle, version-dependent). Without it, channels 2..16 read
   as separate channel bends, not MPE member channels feeding one note-expression stream.
2. "Record MIDI as Note Expression" is the CAPTURE side; the INTERPRETATION (input flagged MPE) is
   separate. Record-on but input-not-MPE = records channel bend, not note expression -- exactly the
   symptom.
3. Receiving instrument supporting VST-NE helps DISPLAY; the channel-bend->note-expression conversion
   is the input-side thing.
4. Bend-range agreement (the clean -2): once converting, confirm Cubase's per-note bend range matches
   Keppel's, or amounts scale wrong. Secondary to getting conversion to happen at all.

For the manual: INCLUDE this -2-shows-but-not-on-notes halfway state as a documented symptom -- it's
the exact confusing state a Cubase user will hit, and "declare the input MPE" is the fix.

Cross-ref: the per-DAW recording guide deliverable above, the DAW compatibility map (Cubase preserves
data -- so once interpreted it should be clean), the reverse-calc monitor (confirm bytes independent of
Cubase).

## Cubase 15 MPE recipe -- CONFIRMED from Steinberg docs (resolves "no MPE in Studio Setup")

Why it was un-findable: Cubase does NOT call this "MPE" and it's NOT a checkbox -- it's a DEVICE you ADD,
filed under "Note Expression", and the make-or-break step is on the TRACK not in Studio Setup. That's
why "nothing about MPE in Studio Setup" and why ticking "record as note expression" alone did nothing.

### The recipe (Cubase 15, from Steinberg's own documentation)
1. **Studio > Studio Setup > Add ("+") > Note Expression Input Device.** (It won't pre-exist for a
   generic virtual/loopback port -- you must add it. Auto-detection only covers some known controllers,
   not a VCV/loopback port.)
2. On that Note Expression Input Device: set **MIDI Input** = the port Keppel/VCV arrives on.
3. In the device's **Horizontal/X** section, activate **"Use for Tuning"** -> this auto-sets the VST
   Note Expression assignment to Tuning (maps incoming per-note pitch bend to note pitch = the thing
   that was missing). This section ALSO has the **pitch range** field = the per-note bend range Rodney
   was hunting for ("there has to be MPE info somewhere e.g. pitchbend range"). SET IT TO MATCH KEPPEL'S
   bend range (e.g. 2 if Keppel is +/-2). Mismatch = amounts scaled wrong.
4. Gliding toggle: "activate for fretless devices that glide seamlessly; deactivate for devices that
   create a new note per key." Keppel creates a new note per gate, so likely OFF -- but Keppel's
   continuous-bend feature blurs this; try both.
5. **On the instrument track Inspector > Input Routing: select the Note Expression Input Device** (NOT
   "All MIDI Inputs" / "Any Input"). *** This is the likely exact bug: *** with input on Any Input,
   Cubase reads raw CHANNEL bends (the live -2) instead of routing through the note-expression device
   that converts them to per-note Tuning. Docs are explicit: "Make sure the Input Group and Channel is
   not set to Any Input."
6. The already-ticked **"Record MIDI as Note Expression"** (Inspector > Note Expression) then captures
   it on the notes.

### Why Rodney's attempt showed -2 but nothing on notes
Track input was (almost certainly) still on Any/All Input, so Cubase saw channel pitch bend (the -2),
never routed it through a Note Expression Input Device (which didn't exist yet / wasn't selected), so
nothing converted to per-note Tuning. Steps 1-2-5 are the fix; step 3 sets the range.

### For the manual (Cubase section)
Document the FULL sequence above, and CALL OUT the two non-obvious traps: (a) it's a "Note Expression
Input Device" you ADD in Studio Setup, not an "MPE" checkbox; (b) the track Input Routing must point at
that device, NOT "All/Any Inputs" -- this is the step that makes the -2-shows-but-not-on-notes symptom
go away. Include the pitch-range = match-Keppel note.

Source: Steinberg Cubase 15 docs -- Note Expression Input Device Page + Recording Notes and Note
Expression Data with MPE Input Devices (steinberg.help, cubase-pro/15.0). Verified current for v15.

Cross-ref: the Cubase diagnosis above (channel-bend-vs-note-expression), the per-DAW recording guide
deliverable, Keppel bend range (set Cubase's device pitch range to match).

## Legato landmine: bend clamp plays a silently mis-tuned note (Rodney) -- post-V1 refinement

### Two separate concerns that meet at the bend-range limit
1. **Cents accuracy over octaves: NOT a problem (reassurance).** Bend is in SEMITONES of pitch, and
   cents-per-semitone is constant at every octave (pitch is logarithmic; a cent = 1/100 semitone at C1
   and C8 alike). 14-bit resolution spreads across the bend RANGE (semitones), not absolute frequency,
   so ~0.024c/step at +/-2 holds EVERYWHERE. Octaves do NOT erode cents precision. That worry is
   unfounded.
2. **The real landmine: the RANGE CLAMP on legato.** Keppel bends RELATIVE to the note latched at
   note-on. In legato (hold gate, new pitch) the note number stays and the BEND carries the whole pitch
   change. If a legato jumps further than the bend range (e.g. an octave = 12 semis with range +/-2),
   the bend CLAMPS at the range and the note plays grossly flat/sharp -- silently, no error, no warning.
   "Ramp to 2 then stop" is this: the pitch wanted more than 2 semitones and the bend couldn't reach.
   A legato interval > bend range = a silently, audibly MIS-TUNED note. The compiles-clean-wrong-value
   failure, in musical form. Currently nothing stops a legato jumping octaves within/beyond the range.

### The principle
A legato must NEVER silently play a mis-tuned (clamped) note. Wrong pitch is worse than a re-attack.

### The fix (lean: B default, A optional)
- **B (default): auto re-articulate on range-exceed.** When a legato would exceed +/-(bend range),
  send note-off/note-on to RE-LATCH the note number at the new pitch -> the bend resets near zero, back
  in range, correct pitch. No new knob -- the threshold IS the bend range (or just inside it). Never
  lets a bend exceed range; re-triggers instead of clamping. A big leap becomes a new note, which is
  what it perceptually IS.
  - Tradeoff to expose maybe: re-articulate = right-pitch but re-attacked (envelope retrigger on some
    receivers); clamp = smooth but wrong-pitch. For microtonal CORRECTNESS, right-pitch wins. Optional
    setting: "legato exceeding range -> re-articulate (correct) vs clamp (smooth but capped)."
- **A (optional refinement): explicit interval/octave-jump limit.** A user cap on legato leap size,
  beyond which force re-articulation. Useful for players who want to bound leaps deliberately, but NOT
  required for correctness (B handles correctness automatically). Don't make the user responsible for
  understanding the legato-vs-bend-range interaction -- that's the fiddly MPE knowledge we keep fighting;
  make Keppel HANDLE it (B), expose the limit (A) only as a deliberate musical control.

### Recommendation
Default B (re-articulate on range-exceed, derived from bend range, no new param) so a clamped wrong
pitch is impossible. Optionally expose the behaviour choice (re-articulate vs clamp) and/or an explicit
interval limit. Post-V1 refinement; not a V1 blocker but a real correctness issue for legato microtonal
lines.

Cross-ref: Keppel bend range (1..12 semis), the held-note continuous-bend code (bend14FromNote clamps
beyond range = where the landmine lives), the resolution section (cents/step by range -- octave-invariant).

## MIDI 2.0: the eventual fix, but gated on VCV Rack -- so allow +/-48 + stay MPE for now (Rodney asked)

Rodney asked if MIDI 2.0 fixes the resolution/range/legato-clamp problems. Answer: YES, largely -- but
it's gated on VCV Rack's MIDI output, not Keppel, so it's not actionable yet.

### What MIDI 2.0 fixes (the whole class of Keppel's pain)
- **Resolution/range tradeoff dissolves**: MIDI 2.0 has 32-bit resolution + per-note control NATIVE
  (vs MPE cramming pitch into MIDI-1.0's 14-bit-across-a-range). Per-note pitch bend at 32-bit = absurd
  precision AND wide range at once -> the +/-2-vs-+/-48 dilemma mostly goes away, and the legato-clamp
  landmine shrinks (huge usable range at full precision).
- **Channel-allocation kludge disappears**: MPE's one-voice-per-channel / 15-member cap / master
  channel / stealing is a MIDI-1.0 workaround. MIDI 2.0 has per-note pitch bend + pressure baked in
  natively -- no channel-allocation workarounds. Keppel's member-channel logic becomes unnecessary in a
  MIDI 2.0 path.
- **Receiver-config friction improves**: Cubase 13+/Nuendo translate MIDI 2.0 CVM -> VST3 note
  expression losslessly; Logic's Step Sequencer edits per-note pitch bend directly. The declare-input-
  MPE / match-bend-range dance is partly a MIDI-1.0/MPE artifact.

### Three caveats (why it's NOT the near-term answer for Keppel)
1. **VCV RACK is the gating factor, not the DAWs.** Keppel emits via VCV midi::Output = MIDI 1.0. Until
   the Rack SDK exposes MIDI 2.0 output (UMP, per-note controllers), Keppel CANNOT emit MIDI 2.0 no
   matter what Cubase 15 can receive. *** CHECK the Rack SDK for MIDI 2.0 / UMP output support before
   assuming a MIDI 2.0 Keppel is even possible -- likely not yet. *** This is the real blocker.
2. **+/-48 is STILL the convention even in the MIDI-2.0 era.** Guides still say "match your pitch bend
   range (usually 48 semitones on both hardware and software)". So for the MPE path Keppel is on now,
   allowing +/-48 is the pragmatic interop choice -- it's the ecosystem default. MIDI 2.0 doesn't remove
   +/-48's near-term relevance.
3. **MPE isn't going away** -- it's the compatibility FLOOR. MPE (ratified 2018, universal) remains the
   lingua franca; MIDI 2.0 devices fall back to MIDI 1.0 unless both ends handshake; there's even an MMA
   MPE PROFILE inside MIDI 2.0. So MPE persists as a profile within MIDI 2.0. Keppel's MPE path stays
   valid indefinitely.

### Decision (confirms the earlier +/-48 discussion)
- **Near term (V1): stay MPE, ALLOW up to +/-48** (default +/-2 for precision, allow 1..48 for receivers
  that expect the +/-48 convention or want glide reach). MIDI 2.0 doesn't help yet (VCV = MIDI 1.0), and
  +/-48 is the ecosystem default, so offering it is correct interop. The earlier conclusion stands:
  allow 48, default narrow, PLUS re-articulation for the legato landmine (range-agnostic correctness).
- **Longer term: MIDI 2.0 is the real fix, gated on VCV Rack.** If/when the Rack SDK exposes MIDI 2.0
  output, a MIDI 2.0 Keppel path would largely dissolve the range/resolution/channel-allocation problems
  (32-bit per-note pitch, no channel kludge, cleaner DAW reception). Genuine future direction; NOT
  actionable until Rack supports it. Watch the Rack SDK changelog for MIDI 2.0 / UMP.

Source: MIDI.org state-of-MIDI-2.0 (Feb 2026), imseankim MIDI 2.0 DAW support (Jan 2026 update),
artistrack MIDI 2.0 guide (bend range still ~48). Verified current early 2026.

Cross-ref: the +/-48 discussion + the legato landmine above (re-articulation is still needed at any
range), Keppel bend range (1..12 now -> extend to 1..48), VCV midi::Output (the MIDI-1.0 gate).

## Human pitch perception + the user-facing manual explanation (Rodney)

### Can humans perceive half a cent? No -- an order of magnitude below threshold.
- Pitch JND (just-noticeable difference) in musical context: ~5-6 cents for most people; ~2-3 cents for
  trained listeners in ideal sustained-comparison conditions.
- Half a cent is ~10x below even a good listener's threshold -- inaudible in any practical setting.
- Honest caveat: MELODIC JND (notes in sequence) is coarser (~5-10c+); the most sensitive channel is
  BEATING of simultaneous sustained tones, where people detect mistuning down to ~1c via the beat RATE.
  But even that bottoms out ~1c; half a cent's beat rate is too slow to register over a normal note.
  So 0.5c is not perceptible in practice, even via beating.

### What this means for the range margins (reassuring)
- +/-2 semitones: ~0.024c/step = ~200x finer than the ~5c musical JND, ~40x finer than the ~1c
  microtonal-demanding threshold.
- +/-48 semitones: ~0.586c/step = still ~10x finer than musical JND, and BELOW the ~1c threshold.
- So EVERY range +/-2..+/-48 resolves pitch BELOW human perception. The range choice is therefore NOT
  an audible-quality tradeoff -- it's purely glide reach + receiver compatibility. Even the coarsest
  (+/-48) is finer than anyone can hear.

### Draft manual explanation (user-facing, no 14-bit math)
> Bend range (advanced): Keppel sends each microtonal note as the nearest ordinary note plus a small
> pitch bend. "Bend range" sets how far that bend can reach. A narrower range (+/-2) gives finer pitch
> steps; a wider range (+/-48) reaches further for big glides and matches what most MPE synths expect by
> default. Both are far finer than the ear can hear -- even the widest setting resolves pitch to under a
> cent, and humans don't reliably perceive differences below about 5 cents. So set the range to match
> your synth (often +/-48) or to cover your largest glide; you won't lose audible tuning accuracy either
> way. The only real limit: a slide bigger than the range stops at the edge, so pick a range at least as
> wide as your biggest glide.

This gives the user: the MECHANISM (nearest note + bend), the TRADEOFF (range = reach + compatibility,
NOT audible quality), and the one practical RULE (range >= biggest glide). Reassures that "resolution"
is a non-issue perceptually. The manual should also cover the per-DAW recording recipes (Bitwig/Cubase/
Ableton) and note the legato re-articulation behaviour in plain terms ("big slides re-trigger the note
so they stay in tune").

Cross-ref: the resolution section (cents/step by range), the +/-48 decision, the legato landmine
(re-articulation), the per-DAW recording guide.

## Clarifying: ONE bend per voice does BOTH microtonal offset AND legato movement (Rodney)

Confusion: "we match microtonal with nearest semitone + per-note bend, but then we ALSO have legato
pitch bend." Resolution: these are NOT two competing bends. There is ONE bend per voice, and it always
equals "current true pitch MINUS the note latched at note-on." That single value carries both:
- **Microtonal offset (static part)**: at note-on the true pitch sits some cents from the nearest
  semitone; that offset IS the initial bend. For a static microtonal note it's the ONLY thing in the
  bend and stays constant.
- **Legato movement (changing part)**: if the pitch CV moves while held (same gate, new pitch), the
  note number is already fixed, so the movement must ALSO be expressed as bend -- added into the same
  value.

So bend(t) = current_pitch(t) - latched_note. Both microtonal and legato are just "distance from the
latched note" -- the same subtraction. Example: note-on 30c above C -> latched note C, bend +30c. Glide
to D+10c while held -> latched note still C, bend = +210c (= D+10c relative to C). The +210c contains
BOTH the microtonal +10c-over-D AND the ~2-semitone legato move -- because both are just distance-from-C.
The code confirms it: bend14FromNote(pitchV, vs.note) computes bend from CURRENT pitch vs LATCHED note --
one function, both jobs, because they're the same subtraction. Static = called once at note-on; legato =
called repeatedly as pitch moves.

Why the landmine is a LEGATO problem specifically: a static microtonal note needs only +/-50c of bend
(nearest-note rounding) -- always safe. A legato glide needs bend = the WHOLE interval moved; if that
exceeds the range, it clamps. Re-articulation fixes it by re-latching the note (bend resets to just the
microtonal offset).

One-line framing (for the manual): Keppel expresses every voice as a fixed note plus a bend, and that
bend is always "how far the current pitch is from that note" -- covering the microtonal offset when
steady, and growing to cover the movement when it glides. One bend, both jobs. Drop "microtonal bend vs
legato bend" as separate -- it's one bend meaning "distance from the latched note", and microtonal-vs-
legato is just whether that distance is holding still or moving.

## Open puzzle (post-holiday): why does Bitwig show a RAMP when our CV STEPS at the slur boundary?
Rodney: Monsoon legato is "hold gate, new pitch" -- the CV STEP-changes at the slur boundary sample
(no slew). So Keppel should send a STEP in bend, not a ramp. Yet Bitwig showed a RAMP. Unresolved.
Leading hypotheses (to test post-holiday):
1. **Bitwig interpolates/draws a large single bend jump as a ramp** in the MPE expression lane (display/
   data-reduction resampling a step into interpolated points). Most likely -- Monsoon emits no slew,
   Keppel adds none, so a visible ramp implies the RECEIVER drew it.
2. **Note-length dependence** (Rodney's own hypothesis): if ramp slope varies with legato note length ->
   a time-based cause (Bitwig interpolating over duration). If slope is length-INDEPENDENT -> fixed
   per-transition. Clean discriminating test: vary legato note length, watch whether slope changes.
3. Confirm with the reverse-calc MONITOR output (does KEPPEL's bend CV step or ramp?) or a MIDI byte
   monitor (one bend message = Keppel stepped, many = Keppel ramped). This is the definitive test:
   if Keppel's own output steps, the ramp is 100% Bitwig's drawing.
Lean: Bitwig is drawing a step as a ramp; Keppel steps. The monitor/byte-monitor confirms in a minute.

### Possibly-relevant Monsoon feature (Rodney): step vs step-legato poly gates
Monsoon outputs two poly gate flavours: STEP gates (legato stripped out -- each step re-triggers) and
STEP-LEGATO gates (gives the within-legato gates). Bearing on Keppel:
- Feeding Keppel the STEP gates (legato stripped) would make every step a fresh note-on -> the note
  re-latches every step -> bend only ever carries the +/-50c microtonal offset, NEVER the legato
  interval -> the legato landmine CANNOT occur (no held-note interval to exceed the range). But you lose
  legato phrasing in the MIDI (every note re-articulates).
- Feeding Keppel the STEP-LEGATO gates preserves legato (held gate across the slur) -> the landmine can
  occur -> needs the re-articulation fix.
- So the two gate outputs are effectively a USER CHOICE between "everything re-articulates, always safe,
  no legato in MIDI" (step gates) and "legato preserved, needs range/re-articulation handling"
  (step-legato gates). Worth documenting as a Keppel usage note: if you don't want legato in the MIDI,
  patch the STEP gates and the range/clamp question disappears. The re-articulation fix is for users who
  WANT legato (step-legato gates) but still want big slurs to stay in tune.
Post-holiday: decide whether Keppel's re-articulation should be automatic, or whether "patch the step
gates instead" is the documented answer for the safe case (probably: offer both -- re-articulation for
those on legato gates, and document the step-gate option as the simple alternative).

Cross-ref: the legato landmine section, the reverse-calc monitor (to resolve the ramp puzzle), Monsoon's
step vs step-legato poly gate outputs, bend14FromNote (current-vs-latched, the one-bend mechanism).

## REFINEMENT (Rodney): step-legato gate = the re-articulation GRID (where to force the new note)

Rodney's sharper point: the step-legato gate isn't just a "safe mode" alternative -- it's a structural
INPUT to the re-articulation logic. It solves the "WHERE do I break a too-long slur" problem.

The problem it solves: when Keppel must force a new note (legato jump exceeds bend range), it needs to
know WHERE to place the re-articulation. Fed only the fully-held legato gate, Keppel sees no internal
structure -> it has to INFER the re-trigger point from pitch-exceeds-range -> a heuristic that might
re-trigger at an awkward sample (mid-phrase, wherever the pitch happened to cross the threshold).

The fix: the step-legato gate output exposes the WITHIN-legato gates -- the individual step boundaries
that were JOINED into the held legato. So Keppel can re-articulate ON THE ACTUAL STEP BOUNDARY, not at
an arbitrary pitch-threshold-crossing sample. The re-trigger lands on a real note division, musically.

Two gate signals, two jobs (use BOTH at once):
- **Held legato gate** (high across the slur): "this is one phrase, bend through it" -- preserves legato.
- **Step-legato gate** (internal step pulses within the phrase): "here are the note boundaries INSIDE
  the phrase" -- the RE-ARTICULATION GRID.
Keppel follows the held gate for legato (bend through small moves), but when a move would exceed range,
SNAPS the forced re-articulation to the next step-legato boundary -> legato phrasing AND correct pitch
AND musically-placed re-triggers.

Even better -- PROACTIVE re-latch (not reactive clamp-avoidance): at each step-legato boundary, Keppel
checks "is the pitch here now more than +/-(range) from where I latched?" -> if so, re-latch AT this
clean boundary. This turns re-articulation from a panic-retrigger-at-the-ceiling into a SCHEDULED
re-latch on note divisions -- exactly how a musician phrases it (slur what fits under the hand,
re-articulate when the leap is too big). The step-legato gate provides the note divisions to decide on.

So the step-legato gate upgrades the re-articulation fix from "guess where to break the slur" to "know
the note boundaries and break there." Post-holiday: design re-articulation to CONSUME the step-legato
gate as the boundary grid (when patched), falling back to threshold-inference only if it isn't. This is
better than either pure gate-choice option alone.

Cross-ref: the legato landmine + re-articulation section, Monsoon step vs step-legato poly gates, the
one-bend mechanism (bend = current pitch - latched note; re-latch resets it to the microtonal offset).

## Worked example: SELECTIVE re-articulation within one legato phrase (Rodney)

The payoff of consuming the step-legato gate: re-articulation is SURGICAL -- it breaks the slur ONLY at
the step that needs it, and legato continues on either side, all under ONE held legato-gate envelope.

Scenario: a multi-step legato phrase (one held legato gate spanning several notes). Early in it, a big
50-SEMITONE jump; after it, several small moves.
- The 50-semitone jump exceeds the bend range -> Keppel RE-ARTICULATES at that step-legato boundary
  (forces a new note, re-latches). It must -- no bend range reaches 50 semitones.
- The small moves AFTER it are within range -> they CONTINUE AS LEGATO BEND, still under the same held
  legato-gate envelope. They are NOT re-articulated just because an earlier step jumped.

So within one held phrase you get a MIX: legato bend -> re-articulation at the big leap -> legato bend
again. The re-latch resets the bend to just the microtonal offset (one-bend mechanism), so the small
moves after the jump have the FULL range available again -- each re-articulation buys back the whole
range for what follows.

How the two gates make this work (a single gate couldn't):
- HELD legato gate = the phrase envelope ("all of this is one legato gesture").
- STEP-legato gate = every internal note boundary (the candidate re-articulation points).
- Keppel walks the phrase: at each step-legato boundary, check "does continuing to bend from my latched
  note stay within range?" YES -> keep bending (legato continues). NO (the 50-semi jump) -> re-latch
  HERE, then resume bending from the new latched note for the subsequent small moves.

Result: correct AND musical at once -- no note ever silently mis-tuned (big jumps re-articulate not
clamp), no phrasing needlessly broken (small moves stay slurred). Slur everywhere it can, break only
where it must -- exactly how a human plays it (slur what fits under the hand, re-attack across a leap
too big). The step-legato gate is what lets Keppel be SELECTIVE.

Cross-ref: the step-legato-gate-as-re-articulation-grid section, the legato landmine, the one-bend
mechanism (re-latch resets bend to the microtonal offset, restoring full range).

## GENERALISATION (Rodney): the two-gate interface is source-agnostic, not Monsoon-specific

### Step gate vs step-legato gate agree EXCEPT within legato
Outside a legato/tie, the two gate signals are IDENTICAL -- every step is its own note with its own
gate, both signals fire per step. They DIVERGE only inside a legato span: the step gate keeps
re-triggering (each underlying step re-gated); the step-legato gate holds high across the span (ties
join them). So feeding Keppel either gives the same result EXCEPT within legato:
- step gate -> every step re-articulates always (no MIDI legato, landmine impossible, no phrasing).
- step-legato gate (held) -> legato preserved, needs range/re-articulation handling.
- They agree everywhere except inside legato spans -- which is exactly where the selective behaviour
  lives. Confirms the selective-re-articulation design is consistent: same skeleton, differing only in
  whether tie-joints are welded (step) or hinged (step-legato).

### The real insight: Keppel's re-articulation needs PHRASING INFO, not Monsoon
Keppel's phrase-aware re-articulation depends on TWO pieces of phrasing information, NOT on Monsoon:
1. PHRASE boundaries -- the held/tie envelope ("these notes are slurred together").
2. NOTE-DIVISION grid -- the underlying step boundaries ("the individual notes inside the slur").
ANY source that provides both can drive it. Rodney's example: a 16-step sequencer from ANOTHER BRAND
with ties could feed Keppel BOTH the underlying sixteenth gates AND the tie-joined gates -- and Keppel
does the same phrase-aware re-articulation, re-latching on the sixteenth boundaries only where a tied
glide exceeds range. Keppel never needs to know the source is a Monsoon (or a 16-step, or anything).

This is the UTILITY-NOT-EXPANDER decision cashing in. The whole point of utility was "works on any poly
source, doesn't weld to Monsoon." The two-gate re-articulation is a GENERAL MPE-conversion capability
any tie-capable sequencer can use -- not a Monsoon feature.

### Interface (generic, source-agnostic)
Keppel inputs:
- Poly pitch CV.
- PHRASE gate (held/tied envelope) -- required for legato.
- NOTE-DIVISION gate (underlying steps) -- OPTIONAL; enables selective phrase-aware re-articulation.
Behaviour:
- Phrase gate only -> basic legato + range handling (re-articulate on threshold inference).
- Both gates -> selective phrase-aware re-articulation (re-latch on real note divisions).
Label the inputs GENERICALLY ("phrase gate" / "note-grid gate"), NOT "Monsoon step / step-legato" --
Monsoon is ONE example source; the interface is generic. Document Monsoon's step (=note-grid) and
step-legato (=phrase) gates as the native pairing, and note third-party tie-capable sequencers work too.

Cross-ref: utility-not-expander decision (works on any poly source), the step-legato-gate-as-grid +
selective-re-articulation sections, the one-bend mechanism.

## CORRECTION / FINAL SHAPE (Rodney): Keppel gets main gate + within-legato gate; ADD the step gate, remove nothing

Earlier sections over-complicated this ("generic phrase/note-grid, source-agnostic", "one gate your
choice"). The correct, final shape:

### Current Keppel inputs (verified in src/Keppel.cpp:24, configInput 66-69) -- KEEP ALL
- PITCH_INPUT  "Poly pitch (1V/oct)"
- GATE_INPUT   "Poly gate"   <- this is the MAIN gate
- ACCENT_INPUT "Poly accent gate"
- VEL_INPUT    "Poly velocity CV"
None of these are removed or changed.

### What to ADD: one new input -- the within-legato (step) gate
- ADD a new input, e.g. STEP_GATE_INPUT / WITHIN_LEGATO_GATE_INPUT (a 5th input). Do NOT replace or
  repurpose the existing GATE_INPUT -- the main gate stays as is; this is purely additive.
- From the Monsoon system: patch the MAIN gate into GATE_INPUT (as now) and the WITHIN-LEGATO gate into
  the new input.

### What each gate does for Keppel
- **Main gate (GATE_INPUT, existing)**: defines notes/phrases -- note-on/off, the legato envelope. As now.
- **Within-legato gate (new input)**: the inner note divisions inside a legato span. Gives Keppel the
  internal boundaries so a FORCED re-articulation (when a jump would exceed the bend range -- e.g. beyond
  the set range up to 48) lands on a REAL inner note division, not an inferred pitch-threshold point.
- Both step and step-legato from Monsoon have the SAME influence on the forced beyond-range
  re-articulation; the within-legato gate just supplies the boundary grid for WHERE the forced
  re-articulation lands within a held legato.

### The within-legato gate ALSO has a general use OUTSIDE Keppel
Independent of Keppel, the within-legato (step-legato) gate fires OTHER events within a legato span --
envelopes, accents, triggers on the inner note divisions that the held main gate hides. It earns its
place as a general Monsoon output on its own musical merits; Keppel is just ONE consumer of it. Both the
step gate and the step-legato gate earn their place in the Monsoon system for general use.

### Net
- Keppel: KEEP pitch/gate/accent/vel; ADD one within-legato gate input. Main gate + within-legato gate.
- Monsoon: step and step-legato gates both stay, both useful generally (step-legato notably for
  within-legato event triggering, outside any Keppel context).
- The forced beyond-range re-articulation behaves the same regardless; the within-legato gate places it
  on real note divisions.

Supersedes the earlier "generic two-gate interface" and "one gate your choice" framings above -- THIS is
the shape.

Cross-ref: src/Keppel.cpp:24 (InputIds -- add one), the legato landmine + selective re-articulation
sections (the within-legato gate is the boundary grid), Monsoon step vs step-legato poly gates.

## REMINDER + finding: accent within legato -- currently CONSTANT (sampled at note-on only)

Rodney wanted to check whether accent is constant within a legato or per-step. VERIFIED in
src/Keppel.cpp (rising-edge block ~189-211):

### Current behaviour: accent is a note-on property, latched, CONSTANT within a legato
- Velocity (from VEL CV or the ACCENT gate) is read ONLY inside `if (rising)` -- at note-on -- and set
  once. The held branch updates ONLY the bend; velocity/accent is never re-read while held.
- A held legato has ONE note-on (gate stays high, no new rising edge until it drops+re-rises), so the
  accent captured at the START of the slur stays fixed for the whole slur.
- Inner notes within the legato (hidden by the held gate) get NO accent of their own -- there's no
  note-on to sample them at; MIDI-wise it's one held note bending.
- Consistent with MIDI/MPE (velocity is a note-on property; can't re-articulate velocity on a held
  note without a new note-on). But musically, accents usually ARE per-note even within a slur -> a gap.

### The design question (decide, don't just leave it)
Within a legato, an accented INNER note should be:
- (a) CONSTANT [current] -- whole slur carries the first note's accent. Simplest; but can't accent an
  inner note.
- (b) FORCE a re-articulation on accented inner notes -- sends a new note-on so the accent lands as real
  velocity, but BREAKS the slur at that note.
- (c) Express inner-note accent as a CONTINUOUS MPE dimension (channel pressure / poly aftertouch) that
  CAN change on a held note -> preserves the slur, accent as pressure not velocity. Most MPE-idiomatic
  for expression-within-a-slur.

### Ties to the within-legato gate
The within-legato gate (the inner note divisions) is exactly what would drive per-inner-note accent:
sample the accent at each within-legato boundary. But because MPE can't change velocity on a held note,
per-inner-note accent needs either (b) re-articulate (accent as velocity, breaks slur) or (c) a
pressure/aftertouch dimension (accent moves under the held note, slur preserved). So "accent legato"
and "re-articulation" both consume the within-legato gate -- worth designing them together.

Post-holiday: decide (a)/(b)/(c). Lean: keep (a) as default (simplest, current), consider (c) as the
expressive option (pressure-based inner accent under a slur) since it's the only one preserving legato
AND per-inner-note accent. (b) only if a hard velocity accent on an inner note is specifically wanted
(accepting the slur break).

Cross-ref: src/Keppel.cpp rising-edge velocity/accent (currently note-on only), the within-legato gate
(the per-inner-note trigger grid), the one-bend/held-note mechanism (why velocity can't change on a held
note).

## BETTER SOLUTION (Rodney): Monsoon emits an accent-within-legato output; let the patch decide

Instead of Keppel deciding how to express inner-note accent in MPE (the (a)/(b)/(c) fork), MONSOON emits
an ACCENT-WITHIN-LEGATO output -- accent sampled PER INNER NOTE even inside a held legato. Parallel to
the within-legato gate: step-legato exposes inner note BOUNDARIES; accent-step-legato exposes inner note
ACCENTS. Available in the patch as a normal signal, route anywhere.

### Why this is better than deciding inside Keppel
- Moves the decision OUT of Keppel and INTO the patch. (a)/(b)/(c) each had a compromise (constant /
  slur-break / pressure-not-velocity). Exposing the signal = "here's the inner accent; you decide."
  More modular-idiomatic: expose the info, let the patch route it.
- Makes accent SYMMETRIC with the gate. Two parallel within-legato outputs now fully describe a slur's
  inner structure:
  - within-legato GATE = inner note DIVISIONS (when inner notes start).
  - within-legato ACCENT = inner note ACCENTS (how hard each inner note is).
- Keppel stays SIMPLE (accent at note-on, as now); expressiveness comes from Monsoon exposing more of
  the phrase's inner structure -- consistent with how it already exposes the within-legato gate.

### The three options become PATCH configurations of one exposed signal
- (a) constant: don't patch the within-legato accent -> current behaviour.
- (b) velocity accent on inner notes: patch within-legato accent + within-legato gate -> Keppel
  re-articulates on accented inner notes so the accent becomes a real note-on velocity. Opt-in via
  PATCHING, not a Keppel mode.
- (c) continuous pressure accent: route the within-legato accent to Keppel's continuous-expression path
  (pressure/aftertouch) -> accent moves under the held note, slur preserved.
So the (a)/(b)/(c) fork collapses: don't make Keppel choose -- expose the signal, the patch chooses.

### Build note: gate vs CV
Decide whether the accent-within-legato output is a GATE (accented/not, two-level, matches the current
ACCENT gate -- simpler, consistent) or a CV (continuous accent amount -- more expressive, pairs with the
(c) pressure route). Lean: mirror whatever the MAIN accent already is, for consistency; but CV opens the
pressure route. Worth a moment since it sets what downstream can do.

Post-holiday: add the accent-within-legato output to Monsoon (parallel to the within-legato gate); leave
Keppel's note-on accent as is; document the (a)/(b)/(c) as patch recipes.

Cross-ref: the accent-within-legato finding above (currently constant), the within-legato gate (the
parallel inner-structure output), Keppel ACCENT_INPUT (note-on accent, unchanged).

## Accent-output FAMILY across Monsoon / Straits / Intertropical (Rodney) -- three flavours

This is a SYSTEM-WIDE output convention, not Monsoon-only: the accent-producing modules -- Monsoon,
Straits, Intertropical -- all get the same accent-output family. A user who learns it on one finds it on
the others. (Parallels the gate side's held-vs-within-legato distinction.)

### Three accent flavours (the conceptual set to lock; panel economy separate)
1. **Held accent (across legato)**: accent held constant for the whole slur -- one value per phrase
   envelope. CORRECTION (Rodney): this HOLDING is the SOURCE module's behaviour (Monsoon / Straits /
   Intertropical drive the accent output), NOT Keppel's. Keppel is only a CONSUMER -- it samples
   whatever accent voltage is present at note-on. So "held across legato" = how Monsoon drives its
   accent output; Keppel merely reads it once per note-on. (Earlier text wrongly called this "Keppel
   behaviour" -- Keppel samples at note-on, but the HOLD is decided upstream by the source module.)
   Use: things that stay constant through a slur.
   NOTE: the exact way Monsoon currently drives accent across a legato was NOT pinned in-container
   (accent output writing found at Intertropical.cpp:168, Monsoon ACCENT_OUTPUT bound in
   MonsoonWidget.cpp:552, but the legato-hold logic not located) -- CHECK this post-holiday; it's the
   crux of the accent-family question (what does the current output actually do across a slur?).
2. **Inner accent LEVEL (sampled at inner steps)**: accent re-sampled per inner note within the legato,
   as a LEVEL/CV that updates at each inner boundary (no pulse -- a value that steps). Use: the accent
   AMOUNT per inner note -> Keppel option (c) pressure/aftertouch route (accent moves under the held
   note).
3. **Inner accent GATES (step-legato accent)**: the inner accents as individual GATE PULSES within the
   legato envelope -- discrete accent triggers, a pulse per accented inner note. Use: RETRIGGER per
   accented inner note (envelope re-fire; Keppel option (b) re-articulation trigger).

Key distinction 2 vs 3: LEVEL vs TRIGGER. (2) = "what's the accent amount at each inner note" (a value
that steps); (3) = "fire an accent gate on each accented inner note" (discrete pulses). Both per-inner-
note; one a level, one a gate. (1) is the held/phrase version.

Maps to three downstream uses: (1) held -> constant-through-slur; (2) inner level -> per-inner accent
AMOUNT (pressure route); (3) inner gates -> per-inner RETRIGGER.

### Panel economy (decide post-holiday, not now)
3 flavours x 3 modules = many jacks. Options:
- Expose the two most useful (likely HELD + INNER-GATES; inner-LEVEL/CV is the specialist pressure
  route) and leave the third as a context-menu option / normalled variant.
- Or ONE mode-switchable output (context menu picks held / inner-level / inner-gates for that jack).
- Or full set on Monsoon (flagship), reduced on Straits/Intertropical.
Lock the CONCEPTUAL set now (held / inner-level / inner-gates); resolve which get dedicated jacks per
module during the panel-refinement phase.

Cross-ref: the accent-within-legato solution above (Monsoon exposes inner accent, patch decides), the
within-legato gate (the gate-side parallel), Keppel options (b) velocity-via-reartic and (c) pressure
(consumers of inner-gates and inner-level respectively).
