# Quantiser modes = sequencer modes for external note CV (Rodney) -- the unification

Reframes the quantiser modes (was Vermona-style C/D) so they MIRROR the sequencer modes: each quantiser
mode is a sequencer mode, but reading EXTERNAL note CV instead of generating pitch internally. Post-V1
direction; captured from a design discussion (Rodney on commute, laptop staying home for the trip).

## Vermona original (what we're diverging from)
- C (Quantizer 1): quantises external CV (CV IN 2, 0-5V) on QUARTER NOTES (clock). Faders = scale;
  higher fader = wider quantisation range. FIXED quarter-note grid.
- D (Quantizer 2): same but sampled while external GATE IN 2 is high.

## The change: C driven by INTERNALLY GENERATED gates (not fixed quarter notes)
Make C like D, but driven by the engine's OWN generated gates instead of external Gate 2. The engine
already generates rhythms (probability-driven, any division/pattern), so C quantises the external CV at
WHATEVER RHYTHM THE ENGINE GENERATES -- 1/4, 1/8, 1/16, or any probabilistic pattern. Replaces Vermona's
fixed quarter-note grid with our rhythm engine as the sampling clock. Strictly more powerful, costs
nothing new (rhythm generation already exists).

## The symmetry (quantiser modes mirror sequencer modes)
| Sequencer | timing source            | Quantiser | same source, quantising external CV |
| A         | clock                    | (old C)   | clock / quarter notes               |
| B         | internal generated gate  | NEW C     | internally generated gates          |
| (ext)     | external gate            | D         | external Gate 2 (unchanged)         |
| E         | phase                    | NEW F     | phase (follows Mode E gates)        |
So: new C = the quantiser's Mode B (generated gates); F = the quantiser's Mode E (phase); D stays
external-gate. The quantiser side becomes the mirror of the sequencer side. Mode F completes the
symmetry (the prerelease doc already flagged the quantiser was missing the phase equivalent).

## The deep insight: timing engine + swappable pitch source
The module has always been a TIMING/GATING/PHRASING engine with a PITCH-GENERATION stage on top.
Quantiser modes just REPLACE the pitch-generation stage with pitch-from-external-CV, keeping everything
else. That's why "everything else applies as is" -- the timing/gate/legato/accent machinery is
pitch-source-agnostic (it decides WHEN/WHETHER notes happen, not WHAT pitch). Quantiser mode = keep the
timing engine, swap the pitch source.

## Consequences (Rodney)
1. **Poly quantise, mono algorithm.** quantize(vIn) is per-voltage already; iterate it across poly
   channels. The mono weighted radius-gated snap (SequencerEngine::quantize) applies unchanged per
   voice. Poly is free.
2. **Same legato rules, based on the quantiser note CV.** Legato/tie logic (hold across / re-articulate)
   keys off the EXTERNAL CV's note changes instead of generated notes. Same rules; the "note" is now the
   quantised external CV. Generated gates (new-C) provide the WHEN; external CV provides the WHAT.
   Legato in quantiser mode = incoming CV held across generated gate spans, re-articulated on generated
   gate onsets. Ties straight to the Keppel step/step-legato gate structure.
   - "Note changed?" = "quantised DEGREE changed" -> reuse degreeOf. Confirm that's the
     re-articulate-vs-hold trigger in quantiser mode.
3. **Sands melody + octave IGNORED in quantiser modes.** Melody draw + octave randomisation are
   PITCH-generation; pitch now comes from CV, so bypass them. Keep the rhythm/gate/legato/accent
   generation, drop pitch generation.
   - CAUTION (codebase risk-shape "compiles, plausible wrong value"): BYPASS melody/octave, don't just
     leave them computed-and-discarded -- an ignored-but-wired melody draw could perturb RNG state or
     lane position. Gate it out, don't compute-and-ignore.
4. **Everything else applies more or less as is** -- rest, accent, direction, lock, dice-scrub, the lane
   model all operate on WHEN/WHETHER not WHAT pitch, so they carry over. Only the pitch stage is swapped.

## Net
Quantiser modes end up like sequencer modes but for external note CV: new C = generated-gate-driven
(any rhythm), D = external-gate (unchanged), F = phase-driven (new). Poly, same legato on the CV's
degrees, Sands melody/octave bypassed, rest/accent/etc unchanged. The two halves of the module become
ONE timing engine with two pitch sources (internal draw vs external CV).

