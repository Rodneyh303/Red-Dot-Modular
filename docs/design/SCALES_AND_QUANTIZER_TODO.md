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

## CORRECTIONS + Lantern pitch-axis + what's actually 12-bound
Correcting the prior section's overstatements about the cross-cutting surface:

- INTERCHANGE is SMALL, not load-bearing. MonsoonInterchangeExpander = CV mod of the 12 note sliders;
  generalising it is bounded (its slider count follows the tuning), no more load-bearing than any
  per-degree control. Prior "interchange is the load-bearing change" was WRONG.
- CHANGE ALLEY has NOTHING 12-TET-specific. It operates on SOURCE INDICES and voice PERMUTATIONS, never
  on pitch values -- it correlates which voice draws from which source, never inspects/transforms the
  pitch. Nothing to change for microtonal. (Consequence of the abstraction level it works at.)
- So the ACTUAL 12-bound surface is narrower than claimed: the CORE (GateState note representation,
  the CV output arithmetic /12, the scale MASK width, the 12-fader bank) + the pitch-axis RENDERERS.
  Not the interchange, not Change Alley.

### Lantern pitch-axis replacement (piano-roll doesn't survive non-12)
The piano-roll works because a piano keyboard IS 12-TET visualised (black/white keys, semitone rows,
octave repeat). In a non-equal tuning that metaphor collapses -- no black/white, unequal rows, "which
key" has no answer. Need a different pitch axis. Options (pitch-axis RENDERERS over the same data):
1. CENTS RULER -- continuous vertical 0..1200c axis, ticks at the loaded tuning's ACTUAL pitches; a
   note sits at true cents height. Tuning-agnostic (any .scl), TRUTHFUL (irregular spacing is VISIBLE),
   degrades to 12-TET (ticks every 100c). The microtonal lingua franca. General fallback.
2. DEGREE LANES -- one row per tuning DEGREE (Pelog 7, Slendro 5, 24-TET 24), labelled by
   cents/ratio/traditional-name. MIRRORS THE FADER BANK (roll + faders share vertical vocabulary ->
   elegant, readable). Tradeoff: hides real interval spacing (equal-height rows for unequal steps).
   Best DAILY view.
3. Both as a TOGGLE + keep KEYBOARD for 12-TET. Consistent with Lantern's existing view-mode toggles
   (Notes/Vel/Prob, grid/roll). Keyboard for 12-TET, cents-ruler/degree-lanes when a tuning is loaded.

KEY PAYOFF: Lantern already stores pitch as pitchV (a VOLTAGE), not a semitone index
(c.pitchV = gs.currentPitchV; roll maps voltage->row). So Lantern is ALREADY pitch-continuous under
the hood -- only the RENDERING (keyboard gutter, semitone row height) assumes 12-TET. => swapping to
cents-ruler / degree-lanes is a DRAW-CODE change, not a rearchitecture. Payoff of the store/engine/
display separation discipline.

RECO: degree-lanes = primary microtonal view (readable, matches faders), cents-ruler = truthful/general
option, keyboard = 12-TET. All three are pitch-axis renderers over pitchV, selected by loaded tuning +
view toggle.

### Transpose x pitch-axis interaction (flag)
Change Alley is clear, but the OUTPUT-STAGE TRANSPOSE (Intertropical, the house-chord fan-out) has a
microtonal design Q: "transpose +7 semitones" lands BETWEEN degrees in a non-12 tuning. Transpose by
cents? degrees? ratio? Whatever it does, a transposed note may not sit on any DEGREE LANE -> renders
awkwardly on view 2 but naturally on the CENTS RULER (view 1). Another vote for keeping the cents ruler
as the truthful fallback even when degree-lanes is the daily view. (Transpose stays non-key-aware per
the INTERTROPICAL DECIDED note; microtonal just changes its UNIT.)

## Module triage for microtonal (Rodney's first-pass verdict -- audit properly when actioned)
Preliminary per-module read (NOT a full code audit -- do that when microtonal is actioned; this is the
map to start from). Confirms the 12-TET surface is SMALL: most modules are pitch-agnostic.

