# PORTABLE SEQUENCE EXPORT  design + first CC prompt (Rodney-decided)

## PURPOSE
Copy a phrase out of a generative sequencer to the clipboard, to paste into any sequencer that supports the
portable sequence standard (Impromptu PhraseSeq16/32 and Foundry, and others). ONE-WAY: export only. No round trip
back into our modules, so no voice index is written.

## FORMAT (the standard, as read from the Impromptu spec)
    "vcvrack-sequence": { "length": <beats>, "notes": [ { "type": "note", "start": <beats>, "pitch": <volts>, "length": <beats> }, ... ] }
- length: beats, must cover every note. May be longer than the last note.
- start, length: beats. 1 beat = a quarter note.
- pitch: volts (1 V/oct). Use the standard's reference (see OPEN).
- POLYPHONY: simultaneous notes with the same start. No special form. A mono receiver may collapse a chord.
- Standard property only. No prefixed properties, no extra note fields.

## TIMING CONVERSION
- Clock and phase modes only. Gate mode is excluded (no time base).
- A 1/16 step = 0.25 beat (quarter = 1 beat). 16 steps = 4 beats; 32 steps = 8 beats.
- At 24 PPQN a step is exact (6 pulses = 1/16). Start = step_index * 0.25.
- A note's length = its legato span in steps * 0.25 beats.

## WHAT GETS EXPORTED
1. MONSOON, mono by voice: one voice, selected by the user (menu), as a standard mono clip. Default: the voice the
   Macro strip selects.
2. STRAITS, mono by voice: any one voice, chosen from the Straits panel or menu.
3. STRAITS, poly: all 16 voices as simultaneous notes (chords by start).
4. INTERTROPICAL, poly: the 8 poly outputs as simultaneous notes. Output mapping to be confirmed (OPEN).
   Clip length spans the whole loop of scenes (sum of scene durations, in beats).

## THE SNAPSHOT (copy from the previous phrase while playing)
- Copy reads a SNAPSHOT, never the live state.
- At each completed phrase boundary (the existing single phrase-boundary event), capture the phrase's notes into a
  snapshot buffer. Keep ONE snapshot: the last completed pass. Replace it at the next boundary.
- The snapshot is 32 steps x 16 voices at most: small.
- Intertropical: the same, at the loop boundary, over the scenes.
- Copy is always available: it exports the snapshot, not the phrase currently playing.

## STAGING
- STAGE 1  Monsoon mono export, clock mode, selected voice. Snapshot at the phrase boundary. Serialiser written here.
- STAGE 2  Monsoon phase mode: same export, content only (not the trajectory).
- STAGE 3  Straits mono export (any voice) and Straits poly export (all voices), reusing the Stage 1 serialiser.
- STAGE 4  Intertropical poly export per loop, reusing the snapshot pattern at the loop boundary.

## OPEN (confirm before the stage that needs it)
- Pitch reference: is 0 V = C4 in the standard, and does our voltage match? (Stage 1, check the example: 0.1666 = 2 semitones.)
- Intertropical output-to-pitch mapping and gate mapping onto the grid (Stage 4).
- The clipboard API: which Rack call writes the clipboard string (Stage 1).

## CC PROMPT  STAGE 1  Monsoon mono export (clock mode), with the snapshot
Build on this branch.
1. SNAPSHOT. Add a PhraseSnapshot to the Monsoon engine path: on each completed phrase boundary, copy the phrase's
   per-step notes (pitch voltage and gate/legato span) for the selected voice into a single buffer. Keep ONE buffer.
   It is written only at the boundary and read only by export. No effect on playback.
2. SERIALISER. A function that turns the snapshot into the standard JSON object (length, notes), with start =
   step * 0.25 and length in beats. It must use the step and PPQN constants (PULSES_PER_STEP), not literals.
3. MENU. Add "Portable sequence: copy voice N" (N selectable, default the Macro-selected voice) to the Monsoon
   context menu. It writes the serialised JSON to the clipboard.
4. CLOCK ONLY. In gate mode the menu item is disabled (no time base); in phase mode it is disabled until Stage 2.
5. SELECTED VOICE. The chosen voice comes from the Macro send voice (the same selection the sends use), unless the
   user picks another voice in the menu.

## GATES
- Snapshot: a phrase boundary captures exactly one phrase; playback is bit-identical with or without the snapshot.
- Serialiser: a 16-step phrase at 24 PPQN produces 16 notes with start = 0, 0.25, ... 3.75 and length = 4.0 beats.
  A 32-step phrase produces length 8.0.
- Chord: two voices with the same step produce two notes with the same start.
- Copy while playing: copy during phrase N returns phrase N-1's notes, not the live phrase.
- Gate mode: the menu item is disabled.
- Paste into a receiver that reads the standard format plays the same notes (verify with Impromptu PhraseSeq16).
- run_all green on both SDK legs.

## DO NOT
- Write any prefixed property or extra note field (one-way export, standard format only).
- Change playback, the engine's step logic, or the audio.
- Export gate mode.