Cross-ref: MODES_C_D_QUANTIZER_PRERELEASE.md (the C=clock/D=gate starting point + the missing-phase-mode
observation), SequencerEngine::quantize (the mono algorithm), the legato/tie model + Keppel step/step-
legato gates (same phrasing structure), degreeOf (note-change detection).

## Per-module behaviour, poly-CV-in placement, two product shapes, mode-lettering refactor (Rodney)

### Per-module behaviour in quantiser mode
- **Monsoon & Change Alley**: melody dice DISABLED/IGNORED; only RHYTHM dice shown in quantiser mode.
  UI-level expression of "keep the timing engine, drop pitch generation" -- hide melody dice so the
  panel reflects that pitch now comes from CV. Change Alley shows only rhythm dice (its correlation
  transforms act as a rhythm/texture axis here, not pitch). Quantiser mode thus has a visibly simpler
  face.
- **Straits**: hosts the POLY CV IN (likely -- see placement below).
- **Intertropical, Lantern, Sikit, Colonnades, Duo**: nothing to change. Lantern = display; the
  microtonal trio (Sikit/Colonnades/Duo) are tuning authorities the quantiser READS (the target scale
  to snap to) -- unchanged whether pitch is generated or quantised-from-CV.

### Poly CV in -- placement (the one open interface decision)
"Likely Straits." Real spec: POLY note CV in (up to 16ch), routed to the ENGINE's quantise stage
(Monsoon core), physically on Straits (the input expander). Considerations:
- The jack lives on Straits but routes into the SHARED engine -- confirm Straits has the expander
  bandwidth to carry poly CV to the core.
- Conceptually the CV-in belongs to the Monsoon quantiser MODE; Straits provides the physical jack.
  Keep the routing clear (Monsoon-mode feature, Straits-exposed) so it isn't confused.

### Two product shapes (two instruments, one engine, by which expander fronts the input)
1. **Rhythmic poly quantiser (Straits-fronted)**: external poly CV -> quantise to scale (12-TET or
   microtonal via Sikit/Colonnades/Duo) -> gated/phrased by the engine's GENERATED rhythm -> out. A
   quantiser that also imposes generated rhythm + phrasing (most quantisers are static quantise-on-gate;
   this one adds rhythm). "Optionally microtonal" comes free from the tuning authorities.
2. **Arranging quantiser (Intertropical-fronted)**: external CV routed/sequenced through Intertropical's
   ARRANGER / SEQUENTIAL SWITCH before/around quantising -> a quantiser that ARRANGES (sequences between
   CV sources or reorders) as well as snaps. A different instrument again.
Same engine yields both -- distinguished by which expander fronts the input. The ecosystem philosophy
(one engine, many faces) applied to the quantiser side.

### Mode-lettering refactor: A/B/C sequencer, D/E/F quantiser
Eventually renumber so the lettering makes the unification visible:
- **A, B, C = SEQUENCER** modes (internal pitch): clock / generated-gate / phase.
- **D, E, F = QUANTISER** modes (external CV): clock-or-generated-gate / external-gate / phase.
- The two triples MIRROR by timing source (A<->D, B<->E, C<->F). Clean: three seq + three quant, the
  "quantiser = sequencer for external CV" unification visible in the letters.

CAUTION (the migration): this RENUMBERS existing modes -> touches saved patches + DAW automation mapped
to the mode param. A patch saved in today's "Mode C" (a quantiser clock mode) would break/silently
change if C becomes a SEQUENCER mode. So the refactor MUST ship with a VERSION-BUMPED PATCH MIGRATION:
old-mode-index -> new-mode-index remap on patch load, so existing patches map correctly. Same discipline
as the .dmtune v1->v2 migration. "Compiles clean, plays wrong" in patch-compatibility form -- don't
renumber without the migration.

Cross-ref: the unification above, MODES_C_D_QUANTIZER_PRERELEASE.md, the microtonal tuning authorities
(Sikit/Colonnades/Duo as the snap target), Straits/Intertropical as input-fronting expanders, the
.dmtune v1->v2 migration (the pattern for the mode-renumber migration).

## Positioning: the quantiser is an INTEROPERABILITY LAYER, not a "me too" quantiser (Rodney)

Static gate quantise is a SUBSET of the offering. The real thing: quantiser mode takes CV from ANY
other Rack CV sequencer and re-expresses it -- new rhythms, scales, tunings, arrangements, mono or poly.

