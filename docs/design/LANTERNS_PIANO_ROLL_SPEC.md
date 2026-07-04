# "Lanterns" — tabbed Lantern with a piano-roll view (spec, for fresh-session build)

Status: SPEC ONLY, not built. Data pipeline already exists; this is a new DRAW + tab UI over the
same cells[][] store. No engine changes needed.

## Concept
Lantern becomes tabbed (mirroring the Sands displays' tab mechanism): Tab 1 = the current lane grid
(unchanged, already tuned — carets, lead outline, colours, playhead). Tab 2 = a PIANO ROLL of the
same data. Hence "Lanterns" (plural views, one module).

## Piano-roll tab
- ONE overall overlaid view (not per-voice tabs; voice isolation via on/off toggles instead).
- VERTICAL axis = semitone rows, FIXED range driven by the octave window
  (PatternInput.octaveLo / octaveHi — see genPitchLive in PatternEngine.cpp:118). Pitch volts:
  v = oct - 4 + (sem + transpose)/12, clamped ±5V. So rows span [octaveLo .. octaveHi] inclusive
  (octaveHi's full octave), i.e. (octaveHi - octaveLo + 1)*12 semitone rows. Stable, no auto-fit.
  Lantern reads these off the Monsoon it finds via findMonsoonEitherSide (octave window is a
  Monsoon param — confirm the exact param id / accessor at build time).
- HORIZONTAL axis = the 16 steps (same stepW mapping as the grid). A note draws as a horizontal bar
  at its pitch row, spanning cell.lengthSteps. Reuse the playhead line.
- DATA: entirely from cells[voice][step] — pitchV → row, lengthSteps → bar width, voice idx →
  identity. A re-draw of the existing store; recordCell already fills it.

## Three view controls (all small)
1. TAB: Grid <-> Piano roll (Sands-style tab widget, reuse TabButton).
2. COLOUR MODE toggle: "by voice" vs "by role".
   - by VOICE: each voice a distinct hue → reads the polyphony/harmony (the melodic lines weaving).
   - by ROLE: the CURRENT Lantern colour meaning — Single=blue, Legato=teal, Tie=violet → reads
     articulation/phrasing across the whole texture. (In this mode voices lose hue distinction;
     that's the intended tradeoff the toggle exists for.)
3. VOICE VISIBILITY: per-voice on/off, to declutter / isolate lines. Composes with both colour modes.

## Nice orthogonality
- The LEAD marker (amber OUTLINE, Cell.leadsLegato) is a stroke, not a fill — so it stays ON TOP in
  BOTH colour modes; lead intent is always visible regardless of by-voice/by-role.
- Playhead-tracking note-name labels already exist (draw() gutter). Decide whether the roll shows
  per-row pitch labels on the left axis (likely yes — it's a piano roll) instead of the lane labels.

## Build notes / gotchas (learned this session)
- TEXT MUST be drawn in draw(), NOT drawLayer(1): nvgText glyphs do not composite on Rack's light
  layer (bars, being nvgFill, do). The current label fix already lives in draw(); the roll's pitch
  labels must too.
- cells[][] lives on the Lantern MODULE (module->cells), engine state via findMonsoonEitherSide.
- Scale: a few hundred lines across tab UI + roll draw + toggles. Fresh-session job, not a patch.

## Open decisions for next session
- Exact Monsoon param id/accessor for octaveLo/Hi (confirm in Monsoon.hpp param enum).
- Left pitch-axis labels: every semitone, or just C's / octave boundaries (cleaner).
- by-voice hue palette (needs N_VOICES distinct, legible-on-dark hues).