NO CHANGE (pitch-agnostic -- operate on gates/probability/routing/voice-indices, never pitch values):
- All SANDS (shape probability + articulation)
- RAFFLES (fires gates)
- JUNCTION (routing)
- CAUSEWAY, STRAITS, CHANGI (voice/CV transport -- carry the CV as-is, don't interpret pitch)
- CHANGE ALLEY (source indices + voice permutations, never pitch -- confirmed)

SMALL / BOUNDED:
- INTERCHANGE -- modulates the current note sliders; count follows the tuning. Bounded.
- INTERTROPICAL -- restrict the per-output TRANSPOSE knobs to OCTAVE-only in microtonal. An octave is a
  2:1 RATIO, valid/meaningful in ANY tuning -> sidesteps the "transpose lands between degrees" problem
  entirely. Loses microtonal fan-out CHORD building (fifths/thirds), keeps octave-doubling. Clean
  trade, "no big deal" (Rodney). (12-TET mode keeps full +/-24 semitone transpose.)

REAL CHANGE:
- SHOPHOUSE -- currently loads 12-TET scales; becomes the TUNING LOADER (12-TET scale set OR a .scl/
  degree-table tuning). The natural home for tuning selection.
- CORE -- GateState note representation, CV output arithmetic (/12), scale-mask width, the 12-fader
  probability bank.
- LANTERN -- pitch-axis RENDERER swap (keyboard -> cents-ruler / degree-lanes); draw-code only, pitchV
  is already continuous.

=> Microtonal = Shophouse (tuning loader) + core note-repr + Lantern renderer + Intertropical octave-
restrict + Interchange follows. The rest of the 15-16 module collection is untouched. Smaller and more
tractable than the feature "feels" -- the modular abstraction holds at the right level. Still
POST-LIBRARY (core note-repr change = high regression risk), but the SCOPE is now mapped.

## Monsoon Microtonal = SEPARATE MODULE (decided) + Scalar reference (validation)
Clarified: the wider/microtonal Monsoon is ANOTHER module, "Monsoon Microtonal" -- NOT a mode of the
standard Monsoon. WHY it must be separate (from the panel): the current 12 sliders are STATIC PANEL ART,
labelled C/C#/D..., baked at fixed positions for exactly 12. Reflowing to 5 or 24 = a different panel by
definition. So standard Monsoon ships now unchanged (12-TET, static, zero risk to 2026 library);
Monsoon Microtonal is a later module with a WIDGET-DRAWN variable bank.

### VCV Scalar (premium) is the reference + independent validation
Compared Scalar (5-div Slendro and 24-div maqam) against current Monsoon. Scalar confirms our earlier
decisions:
- 24 is the REAL-WORLD CEILING. Scalar -- a serious commercial microtonal quantizer -- caps at 24
  Scala divisions. Our "limit to 24" is industry-sensible, not a shortfall.
- EVEN-SPACING, not proportional-to-cents. Scalar shows the tuning's notes as EVENLY-spaced cells (5
  cells for Slendro, 24 for maqam) filling the same fixed box; it does NOT space cells by cents width.
  This is EXACTLY the call we already made (don't horizontally space sliders by tuning width). A
  premium module made the same choice -> solid ground.
- Explicit readouts: NOTES: 5 / NOTES: 24, TUNING: Unequal, CENTS. Good model for Monsoon Microtonal's
  own tuning readout.

### Monsoon Microtonal UI (settled from the above)
- WIDGET-DRAWN variable fader bank, 5..24 faders = the loaded tuning's NOTE COUNT, EVEN-SPACED across a
  fixed-width region (Scalar-style), each labelled by cents/ratio/degree (NOT spaced by pitch width).
- Panel sized for the 24 MAX; fewer notes = fewer faders even-spread (NOT 24-with-greying -- greying
  phantom slots is wrong for irregular scales; greying stays ONLY for out-of-scale-WITHIN-a-tuning).
- Two mechanisms kept distinct: REFLOW = how many pitches exist (follows tuning); GREY = which existing
  pitches are out of the current scale (as today). Don't conflate.

### Open: does Monsoon Microtonal OWN the tuning, or consume it?
Scalar already parses Scala + quantizes. Monsoon Microtonal likely should NOT reinvent Scala parsing --
option: expose N tuning-driven probability faders and let an UPSTREAM tuning source define what the N
pitches ARE (Scalar via its quantize outs, or a Shophouse-microtonal tuning loader). Then Monsoon
Microtonal stays focused on PROBABILISTIC GENERATION over N degrees; something else owns the tuning.
More modular, more on-brand (composition of focused modules). DECIDE when actioned: self-contained
Scala loader vs consume-upstream-tuning. Leaning consume-upstream.

## SCALES WITHIN TUNINGS (design clarification -- post-library, if a Scala-reading Shophouse is built)
Hypothetical: a Shophouse that reads Scala (.scl) tunings. How do SCALES work WITHIN a TUNING? This is
two levels of structure that 12-TET conflates (because in 12-TET the tuning is fixed + invisible).

### The layering: scale subset-of tuning subset-of octave
- TUNING = the set of available pitches/octave + their exact cents (the SUPERSET, everything you COULD
  play). 12-TET tuning = the 12 chromatic semitones. A .scl = whatever it defines (Pelog ~7, a shruti
  tuning ~22, an EDO = N equal steps).
- SCALE = a SELECTED SUBSET of the tuning's pitches (the mode/raga/key actually in use). "C major" =
  7 of the 12. A Pelog pathet = 5 of the 7 Pelog pitches.
Current 12-TET system = tuning fixed (always 12) + scale selectable (the mask). A Scala Shophouse makes
the TUNING itself selectable, and the scale is a subset WITHIN it.

### Mechanism = the N-bit mask generalisation (same as the 12-TET audit, from the scales angle)
Current scale system = a 12-BIT MASK (MonsoonScaleManager, bits 0-11). Generalise:
- Tuning defines N degrees (N = the loaded .scl's pitch count, <=24).
- Scale = an N-BIT MASK, one bit per tuning degree -- exactly parallel to today's 12-bit mask, N wide.
- "Select/deselect keys" (VCV Scalar's model) = toggling bits in the N-bit mask.
Conceptually NOTHING new: "mask selects scale degrees from the available pitches" already exists; the
available pitches just go from a fixed 12 to a tuning-defined N. The scale-mask concept survives intact.

### What are the "scales" when the tuning isn't 12-TET? (the harder question)
Named 12-TET scales (Major/Dorian/...) DON'T transfer -- "Dorian" is a 12-TET concept, meaningless in
7-pitch Pelog or 22-shruti. Three options:
- A. PURE MANUAL selection (Scalar's model): no named scales for custom tunings; user enables/disables
  degrees by hand. Simple, fully general, loses named convenience. (Scalar is a TUNING tool, not a
  scale library.)
- B. TUNING-SPECIFIC scale libraries: each tuning ships its OWN named scales (Pelog with its pathets,
  a raga tuning with named ragas). Richer + culturally authentic, but requires per-tuning curation +
  domain knowledge.
- C. HYBRID (THE ANSWER): manual by default (A always works) + named-scale presets WHEN the tuning
  provides them (B where curated). Mirrors what we already do: 12-TET = "the 12-TET tuning + a curated
  named-scale library + manual toggle". Generalised = "any tuning + optional curated named scales for
  it + always-available manual toggle". The 12-TET case is just the best-curated instance.

### Shophouse fit (elegant -- ties to "Shophouse becomes the tuning loader")
Shophouse currently has 12 shutters (root/scale selector). Generalised:
- Shophouse loads a TUNING (.scl) -> N degrees.
- Its shutters become N shutters (tuning-width) -- each toggling a degree in/out of the scale.
- Named scales for that tuning (if curated) = presets that set the shutter pattern.
So Shophouse IS the scale-within-tuning selector (Scalar's select/deselect housed in the shophouse
metaphor) -- and the shutters opening/closing = degrees enabled/disabled is a lovely literalisation
(a facade where each shutter is a pitch).

### Two honest HARD PARTS
1. ROOT / TRANSPOSITION in an UNEQUAL tuning: in 12-TET "C major" and "D major" are one mask ROTATED.
   In an unequal tuning you CAN'T rotate the mask -- intervals differ per degree, so "same scale, new
   root" is a different set of cents, not a rotation. Decide: is a scale a ROTATABLE PATTERN (equal
   tunings only) or an ABSOLUTE degree selection (any tuning, but "transpose" means something else)?
   For unequal tunings, ABSOLUTE selection is the honest model. (Scalar works in absolute pitches for
   exactly this reason.)
2. NAMED-SCALE CURATION is real ethnomusicological work: Pelog pathets done RIGHT needs Javanese modal
   theory. Curate few tunings carefully OR ship manual-only (A) for un-curated tunings. Do NOT half-do
   cultural scales -- a wrong pathet is worse than no pathet.

### Summary
Scales-within-tunings = the N-bit-mask generalisation of the existing 12-bit mask, with per-degree
enable/disable (Scalar's model) as the always-available base + optional curated named-scale presets per
tuning, housed in a Scala-reading Shophouse (shutters = tuning-width degree toggles). Concept is a clean
generalisation; hard parts are root-in-unequal-tunings (absolute not rotatable) + curation care.
POST-LIBRARY, with the rest of the microtonal arc.

## CORRECTION re .kbm (keyboard mapping) -- NOT the scale-within-tuning source
Earlier note suggested named-scales-per-tuning might come from a Scala ".kbm companion". That's WRONG.
Clarified (Scala docs + Eurorack practice):
- .kbm = KEYBOARD MAPPING (MIDI-note -> scale-degree: reference note/freq, which keys skipped, octave
  repeat). It's about KEYBOARD PLAYABILITY, not about defining scales-within-tunings or named modes.
- For a CV instrument (no MIDI keyboard in the path) .kbm is largely IRRELEVANT -- our Shophouse MASK
  is the degree selector; we don't need .kbm to do that job. (Confirmed by Eurorack quantizer practice:
  uTune etc. don't use .kbm; the user/module enters the degree mapping directly.)
- .kbm files are also RARE + GENERIC (one linear .kbm serves many same-size .scl files), so they don't
  pair 1:1 with scales anyway. Finding them = download the Scala archive (huygens-fokker.org/scala) and
  `find . -name "*.kbm"` -- but you'll find few, and they're generic, not per-scale-named.

### The RIGHT idiom for us (from Scala's own `mode` command)
Scale-subsetting is idiomatically done WITHIN the .scl, not via .kbm: Scala's `mode` command picks a
subset of degrees and deletes the rest, yielding a single subsetted .scl (instead of a .scl+.kbm pair).
This is explicitly the method "if your synth supports .scl but not .kbm" -- exactly us. So:
- .scl defines the TUNING (all available pitches).
- Scale-subsetting = our MASK (Shophouse shutters) at runtime, OR pre-subsetted .scl files for named
  modes (the `mode`-command approach) shipped as presets.
- NAMED scales within a tuning (Pelog pathets etc.) = OUR OWN CURATION, not harvested from .kbm. Either
  curated masks in-module, or curated pre-subsetted .scl presets. The ethnomusicological-care caveat
  stands: do it right or ship manual-only.
So we consume .scl (tuning) and essentially NEVER need .kbm. Named modes are our curation, delivered as
masks or as subsetted .scl presets.

## REFINEMENT: .scl is role-agnostic + .kbm IS useful for Monsoon Micro's fixed faders

### .scl is BOTH tuning and scale (role-agnostic)
A .scl is just a LIST OF PITCHES in an octave. It doesn't know if it's a "tuning" or a "scale" -- those
are ROLES you assign, not properties of the file. Same format holds 22 shrutis (a "tuning"), 5 Slendro
pitches (a "scale"), or 7 Pelog (either). That's why `mode`-subsetting a .scl yields ANOTHER .scl: you
take the 22-pitch superset .scl and produce a 5-pitch subset .scl -- both are .scl, only the
superset/selection ROLE differs.
Two valid architectures (can coexist):
- A. MASK-based (best for us): load one .scl as tuning; Shophouse mask selects the scale subset live.
- B. TWO-FILE: load a big .scl (tuning) + a small .scl (scale) whose pitches subset the tuning -- the
  scale .scl IS the selection, no mask. Also = how to ship NAMED-SCALE PRESETS (pre-subsetted .scl).
Use A for live selection (our paradigm), B for shipping curated named modes. Both are just .scl.

### .kbm IS the right tool for Monsoon Micro (CORRECTION to "never need .kbm")
Earlier: ".kbm irrelevant, we have no keyboard." TRUE for a pure CV quantizer (no fixed slots). But
Monsoon Micro HAS fixed slots: the 12 or 24 FADERS. The fader bank IS a keyboard -- a fixed array of
playable positions that a variable-pitch-count tuning must be mapped onto. That's EXACTLY the problem
.kbm solves: "map a tuning's N degrees onto a fixed set of slots, skipping some, defining the octave
repeat."
Where .kbm (or an internal equivalent) earns its place on Micro:
- Tuning has FEWER pitches than faders (5-pitch Slendro on Micro-12): WHICH 5 of 12 faders are active,
  which are dead? .kbm = a map with skipped positions. Exactly this.
- Tuning has an idiomatic keyboard layout (22-shruti conventionally mapped to 12): .kbm encodes the
  conventional degree->slot layout so faders match how the tuning is MEANT to sit, not naive first-N.
- Octave-repeat degree isn't the last pitch: .kbm specifies which degree is the formal octave -> how
  the fader bank wraps.
So for Micro: consume .scl (the tuning pitches) AND a .kbm-style mapping (degree -> which fader, with
skips + octave point). Either read actual .kbm files, or implement the same concept internally (a
degree->fader map). The fader bank being FIXED (12/24) is precisely why .kbm becomes relevant here and
NOT in a slot-less CV quantizer.

### Net
- .scl -> pitch list (tuning or scale by role). Consume for both tuning and named-mode presets.
- .kbm -> degree->slot map. SKIP for pure CV quantizing; USE for Monsoon Micro's fixed 12/24 faders
  (the fader bank = the keyboard). This is the exception where .kbm is the correct tool.