### Why this matters (the anti-"me too")
VCV has thousands of modules; the long tail is derivative (another VCO/filter/basic quantiser). A static
gate quantiser is the fiftieth-time thing. This is NOT that: it's a TRANSFORMATION STAGE you drop AFTER
anything. Patch any sequencer's pitch CV (Marbles, Turing Machine, Bloom, anything) into quantiser mode
-> it gets OUR rhythm engine, microtonal tunings, arrangement logic, poly handling grafted onto their
existing pattern. You AUGMENT their sequencer, not replace it.
- Most modules ask you to REPLACE something; this asks you to ADD to what you have -- easier adoption,
  stronger interoperability story.
- dot.modular becomes not just a self-contained ecosystem but a set of PROCESSING STAGES the whole Rack
  world can route through. The correlation engine, microtonal tunings, phrasing, arrangement -- all
  available to ANY CV source.
- The microtonal angle makes it near-unique: arbitrary 12-TET CV in -> microtonal re-tuning + rhythm +
  arrangement, poly -> MPE out. That specific chain barely exists. Not another quantiser in a crowded
  field -- a microtonal-arranging-interoperability bridge in an almost empty one.

### Launch relevance
"Drop it after ANY Rack sequencer to re-rhythm, re-tune, re-arrange it" is a HEADLINE capability, not a
footnote. Adds Rack interoperability big time. Frame it as such at launch.

## Sample content: .dmtune JINS + 24-degree .scl MAQAMS (Rodney) -- demonstrates the format's whole point

Plan: generate .dmtune jins to go with 24-degree .scl maqams, like Ableton's tuning website offers.

