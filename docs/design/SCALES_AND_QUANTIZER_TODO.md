# Scales to add + quantizer modes -- TODO (own session)

## Scales to add (MonsoonScaleManager.cpp MONSOON_SCALES -- currently 28)
Priority, with the Singapore East/West / demographic-completeness rationale:

1. SLENDRO -- PRIORITY. Completes the GAMELAN PAIR (Pelog is already in; Slendro is the other gamelan
   tuning -- having one without the other is like Major without Minor). Directly strengthens the
   heterophony/gamelan/Singapore anchor. 12-TET approx: commonly {0,2,5,7,9} or {0,2,4,7,9} (real
   Slendro is ~equipentatonic, ~240c steps, NOT 12-TET -- ships as an approximation, same as Pelog
   already does; label honestly).
2. CHINESE PENTATONIC (Gong) -- Singapore's largest cultural group is Chinese, and the set has
   Japanese + Indonesian + Indian but nothing explicitly Chinese. Note: {0,2,4,7,9} = same intervals
   as Pentatonic Major already present -- so this is a NAMED/framing add (the cultural label carries
   the story), not new pitch content. Legit for the demographic-completeness statement; be aware it's
   a relabel not a new sound. (Could add other Chinese modes -- Zhi/Yu etc. -- for actual variety.)
3. A CARNATIC (S. Indian) raga -- Bhairav is N. Indian/Hindustani; a Carnatic scale complements it and
   matches Singapore's Tamil population. Rounds the Indian representation.

Result = a scale set whose STRUCTURE mirrors Singapore's cultural composition (Chinese / Malay-
Indonesian / Indian / Western) -- the scale list itself tells the Singapore story, like the module
names do. Curate: prefer scales that extend the East/West span or complete a pair; resist bloating to
a generic 40+ "scale library" (curation is the pitch, not exhaustiveness).

HONEST BOUND (applies to Pelog, Slendro, and the Japanese/Indian scales): these are 12-TET
approximations of non-12-TET tunings. Correct + standard for a semitone-mask system; the REAL tunings
would need microtonal / Scala / per-note-cents support (see quantizer note below).

## Quantizer modes -- COME BACK TO (Rodney flagged)
Revisit the quantiser/scale-enforcement modes. Open questions to work through in that session:
- Current model: scale MASK gates the READ (getSemitoneWeight returns 0 for out-of-scale when locked);
  non-destructive (faders keep values, dim in UI). That's the LOCK behaviour. What other modes?
- Candidate modes to consider: (a) hard quantise (snap out-of-scale to nearest in-scale) vs current
  (b) probabilistic mask (zero the weight, current). (c) "nudge to scale" -- redistributeWeights() is
  ALREADY retained as a static helper for a possible USER-INVOKED nudge (not auto-applied) -- wire it
  to a menu/gesture? (d) nearest-up / nearest-down / nearest bias for a hard-quantise mode.
- Microtonal / true tunings: the 12-TET-approximation bound above is the real limit. A future
  quantiser mode could support per-note cents / Scala import so Pelog/Slendro are the ACTUAL tunings,
  not approximations -- would make the East/West gamelan claim literal rather than approximate. Big
  feature; note as a direction, not near-term.
- Decide interaction with Shophouse conservation + the transpose escape-hatch (transpose is
  deliberately NON-key-aware; a quantiser mode must not accidentally re-conform transposed output --
  see INTERTROPICAL transpose DECIDED note).

## Microtonal / non-equal tunings -- UI & probability model (design, POST-LIBRARY)
Rodney's question: to accommodate microtonal/Scala, is the main change "a Monsoon with 20-40 note
sliders"? Working it through, that framing is the NARROWER (equal-division) solution and conflates two
separable features:

### Two different features
- EDO widen (19/22/24-TET): octave still divided into N EQUAL steps, N != 12. Engine change =
  parameterise every hardcoded 12 (% 12, /12.f CV, 12-bit mask, 12-fader bank) into a variable EDO.
  This IS the "more sliders + engine change" intuition, and it's correct FOR THIS CASE.
- True Scala tunings (REAL Pelog/Slendro, historical temperaments): NOT more pitches -- FEWER,
  IRREGULARLY spaced (real Pelog ~7 pitches at non-equal cents). Not a wider grid; a decoupling of
  "fader index" from "pitch". Needs a TUNING TABLE (each degree an arbitrary cents/ratio) + .scl/.kbm
  import, CV = table lookup not index/N. Table-lookup is STRICTLY MORE GENERAL (an equal table
  represents any EDO as a special case), so if doing microtonal at all, target TABLE-LOOKUP, not
  wider-faders -- one feature covers both, and makes the gamelan claim LITERAL not approximate.

### The three findings (why it's a different architecture, not a parameterisation)
1. OCTAVE survives as the repeat PERIOD (Pelog/Slendro repeat at 2:1; the octave as CONTAINER is fine,
   the SEMITONE as unit dies). Per-tuning repeat-interval override only needed for non-octave scales
   (Bohlen-Pierce 3:1 etc.) -- .scl's last entry already specifies this. Default octave, override rare.
2. UI becomes a TUNING-DRIVEN, VARIABLE-WIDTH, LABELLED fader bank: one fader per tuning DEGREE (7 for
   Pelog, 5 Slendro, 24 for 24-TET -- not a fixed 20-40), each LABELLED by cents/ratio/degree-name
   (position no longer encodes pitch). Even-spacing + cents labels (practical) vs proportional-to-cents
   spacing (honest, shows irregular intervals) -- lean even+labels, proportional as option. KEY: the
   bank is no longer STATIC PANEL ART -- it must be WIDGET-DRAWN and REFLOW on tuning load. Departure
   from Monsoon's fixed-position faders.
3. PROBABILITY model changes: Option A = one weight per TUNING DEGREE, tuning REPLACES the scale mask
   (tuning defines which pitches exist -> no "out of scale"; the degree list IS the scale). Weights
   become tuning-SCOPED (don't transfer across tunings). This is simpler than the current
   fixed-12-superset + mask, but DIFFERENT model. (Option B = weight per universal fine grid preserves
   cross-tuning weights but needs hundreds of entries -> not faders -> overkill, skip.)

### Reopened decision: SEPARATE VARIANT vs GENERALISE
Earlier lean was "generalise the existing Monsoon (12 -> EDO var), not a new module." Working through
the PROBABILITY model reverses that: microtonal isn't a parameterisation, it's a DIFFERENT MODEL
(fixed-universal-grid+mask  ->  loaded-tuning-defines-everything). A "Monsoon Microtonal" built around
loaded tunings from the start may be CLEANER than bolting a tuning mode onto 12-TET Monsoon. Decide
when actioned.

### INTERCHANGE is the load-bearing change (Rodney's point)
The modular architecture decouples most things, BUT the per-note state that crosses modules bakes in
12: GateState has semiPlayRemain[12] + lastSemitone (12-scoped), and there's a MonsoonInterchangeExpander
carrying inter-module state. Microtonal would require the INTERCHANGE / GateState note representation to
change from semitone-index (0..11) to a tuning-degree-index or a cents/pitch value -- and every consumer
(Lantern piano-roll pitch->row, Changi CV out, Change Alley pitch handling, the semi LED flashers) to
follow. This is the diffuse, cross-cutting surface that makes it POST-LIBRARY and high-regression-risk.
Do NOT attempt before the 2026 library submission; 12-TET approximations of Pelog/Slendro are fine to
ship. Literal tunings = a 2.x update / its own news moment.
