# Fullerton — a jack-fed Lantern for arbitrary gate/CV combinations

Status: DESIGN. No code.

## 0. Naming — Fullerton and Lantern (thought through, not trivia)

The Fullerton Bay Hotel (2010, at Clifford Pier, Fullerton Heritage precinct — NOT the
main 1928 Fullerton Hotel, be precise) has a rooftop bar named LANTERN. The bar takes
its name from Clifford Pier's historic Chinese name, "Red Lantern Pier": red lanterns
hung at the pier to guide seafarers and arriving immigrants to shore — the landing
point for signals from OUTSIDE. The original Fullerton Building separately carried a
working lighthouse on its roof. Both stories are "a light on a Fullerton roof, guiding
ships in."

The pair maps to the architecture truthfully, which is why the names are load-bearing
and not a pun:
1. **Lantern sits atop Fullerton** — in Singapore (the bar on the hotel's roof) and in
   the code (Fullerton = a new capture front-end; the Lantern's RENDER layer is reused
   on top of it). The names encode the software layering.
2. **Red lanterns guided ARRIVING vessels** — external signals landing. Exactly the
   module split: the Lantern lights the engine's own material; Fullerton lights what
   arrives from elsewhere in the rack (arbitrary gate/CV over ordinary jacks).
3. **Red lantern → the little red dot** — the beacon motif lands on brand #d4001a and
   hands the panel its centrepiece: a red lantern beacon at the top of the panel (the
   rooftop), its light-sweep as the playhead. Clifford Pier's arrival semantics also
   ties Fullerton into the crossing/arrival family with Causeway and Changi.

Naming risks considered: precinct place-names are the established dot.modular
convention (Esplanade, Causeway, Changi, Raffles), so Fullerton conforms; the shared
visual language between Lantern and Fullerton is a deliberate implication of the pair,
not an accident users must be warned about.

## 1. Concept

A display module whose inputs are ordinary jacks — poly GATE + poly CV at minimum,
STEP / SLEG / ACCENT optional — rendering the same visual language as the Lantern
(grid + roll views, articulation colours, slur underline, carets) for ANY material the
user routes into it. The routing IS the channel matrix: merges/splits assemble arbitrary
combinations in-rack, Fullerton just displays what arrives. Works on dot.modular's own
outputs, on other sequencers, quantizers, MIDI-to-CV — anything speaking gate/CV.

Why this is now possible: the STEP/SLEG jack work made the articulation model
EXTERNALLY OBSERVABLE. From voltages alone:
- GATE high → sounding; GATE edges → fusion extent
- STEP edge inside a held GATE → sub-note boundary
- CV at a STEP edge: changed → legato (teal); unchanged → tie (violet)
- SLEG high at the edge → slur membership (underline)
- ACCENT high → accent (red top edge)
A tie is distinguishable from a re-struck same pitch: tie = STEP edges while GATE holds;
re-strike = both edge. The vocabulary survives translation to voltages — which is itself
a validation of the output design.

## 2. MEANING — the pairing problem (the core design question)

The display asserts a PAIRING between gate channel i and CV channel i that it cannot
verify against the user's downstream routing. Probe case: 8-channel gate + mono CV.
In a unison patch (one pitch bus, per-voice VCAs) spreading the CV to all lanes is
exactly what the patch does; if the CV was meant for voice 1 alone, spreading paints
7 lanes with pitches nothing plays.

Resolution: adopt Rack's OWN polyphony semantics — `getPolyVoltage`: a MONO signal
spreads to all channels; a POLY signal's missing channels read 0V. Every poly VCA/VCO
downstream will interpret the mono CV the same way, so the display shows what the patch
WILL DO, not a private guess.

The honesty requirement that remains: **the display must SHOW which rule is active.**
- CV channels < gate channels (spread active): spread lanes render pitch DIMMED or
  marked shared, so 8 identical pitches read as "one CV, spread" — not "8 independent
  voices in coincidental unison."
- Lane count = GATE channel count. Gate is the existential signal: no gate channel,
  no lane. All other inputs conform to the gate's count via getPolyVoltage.
- CV channels > gate channels: extras ignored (nothing sounds them). Consider a small
  indicator ("CV 8>4") so the user knows material is unseen.
- STEP/SLEG/ACCENT follow the same conformance rule; disconnected = that layer absent.

## 3. Degradation ladder (what each input adds)

| Connected            | What renders                                                        |
|----------------------|---------------------------------------------------------------------|
| GATE only            | note on/off bars, lengths (mono-colour)                             |
| + CV                 | pitch (roll position / grid label); CV-jump-while-held → slide read |
| + STEP               | sub-note boundaries: tie/legato cells classifiable at STEP edges    |
| + SLEG               | slur underline (exact SLEG mask)                                    |
| + ACCENT             | red top edge                                                        |
Every layer strictly adds; absence never renders falsely — un-classifiable cells fall
back to the plainer read (a held gate without STEP is one long bar, which is TRUE, just
coarser).

## 4. Timebase (the second design question)

The internal Lantern is step-indexed by the engine. Fullerton has no steps. Choice:
- CLOCK input (+ RESET) connected → GRID mode: columns quantized to ticks, 16-step ring,
  the Lantern aesthetic. Mis-bins swung/unclocked material (acceptable: user chose grid
  by patching clock).
- No clock → free-running ROLL mode, scope-style scrolling time, works on anything,
  glides drawn as CV paths (honest for portamento).
The clock jack's PRESENCE selects the paradigm. Reset aligns the ring.

## 5. Ambiguities to render honestly

- **Legato vs portamento**: CV moving while GATE holds. With STEP: sample CV at STEP
  edges, classify discretely. Without STEP: grid mode should NOT claim tie/legato
  (insufficient information — draw plain continuation); roll mode draws the CV path
  as-is (a glide LOOKS like a glide).
- **Sub-threshold CV wiggle** at a STEP edge (analog sources): tie-vs-legato needs a
  pitch-change threshold (e.g. >25 cents = changed). Constant, tunable.
- **Stale CV between gates**: CV during gate-low is meaningless for notes; don't render
  pitch for silent lanes (the faithful Lantern's Inactive rule).

## 6. Implementation sketch (cheap: render reuse)

The Lantern's render layers (grid/roll draw, colours, underline, carets, cell ring) are
reusable as-is. New: a capture front-end replacing engine decisions with edge detection +
CV compare per lane — SchmittTriggers on GATE/STEP/SLEG/ACCENT per channel, CV
sample-and-compare at edges, clock tick counter for grid binning. Cell classification
becomes a small pure function of (gate, step, sleg, accent, cvChanged) — trivially
replica-testable. No engine access at all: Fullerton is a pure observer, placeable
anywhere in the rack (not an expander).

## 7. Open questions

1. Channel-pairing annotation visual: dimmed pitch vs a "spread" glyph vs gutter note.
2. Grid length: fixed 16 vs clock-divided ring length control.
3. Should Fullerton ALSO accept the expander bus when adjacent to Monsoon (faithful
   mode), or stay a pure jack observer? Purity is cleaner; one module, one semantics.
4. Panel: RESOLVED direction in §0 — red lantern beacon at the rooftop position,
   beam/light-sweep as the playhead; lighthouse and red-lantern stories converge.