### Why this is more than sample content -- it demonstrates the format hierarchy
Maqam structure maps EXACTLY onto the format hierarchy:
- 24-degree .scl = the full MAQAM tuning (the pitch set).
- .dmtune JINS = a mask over that tuning selecting a jins (a subset of the maqam's degrees), AND
  transposable via the tonic work (ajnas relocate).
- Shophouse Micro modulating between jins-.dmtunes = MAQAM MODULATION (intiqal between ajnas) -- central
  to how maqam music actually works.
So shipping "24-degree .scl maqams + .dmtune jins within them" DEMONSTRATES the format's whole point:
tuning = maqam, scale-masks = ajnas, modulation = the real performance dynamic. Makes the Change Alley
origin ("microtonal let it pronounce a maqam") DEMONSTRABLE with real content.

### Differentiate from Ableton's tuning library
Ableton's collection is a static REFERENCE library (scales you load). This goes further: not just "here's
maqam Rast" but "here's Rast, its ajnas as .dmtunes, and a patch that MODULATES between them." Ship the
DYNAMICS of the system, not just its scales.

### CAUTION -- accuracy (matters MORE in a cultural-tribute project)
Maqam tunings are contested and regional: Arabic vs Turkish vs Persian systems DIFFER; the "quarter
tone" is an approximation (real ajnas use specific non-equal intervals varying by region/performer);
24-EDO is itself a simplification purists reject. When generating .scl/.dmtune content, either:
(a) use WELL-SOURCED interval values and SAY which tradition/source ("after the Arabic maqam system,
    tuning per [source]"), OR
(b) be explicit these are 24-EDO APPROXIMATIONS for accessibility, not authoritative.
Shipping naive-24-EDO "maqams" as authentic would UNDERCUT the respect the project is built on -- the
Singapore-plurality/love-letter framing makes accuracy matter MORE, not less. Source the intervals
properly or be honest about the approximation.

Cross-ref: the .scl/.dmtune format hierarchy (TUNING_PRESET_FORMAT / SHOPHOUSE_MICRO_SPEC), the tonic-
transpose work (ajnas relocate), Shophouse Micro (maqam modulation via jins slots), LAUNCH_INTENT_AND_
STORY (the Change Alley "pronounce a maqam" origin -- now demonstrable), the two product shapes above.

## Is 2-ajnas (Shophouse Micro at 24 degrees) a big limit for maqam? Honest assessment (Rodney asked)

Shophouse Micro holds 4 fronts at 12 degrees but only 2 at 24 (slot budget halves). So a 24-degree
maqam can switch between 2 ajnas. Is that a big limit?

### Where 2 IS tight
A full maqam performance (a developed taqsim) can traverse SEVERAL ajnas -- primary jins on the tonic,
secondary on the dominant (4th/5th), plus borrowed ajnas from related maqamat: maybe 3-6 across a full
development. To reproduce the FULL modulatory journey, 2 slots runs out partway.

### Why 2 is LESS limiting than it first sounds (three reasons)
1. **Most maqamat are fundamentally a TWO-JINS structure.** A maqam is defined by primary jins (tonic) +
   secondary jins (dominant) -- that skeleton IS the maqam's identity (Rast = jins Rast on tonic + on
   5th; Bayati = jins Bayati on tonic + Nahawand/Rast on 4th). 2 slots captures the ESSENTIAL identity;
   further modulations are development, not core.
2. **The 24-degree .scl already holds ALL the pitches.** The tuning is the full pitch set; the .dmtune
   slots are just MASKS selecting subsets. With only 2 slots you still have every degree available in
   the tuning -- limited only in how many PRE-SET masks you switch between at the boundary, not in
   pitches. Notes outside the 2 masks still exist, just not in the current scale-mask.
3. **2-slot switch is boundary-quantised modulation -- ONE type of maqam movement.** Maqam also
   modulates MELODICALLY (moving through the pitch set without a hard scale change), which you do freely
   within the 24-degree tuning regardless of slots (all pitches present). The 2-slot limit only bites on
   pre-set MASKED modulation, not melodic movement.

### Honest verdict
- For a FULL traditional taqsim (4-6 ajnas): yes, 2 is limiting.
- For a maqam's ESSENTIAL two-jins identity: no -- 2 is the canonical core.
- For PITCHES available: no limit -- the full tuning has them all.
- For SAMPLE CONTENT ("here's a maqam, its core ajnas, modulation between them"): 2 is SUFFICIENT -- the
  tonic-jins <-> dominant-jins switch is the canonical, most-recognisable maqam gesture (the "hello
  world" of maqam modulation), exactly what 2 slots does well.
Don't oversell 2 slots as "full maqam performance"; DO present it as "the essential two-jins modulation",
which is honest and still compelling.

### If maqam work proves central (post-V1 fix)
The fix is NOT more panel slots -- it's decoupling ajnas-count from panel-slot-count:
- A **CV-INDEXED JINS BANK**: store >2 masks as a loadable LIST the CV indexes into, rather than 2
  physical fronts. This lifts the limit without needing panel space. The real design option if maqam
  becomes central.
- Melodic modulation within the full tuning needs NO slot (shared pitches, melodic emphasis).

Cross-ref: Shophouse Micro (4 fronts @12 / 2 @24), the .scl/.dmtune hierarchy (tuning=all pitches,
.dmtune=mask subset), the maqam/jins sample content above, Monsoon scale-authoring (loadable user
scales -- a step toward a jins bank).

## Third modulation layer: DEGREE modulation WITHIN a jins (Rodney) -- shrinks the 2-slot concern further

Correcting/extending the 2-ajnas assessment: modulation happens at THREE levels, not one, and the
2-slot limit only touches the COARSEST -- which is the least expressively-central.

### Three modulation layers
1. **Slot switch** (Shophouse Micro, 2 fronts @24): jump between whole AJNAS at a phrase boundary. The
   coarse, structural modulation. THIS is what the 2-slot limit constrains.
2. **Melodic movement** through the full 24-degree tuning: all pitches present, move freely (no slot).
3. **DEGREE modulation WITHIN a jins**: the per-degree controls (Colonnades/Duo faders/weights/cents,
   per-degree enable/weight) shift, weight, emphasise, inflect degrees INSIDE the active jins -- no slot
   switch needed. The fine, expressive modulation.

### Why layer 3 matters most (and is UNconstrained)
Maqam expression lives substantially at the DEGREE level, not the jins level. The SAYR (a maqam's
melodic path/behaviour) is about specific degrees emphasised, certain notes unstable and pulling to
resolution, microtonal inflection of individual degrees varying by region/phrase. That's WITHIN-jins
degree behaviour, not jins-switching. A maqam isn't just "which ajnas" -- it's HOW you treat the degrees
inside them (which you lean on, bend, pass through). The degree-level controls express exactly that, and
they're FULLY available regardless of slot count.

### Revised verdict on the 2-slot limit
The 2-slot limit constrains only layer 1 (jins-SWITCHING) -- the least expressively-central layer.
Layers 2 (melodic freedom) and 3 (degree inflection = the expressive HEART of maqam) are unconstrained.
A convincing maqam with deep degree inflection but 2 ajnas is far more authentic than a shallow one
mechanically switching 6 ajnas. The degrees are where the soul is -- and that's exactly what dot.modular
gives you in full. So the 2-slot limit is even less of a real constraint than the prior section said:
it caps the structural jins-journey, not the expressive substance.

Cross-ref: the 2-ajnas assessment above (this extends it), the per-degree controls (Colonnades/Duo
faders/weights/cents, enable/weight), the .scl/.dmtune hierarchy (degree-level lives below the mask).

## How unique is "apply own rhythm to external CV"? Honest calibration (Rodney asked) -- claim carefully

Searched the VCV landscape. Differentiation is REAL but should be claimed CAREFULLY (not "nothing else
does this").

### What exists (so DON'T overclaim)
- **Static quantisers, many**: snap CV to scale, some generate a gate WHEN THE NOTE CHANGES (VCV lib has
  several: "quantizer that generates gates as notes change", S&H quantisers, poly-to-poly). These follow
  the INPUT's timing -- they do NOT impose their own generated rhythm. Not what we do.
- **Sequencers that CAPTURE/RECORD external CV then play it on their clock, several**: Impromptu
  PhraseSeq (keyboard CV+gate -> Write inputs, autostep captures), Entrian (records CV, quantises at
  set points). Closer -- but RECORD-then-play, not a LIVE pass-through transform, and none combine with
  microtonal re-tune + arrangement + poly + MPE-out.

### What did NOT surface (but absence in one search != proof of non-existence)
A module taking LIVE external CV and, in real time, re-gating/re-rhythming it with its OWN generated
(probabilistic, any-division) rhythm WHILE re-tuning microtonally AND arranging, poly. That specific
LIVE-TRANSFORM chain didn't appear. But VCV has thousands of modules; one search isn't the whole
library -- can't credibly claim uniqueness.

### The defensible claim (use THIS at launch, not "nothing else does this")
Individual pieces (quantise, re-gate-on-change, capture-and-replay) are each done by others. What's
genuinely UNCOMMON is the COMBINATION as a LIVE pass-through transform:
  "Few if any modules take live external CV and impose their own generated rhythm AND microtonal
   re-tuning AND arrangement AND poly / MPE-out as a single real-time transform stage."
The differentiation lives in the INTEGRATED CHAIN and the LIVE-TRANSFORM framing, NOT any single
capability. AVOID a flat "nothing else does this" -- a knowledgeable forum reader could name a partial
counterexample (a capture-replay sequencer, a re-gating quantiser) and undercut the claim. Lead with the
integrated-chain-as-live-transform being uncommon: honest AND still strong.

### Why still a strong position
Even if pieces exist separately, dot.modular offering them as ONE coherent stage -- fed by the same
correlation engine, with the microtonal tunings and MPE-out -- is a real, defensible differentiator, and
the interoperability framing ("drop after any sequencer") holds regardless of whether a partial
counterexample exists. Just don't claim first-ever; claim uncommon-integration.

Cross-ref: the interoperability positioning above (the claim to calibrate), the two product shapes, the
MPE-out chain (Keppel).

## Foundation check before poly quantiser: are the GATES poly? (Rodney flagged) -- verify first

Rodney flagged: quantiser mode D (external-gate) needs POLY GATES IN (one gate per incoming poly CV
voice); and -- deeper -- is the internal Mode B gate sequencer even POLY, or mono? If Mode B generates
MONO gates, the new-C (generated-gate-driven quantiser) can't drive poly voices independently (one gate
for all voices breaks per-voice re-articulation + per-voice legato).

### What the code shows (partial -- NOT conclusive on gates)
- The engine HAS poly voices: voices[i] up to 15, per-voice strands (polyStrandLen), executePolyVoice,
  per-voice LOR/legato. So the PITCH side is poly.
- But I did NOT definitively locate a `GATE_OUTPUT.setChannels(N)` confirming PER-VOICE gate emission.
  The main GATE_OUTPUT (gs.process, Monsoon.cpp:781/816) may be MONO (a single fused gate) even though
  pitch is poly. UNRESOLVED in-container -- must be checked.

### The check to do (post-holiday, before building poly quantiser)
1. Does GATE_OUTPUT emit POLY (setChannels = voice count, a gate per voice) or MONO (one gate)?
   Look at generateOutputs / where GATE_OUTPUT.setVoltage is called -- is it in a per-voice loop with
   setChannels, or a single setVoltage?
2. Same for the STEP / within-legato gates (the Keppel + quantiser re-articulation grid) -- poly?
3. If gates are MONO: the poly quantiser (and poly Keppel legato re-articulation) need the GATE side
   WIDENED to poly first -- a foundation task before the quantiser modes. This is the gate-side analog
   of the M1 engine-widening that made pitch microtonal/poly.

### Why it matters
- Quantiser D: poly gate IN needed (Monsoon or Straits) -- to know per-voice when to sample/re-articulate
  the incoming poly CV.
- Quantiser new-C: needs the engine's GENERATED gates to be poly, or it can't gate poly voices
  independently.
- Keppel poly legato (the selective re-articulation): also needs per-voice gates. So this same gate-poly
  question underpins BOTH the poly quantiser AND poly Keppel phrasing.
So "are the gates poly?" is a shared foundation for the poly quantiser, poly Keppel, and the within-
legato-gate work. Verify BEFORE building any of them; widen the gate side first if mono.

CAUTION (codebase risk-shape): the pitch being poly might create a false assumption that gates are too.
Don't assume -- CHECK. "Pitch poly, gate mono" is exactly the kind of asymmetry that compiles and half-
works then breaks on the second voice.

Cross-ref: SequencerEngine poly voices (voices[i], executePolyVoice -- pitch poly), Monsoon.cpp:781/816
(GATE_OUTPUT / gs.process -- check poly-vs-mono), the quantiser modes (D poly-gate-in, C generated-poly-
gate), Keppel poly legato re-articulation (needs per-voice gates), the within-legato gate.

## CONFIRMED: Mode B gate INPUT is MONO -- poly gate IN was never tackled (Rodney was right)

Rodney: "Check Mode B code, pretty sure we never tackled poly gate in." CONFIRMED from code.

### The evidence (Monsoon.cpp:527-530)
    input.gate1 = cachedGate1Connected ? inputs[GATE1_INPUT].getVoltage() : 0.f;
    input.gate2 = ... inputs[GATE2_INPUT].getVoltage() ...
    input.gate3 = ... inputs[GATE3_MOD_INPUT].getVoltage() ...
    input.run   = ... inputs[RUN_GATE_INPUT].getVoltage() ...
All gate inputs use getVoltage() -- the MONO read (channel 0 only). NOT getPolyVoltage(i), no
getChannels(), no per-voice loop. input.gate1 is a single float. Mode B (modeSelect==1) drives the
sequencer from this ONE mono gate (gate1High, gate1Rise are scalars, :683/:687/:792). So the poly gate
IN was never built -- Mode B reads a single mono gate on Gate 1.

### Consequence for the quantiser modes
This is the INPUT-side gap (distinct from the output/generated-gate question flagged earlier):
- **Quantiser Mode D (external-gate-driven)**: needs to sample EACH poly voice on ITS OWN incoming
  gate. On the current MONO gate-in it can't -- it would sample all voices on one shared gate. So Mode D
  REQUIRES a poly gate input first. NEVER TACKLED -- confirmed.
- **Quantiser Mode C (generated-gate-driven)**: depends on whether the GENERATED (output) gates are
  poly -- the separate, still-open output-side question.

### So there are TWO poly-gate foundation gaps
1. **Poly gate IN** (for Mode D): external gate input must become poly -- getPolyVoltage(i) per voice /
   getChannels(), a gate per incoming CV voice. Currently MONO (getVoltage(), :527). CONFIRMED not done.
2. **Poly gate OUT / generated** (for Mode C + Keppel poly legato): whether the engine's generated gates
   are per-voice. STILL TO VERIFY (prior flag).

### Build order implication
Before the poly quantiser: (a) widen the gate INPUT to poly (Mode D dependency, confirmed needed),
(b) verify/widen the generated gate OUTPUT to poly (Mode C + Keppel dependency). Both are gate-side
foundation work -- the analog of the M1 pitch-widening, but for gates. The poly quantiser can't be built
on mono gate I/O. Do the gate-poly foundation FIRST.
Note: poly gate in belongs on Monsoon or Straits (Rodney) -- the CV-in host from the quantiser spec; the
poly gate in likely pairs with the poly CV in on the same expander (Straits).

Cross-ref: Monsoon.cpp:527-530 (mono getVoltage gate reads), the quantiser modes (D = external poly
gate, C = generated poly gate), the prior poly-gates foundation flag (output side), Keppel poly legato
(per-voice gates), poly CV in on Straits (pairs with poly gate in).
