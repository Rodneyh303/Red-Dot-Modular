# Effective-distribution VISUALISER (Lantern-style) — per-voice melody + octave distributions
Resolves where to show the F^-1 RESPONSE-CURVE effect (effective note/octave distribution = weights x tilt).
NOT Sands (Sands = probability GENERATION layer; this is the RESPONSE-CURVE layer — keep layers distinct).
NOT under the global faders (per-voice F^-1 means up-to-16 realized distributions; one global-fader strip
can't honestly show them). -> Its OWN surface: a display-only visualiser module, Lantern pattern.

## Decision (Rodney): Option A — separate visualiser (not the big fader-panel rewrite)
- Panel faders KEEP their meaning: RELATIVE heights under a UNIFORM distribution (set relative weight).
  Unchanged. The visualiser shows the TILT's effect; the faders stay "what you set, uniform".
- Lantern pattern: display-only. process() snapshots host state each block (draw() has no engine access);
  draw() renders. Snapshots: weight[] (from Monsoon/Colonnades/Duo) + per-voice F^-1 tilt (from Monsoon) ->
  draws effective distribution = weight x tilt, normalized, per voice. Live-modulated (breathes with slew/
  spread) since it reads realized values each block.
- Opt-in, non-disruptive (no core-panel change, no height problem, no fader rewrite). A SHOWABLE flagship:
  "watch your per-voice melody + octave distributions reshaped by the response curves, modulated live by the
  correlation" — a window into the correlation+response machinery; demonstrates the instrument's depth.
- (Option B — graphical fader panel + per-voice views on the core module, effective bar under each fader —
  is the bigger, LATER, mature-UI project. Defer. Don't rewrite the core panel now.)

## Serves Monsoon, Colonnades, Duo — attach to ONE
Like the tuning expanders (one Micro-per-Monsoon), the visualiser attaches to ONE host and reads its degree
count: Monsoon (12-TET), Colonnades (Micro-12), or Colonnades Duo (Micro-24). So the melody-bar count is
HOST-DRIVEN: 12 (Monsoon/Colonnades) or 24 (Duo). (Same host-snapshot approach Lantern already uses for the
piano-roll N-row grid.)

## Layout (based on Sands, transposed voice<->lane)
Sands has ROWS of LANES; this has ROWS of VOICES.
- One ROW per voice. Vertically SCROLLABLE (Sands-based); ~8 rows visible, scroll to the rest (up to 16).
- Each row = two adjacent BAR GROUPS:
  - MELODY group: 12-24 bars (host degree count) = the effective per-degree note distribution for that voice.
  - OCTAVE group: ~8 bars (octave range; size to what's needed) = the effective octave distribution for that
    voice. Adjacent to the melody group, same row.
- So a row reads: [voice N] [ ...12-24 melody bars... ] [ ...~8 octave bars... ], live.
- Bars = one-lane-of-Sands style (reuse the Sands bar renderer), effective (weight x tilt, normalized).

## Why this is right
- Correct layer home (own module = the response-curve layer, isolated from generation/Sands + global
  controls/faders).
- Solves per-voice (rows of voices, scrollable) AND the degree-count variation (host-driven 12/24) AND
  octave (second adjacent group) in one coherent Sands-derived layout.
- Faders keep their simple uniform-relative-weight meaning; all the tilt/per-voice complexity lives in the
  visualiser where there's room.
- Flagship-demo value: 8+ voices' melody+octave distributions breathing with the correlation.

## Open details (for build)
- Octave bar count: size to the actual octave range (lo..hi); "~8 or whatever we need".
- Voice-row scroll + which voices shown (all active? up to 16?); default 8 visible.
- Pre/post or realized-only? (Realized = weight x tilt. Could toggle to show weight-only (uniform) vs
  realized, = the pre/post idea, per row.)
- Reuse Sands bar renderer + the host-snapshot pattern from Lantern.
