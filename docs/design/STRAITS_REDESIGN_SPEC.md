# Straits East redesign — SPEC (settled, ready to build)

Supersedes the concept note STRAITS_EASTWEST_REDESIGN_NOTE.md (which was idea capture).
Scope corrected: this is about the BASE poly expander's per-voice REST + ACCENT probability
controls and its output/CV I/O. It is NOT the SandsTopology visual-lane-ownership system (that is
separate and already merged) — do not conflate.

## What this module is
The Straits East base poly expander carries the user's per-poly-voice REST and ACCENT controls —
the knobs that set each poly voice's reaction to the probability rolls (its rest probability and
accent probability) — plus the poly output cables. West retires.

## Settled decisions
1. West RETIRES. One East module carries everything (knob grid + poly-cable outs). West returns
   only if faceplate HP overflows (exception, not plan).
2. Poly output = 16-channel poly CABLES (gate / accent / CV), replacing the 21 individual
   POLY_*_OUT_1..7 jacks. Channel 1 = MONO voice ALWAYS; ch2..16 = poly voices 2..16.
3. Mono stays on the PARENT Monsoon (jack + CV). NO separate mono expander.
4. REST + ACCENT per-voice level knobs presented as a 4-col x 8-row grid: 2 cols REST + 2 cols
   ACCENT = 32 knobs, covering 16 voices (voice 1/mono included). Slightly-smaller Monsoon-style
   knobs. The knobs map to the EXISTING POLY_REST_PARAM_1..16 / POLY_ACCENT_PARAM_1..16 enums
   (already defined to 16 in Monsoon.hpp — "Phase 4" voices 9-16). No new engine plumbing for the
   base levels; this is a PANEL + I/O redesign.
5. CV MODULATION inputs (the per-voice REST/ACCENT mod att + CV in, currently on East) MOVE to a
   dedicated CV expander. Addressing: per-voice via a 16ch poly CV in (ch1=mono, matches the outs).
   A broadcast-to-all is just patching a 1ch cable (Rack auto-copies), so per-voice subsumes it.
6. BACKWARD COMPAT: clean break. New module slug; deprecate old East/West. No migration (pre-release,
   own modules only).
7. Poly voice COUNT already lives as a context menu on Monsoon (engine.numPolyVoices). The expander
   just READS it. No new count param/menu on the expander. Poly cables are 16ch wide; voices beyond
   the count output gate-low / 0V.

## Resulting module shape (transit family — per MODULE_NAMING_AND_ROADMAP.md)
Three modules on the poly path:
- **Straits** (base): 4x8 knob grid = 16 REST + 16 ACCENT level knobs (voice 1 = mono included);
  poly OUT cables (16ch): gate, accent-gate, CV — ch1 = mono duplicate, ch2..16 = poly. Reads
  engine.numPolyVoices from parent. "The channel the voices run through."
- **Causeway** (poly CV): modulates poly REST + ACCENT — per-voice via 16ch poly CV in (ch1=mono,
  matches outs) + attenuation. The mod att+CV-in that used to sit on East. Revived name, new role.
  "The link across = modulation across the voices."
- **Changi** (output): per-voice MONO outs — breaks the poly cable out into INDIVIDUAL jacks.
  "Departures = per-voice outs." (This is the per-channel output expander, distinct from Straits'
  poly-cable outs.)
- **West:** retired. **Mono:** unchanged, on parent Monsoon.

## Build blast radius
- New East module (panel 4x8 grid + 3 poly out jacks). Params map to existing POLY_REST_PARAM_*/
  POLY_ACCENT_PARAM_* (1..16). 
- MonsoonExpanderManager: drop the polyOutCap = straitWest?15:7 East/West split; poly voices now
  bounded only by engine.numPolyVoices; write the 16ch poly cables (ch1=mono) instead of 21 jacks.
- New CV expander module for the mod inputs.
- Deprecate old MonsoonStraitsEastExpander / MonsoonStraitWestExpander (move to src/deprecated,
  drop from plugin.json, or new slug alongside).
- Panel art: new East faceplate (Causeway name is now free for this per the rename).

## Open detail to confirm during build
- Exact knob grid geometry / HP (32 slightly-smaller knobs + 3 poly jacks + logo).
- Whether the new base module reuses the "Straits East" name or takes the freed "Causeway" name.
- CV expander panel + its poly-CV-in addressing wiring.
