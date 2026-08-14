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

## "One clock, poly gates" -- separate the CLOCK from the SAMPLING (Rodney's question)

Rodney: Mode B was fixed by realising each external gate = a 1/16 and the rising edge ADVANCES THE CLOCK
(Mode A logic). How to poly that when there's only ONE clock?

### The resolution: DON'T poly the clock -- split its two fused roles
The Mode B fix FUSES two roles into one gate edge: (a) advance the clock/pattern position, AND (b)
trigger a note. That fusion is fine mono but can't poly directly -- a SINGLE shared pattern position
can't be coherently advanced by 16 independent rising edges (voice 3's gate advancing the position voice
1 reads = chaos). So the mono model doesn't lift as-is. Split the roles:
- **Clock / pattern-advance**: stays MONO, ONE source (internal clock, or one designated gate). One
  sequence position, never 16 clocks.
- **Sampling / quantise / per-voice gate**: goes POLY. When the shared clock says "now", sample ALL
  poly CV voices together.
"One clock, poly SAMPLING." The clock stays singular; poly lives in the sampling layer, not the clock
layer. You keep one clock and poly the thing DOWNSTREAM of it.

### Per-mode (only some modes even have a clock)
- **Mode C (generated-gate)**: the engine's generated rhythm IS the clock -- and it's ONE rhythm. At
  each generated gate, sample all poly CV voices. One clock, poly chord sampled per tick = "impose one
  rhythm on an incoming poly chord" (the natural meaning of giving poly CV a new rhythm). NO poly-clock
  needed. (Only per-voice GENERATED rhythms would need the generated gates themselves poly -- separate
  question.)
- **Mode D (external-gate)**: has NO internal clock at all (Vermona D just quantises while the gate is
  high, doesn't advance a pattern). Poly-D = quantise each voice's CV on ITS OWN incoming gate -- pure
  per-voice, driven entirely by external poly gates. No shared clock to conflict with -> the "one clock"
  problem DOESN'T APPLY to D. Just a poly loop: getPolyVoltage(i) pitch + per-voice gate edge detection
  on the poly gate in. (This is the poly-gate-in confirmed missing -- but it's per-voice EDGE DETECTION,
  not 16 clocks.)
- **Per-voice CLOCKS (16 independent sub-sequencers, each advancing its own position on its own gate)**:
  the ONLY model that needs 16 clocks. Expensive, and probably NOT wanted for a quantiser. Explicitly
  DECIDE you're not doing this -- it's the interpretation that makes the problem look impossible, and
  it's the one to discard.

### Net
Clock stays mono. Separate "advance the pattern" (one source) from "sample the voices" (poly). Mode C =
one generated clock + poly sampling; Mode D = no clock, pure per-voice quantise-on-external-poly-gate.
Neither needs poly clocks. The poly-gate-in is per-voice EDGE DETECTION for D's sampling, a much smaller
lift than "16 clocks" implies.

### Verify in code before building
Confirm whether the Mode B fix genuinely FUSED advance-and-trigger. If it did, the poly work is partly
UN-FUSING them -- making "advance the shared position" and "emit a per-voice gate" separable so the
latter goes poly while the former stays singular. If they're already separable, poly-sampling is a small
change; if welded, un-fuse first. (Architectural reasoning here -- the current clock structure needs a
code check to confirm the fusion.)

Cross-ref: the Mode B fix (gate=1/16, rising edge advances clock, Mode A logic), the confirmed mono gate
IN (Monsoon.cpp:527), the quantiser modes C/D, the poly-gate foundation flags (in + generated-out).

## "One clock" vs poly gate in: resolved -- Mode D has N clocks (one per gate), Mode C shares one (Rodney)

Rodney: Mode B mono was fixed by applying Mode A logic + realising each gate = 1/16 and the RISING EDGE
advances the clock. Q: how to poly that when we only have ONE clock? Resolution: the "one clock"
constraint only binds if you assume a SHARED step position. Poly gate-in dissolves it differently per
mode.

### Two meanings of "poly gate in" (they need different answers)
1. **Shared clock, poly CV = a CHORD advanced together**: all voices advance on the SAME gate edge;
   poly-ness is only in PITCH (quantise a chord moving in lockstep). Needs NO gate-input change -- one
   mono gate advances, poly CV quantised per-voice. The simplest poly quantiser; may be all that's
   needed for chordal use.
2. **Per-voice independent gates = polyrhythmic independent lines**: each voice advances on ITS OWN gate
   rising edge (voice 1 steps while voice 2 holds). THIS needs poly gate-in, and this is where "one
   clock" feels binding.

### Resolution per mode (neither needs "N impossible clocks")
- **Mode D (external-gate quantiser)**: there is NO shared clock -- each EXTERNAL gate channel IS its own
  clock. Poly gate-in = N external clocks, which is exactly what external per-voice gates ARE. Read
  getPolyVoltage(i) per channel, detect rising edge per channel, advance/sample THAT voice on ITS edge.
  Voices are independent because their GATES are independent. The tension dissolves: you never had one
  clock in Mode D -- you have one per incoming gate channel.
- **Mode C (internal generated gate)**: ONE shared clock advances the PATTERN POSITION, but per-voice
  RHYTHM STRANDS decide whether THAT voice gates at each position. Poly gates OUT of one clock via the
  per-voice strands that ALREADY exist for pitch. Clock shared; gating per-voice -- same mechanism the
  poly PITCH already uses (one clock, per-voice strands).

### The key subtlety in Mode D: PER-VOICE step position
If each voice advances on its own gate, the "sequencer step" is no longer ONE number -- each voice has
its OWN step position (voice 1 on step 5 while voice 2 on step 3, different gate-edge counts). Same idea
as the lane-position model (position = function of steps-elapsed) but steps-elapsed is now PER VOICE,
driven by that voice's gate-edge count. So Mode D poly = PER-VOICE STEP COUNTERS, each advanced by its
own gate's rising edges. This is the real structural change (shared step index -> per-voice step index),
and it's the thing to design for.

### Honest answer to "how poly it with one clock"
- Mode D: you DON'T have one clock -- each poly gate channel is its own clock advancing its own voice's
  step counter (per-voice step position). Read poly gate, per-channel rising-edge -> advance that voice.
- Mode C: keep one clock; per-voice rhythm strands gate each voice (poly gates from one clock, as pitch
  already works).
The "one clock" only blocks if you assume a shared step position. Mode D drops that assumption
(per-voice steps); Mode C keeps one clock but gates per-voice via strands.

Cross-ref: the Mode B mono fix (gate rising edge = 1/16 advance, Mode A logic), the mono-gate-in finding
(Monsoon.cpp:527), the lane-position model (per-voice steps-elapsed), the poly pitch machinery (one
clock + per-voice strands = the Mode C template), quantiser Modes C/D.

## SIMPLER SOLUTION (Rodney): poly Mode B/D = gate gives the step, Sands poly rules do the rest

Supersedes the per-voice-step-counter framing above. The clean answer: DON'T invent per-voice clocks.
At each step boundary, apply the SAME Sands per-voice legato/rest/accent rules that Mode A poly already
uses. The gate provides the step boundary (the WHEN); Sands poly rules do the per-voice differentiation
(the WHAT-each-voice-does).

### The mechanism
- **Gate rising edge = ONE shared step advance** (exactly as Mode B mono already does -- each gate =
  1/16, rising edge advances). Mode B: mono gate. Mode D: external gate (poly or mono) provides the
  step boundary.
- **At each shared step, run Sands poly rules per voice** (exactly as Mode A poly already does):
  - **Poly REST**: each voice opts IN or OUT of sounding at that step.
  - **Poly LEGATO**: each voice independently ties across the boundary or re-articulates.
  - **Poly ACCENT**: per-voice emphasis.
- So voices share ONE step grid (from the gate) but are DIFFERENTIATED by per-voice rest/legato/accent.
  Genuinely polyphonic (independent lines) WITHOUT independent clocks.

### Why this beats the per-voice-step-counter idea
1. REUSES Mode A poly wholesale -- the Sands legato/rest/accent poly rules are already written, tested,
   working. Apply an existing proven system at a new CLOCK SOURCE (external gate vs internal clock).
2. Sidesteps per-voice-step-position complexity entirely -- ONE shared step index, gate-advanced, no
   divergent per-voice positions to track. (My earlier per-voice-step-counter framing was
   overcomplicated -- you do NOT want independent step positions; you want one step grid + per-voice
   behaviour at each step.)
3. Consistent across the mode family: Mode A (internal clock) and Mode B/D (external gate) become the
   SAME engine with different clock sources, all running identical per-voice Sands rules. Extends the
   quantiser unification to the gate side.

### Net
Poly Mode B/D is not a NEW build -- it's a COMPOSITION of two working things:
  Mode B gate-drives-clock (mono, done) + Mode A Sands-poly-per-step (poly, done).
Gate says WHEN (shared step advance); Sands says WHAT each voice does (rest/legato/accent per voice).
The poly gate handling = the intersection of two existing systems, not new machinery.

### Build note
- Mode B poly: mono gate advances the shared step; run Sands poly rest/legato/accent per voice at each
  step (Mode A poly path, but clocked by the gate edge instead of the internal clock).
- Mode D poly: same, but the step boundary comes from the external gate (which can be poly -- but even
  then, treat each gate edge as a step advance and let Sands poly rules differentiate; OR use per-voice
  gate edges if truly independent per-voice rhythm is wanted -- but the SHARED-step + Sands-poly path is
  the simple default and likely sufficient).
- This makes poly gate-in far less of a "foundation rebuild" than feared: the poly differentiation
  already exists (Mode A Sands poly); only the clock SOURCE changes.

Cross-ref: Mode A poly Sands rules (rest/legato/accent -- the reused machinery), Mode B mono fix (gate
edge = 1/16 step advance), the mono-gate-in finding (:527), the quantiser modes, the accent/legato work.

## CORRECTION (Rodney): Mode B AND D need only MONO gate in -- and Mode B poly likely ALREADY works

Correcting my drift: I'd been treating poly-gate-in as a foundation gap. Rodney: neither Mode B nor D
needs poly gate IN -- MONO suffices for both. The gate's only job is the SHARED step boundary (the
WHEN); one mono gate does that. The poly-ness comes AFTER, from Sands per-voice rules at each step:
- Mono gate in -> shared step advance.
- Sands poly rules -> per-voice rest/legato/accent at that step.
- Poly CV out -> the differentiated voices.
One mono gate drives everything; there is NO poly gate-in to build. My "poly gate-in foundation gap"
was solving a problem that doesn't exist -- mono IS the design. (The earlier mono-gate-in finding at
:527 is therefore CORRECT AS-IS, not a gap.)

### Mode B poly likely ALREADY implemented (verify the wiring)
Rodney: "surprised if we don't already have this in Mode B." Code strongly suggests he's right:
- Mode B (modeSelect==1) is a STEPPED mode (SequencerEngine.cpp:362), same category as Mode A (0).
- executeModeB (:651) calls executeStep (:729) -- the SAME step function Mode A uses (:641).
- executeStep is explicitly "mode-agnostic" (:725) -- the step logic doesn't care which mode clocked it.
- executePolyVoices / executePolyVoice (:971/:753) hold the Sands per-voice rest/legato/accent rules.
So the architecture is SET UP for Mode B poly: mode-agnostic step + shared poly voices. Mode B poly
probably already runs.

### BUT -- could NOT confirm the wiring from the traced snippets (verify, don't assume)
I did NOT confirm executePolyVoices is actually CALLED in Mode B's execution path (vs wired only for
Mode A). The pieces exist and are architecturally compatible, but "pieces exist + compatible" != "wired
and runs in Mode B" -- and this codebase's risk-shape is EXACTLY that gap (compiles, plausibly works,
but a path isn't connected). So: probably already works, but TEST it.

### The settling test (post-holiday, quick)
Run Mode B with POLY output and watch the voices:
- If they DIFFERENTIATE (per-voice rest / legato / accent) -> Mode B poly already works, nothing to do.
- If all voices move in LOCKSTEP (no per-voice rest/legato) -> executePolyVoices isn't wired into Mode B;
  add that one connection (route Mode B's step execution through executePolyVoices, as Mode A does).
Either way it's small: the poly differentiation machinery exists; at most it's a wiring connection, not
a build. And Mode D = Mode B with the gate from an external source -> same story, mono gate + Sands poly.

Cross-ref: SequencerEngine.cpp:362 (Mode B stepped), :651/:729 (executeModeB -> executeStep,
mode-agnostic), :753/:971 (executePolyVoice/s -- the Sands poly machinery), :527 (mono gate in, correct
as-is), the Sands-poly-per-step solution above, Mode A poly (the reference wiring).

## Mode D stated minimally (Rodney): = Mode B + one poly CV in. Nothing else new.

Mode D gets the SAME mono gate input as Mode B; it just needs ONE additional input: a poly CV in for the
notes to quantise. That's the whole difference.

### Mode D = Mode B + poly CV in
- SAME mono gate in as Mode B -> the shared step boundary (the WHEN). No new gate infrastructure.
- SAME Sands poly rules at each step -> per-voice rest/legato/accent (how each voice behaves).
- THE ONE ADDITION: a poly CV in -> the external notes to quantise (the WHAT PITCH). The only new input
  over Mode B.

### The only difference B vs D is the PITCH SOURCE
- Mode B: pitch from the engine's internal melody draw.
- Mode D: pitch from the external poly CV, quantised to the scale.
Everything else -- gate-driven shared step, Sands poly differentiation, output -- is IDENTICAL. Mode D is
Mode B with the internal melody draw swapped for "quantise this poly CV." The concrete input-level form
of the quantiser=sequencer unification: D is B's twin, sharing gate + poly machinery, differing only in
one poly CV in replacing internal pitch generation. (Matches "Sands melody/octave ignored in quantiser
modes" -- in D the melody draw is REPLACED by the poly CV quantise.)

### Why it's cheap to build (almost all pieces exist)
1. Gate handling: already there (Mode B mono gate -> shared step).
2. Poly differentiation: already there (Sands poly rules; likely already wired in Mode B -- pending the
   settling test).
3. Quantise function: already there (SequencerEngine::quantize, weighted radius-gated snap).
4. Genuinely new: ONE poly CV input jack + routing it into the quantise stage instead of the internal
   melody draw.
So Mode D = existing gate + existing Sands poly + existing quantiser + one new poly CV input, wired so
that at each step (mono gate) each voice quantises ITS channel of the incoming poly CV to the scale,
Sands rules deciding rest/legato/accent per voice.

### Input placement (refined -- simpler than before)
Poly CV in on Straits (or Monsoon), per the earlier placement. It's JUST the CV -- NOT a paired poly
gate (the gate stays mono, shared with Mode B). So the Straits addition for Mode D is a SINGLE poly CV
input, not a CV+gate pair. Simpler than the earlier "poly CV + poly gate" framing.

Cross-ref: the mono-gate correction above (B and D both mono gate), the Sands-poly-per-step solution,
SequencerEngine::quantize (existing quantiser), Straits poly CV in (single input, no paired gate), the
quantiser=sequencer unification (D = B with external pitch source).

## UI refinement stage: mode-relevant display dimming (Rodney) -- post-holiday, UI phase

The panel should visually reflect which controls are LIVE in the current mode (greyed = "does nothing
now, and here's why: the mode"). Same honesty as the greyed faders in the enabled/N work.
- **Quantiser modes**: dim/disable Sands MELODY + OCTAVE displays (pitch comes from CV, those
  pitch-generation controls are irrelevant -- already bypassed in logic; dimming makes the panel SAY so).
- **Gate-driven modes**: dim Sands VARIATION, disable NOTE VARIATION + LENGTH MOD (note duration = the
  gate's width, not an internal length -- Mode B code already nullifies internal note-length and bypasses
  variation: ":686 note DURATION is Gate 1's width", ":694 Variation intentionally bypassed in Mode B").
  So dimming REFLECTS a bypass that already exists -- the UI catching up to the logic.
Principle: greyed = inactive-in-this-mode + why. Depends on mode behaviours being functionally settled
first (dim the RIGHT things once final). UI-refinement-stage polish, which Rodney hopes is not far off
after the holiday.

## Behaviour gap: spread on ALL lanes for Sands-mono + Straits voice-1/mono (Rodney) -- verify path

Rodney: Sands (mono) and Straits -- but NOT Helix -- should get spread applied on ALL lanes for
voice-1/mono. Currently MISSING on the LEGATO and ACCENT lanes. Reason: the negative spread (1 - draw)
should be available to ALL lanes -- spread/invert isn't just a rest-lane thing; legato and accent should
have it too.

### What I could verify (partial -- and only in the POLY/East path, NOT the mono path Rodney means)
In the poly (East) path MonsoonExpanderManager.cpp: REST (:396-405), MELODY (:421-426), OCTAVE
(:449-454), ACCENT (:468-484) all get combineSpread + SpreadInterp::apply (accent spread CV was
previously missing, since fixed :469). **PL_LEGATO is ABSENT from this poly spread list** -- consistent
with "legato missing spread". BUT this is the POLY/East path; Rodney's point is about voice-1/MONO on
Sands-mono + Straits, a DIFFERENT code path I did NOT locate. So: legato-missing-from-poly-spread is
confirmed here, but the MONO voice-1 legato+accent spread gap Rodney describes is NOT yet located in
code. Don't conflate mono and poly paths (this codebase's off-by-one breeding ground).

### The intent (design statement to implement, post-holiday)
- Apply spread (incl. negative spread = 1 - draw, the invert) to ALL lanes -- REST, MELODY, OCTAVE,
  LEGATO, ACCENT -- for voice-1/mono on Sands-mono and Straits.
- NOT Helix (explicitly excluded).
- Currently missing on LEGATO and ACCENT (per Rodney). Rationale: negative spread/invert should be
  universally available per lane, not rest-only.
- CC to: find the mono voice-1 spread application (distinct from the poly/East path above), confirm
  legato + accent are missing there, add spread (with the 1-draw negative/invert) to those lanes for
  Sands-mono + Straits, leave Helix untouched. Then verify with a spread sweep per lane.

Cross-ref: SpreadInterp (src/dsp/SpreadInterp.hpp, single source of truth), MonsoonExpanderManager.cpp
poly spread (:388-484, legato absent), the spread-sign / negative-spread-inverts note (:277), the
probability-modifier unification (spread engine migration).

## Shared Change Alley + mixed modes: the reroute belongs at the Monsoon, not Change Alley (Rodney)

Subtle topology problem: Change Alley (correlation engine / East-West axis) is SHARED across Monsoons,
but MODE (sequencer vs quantiser) is PER-Monsoon. So Monsoon A can be in sequencer mode and Monsoon B in
quantiser mode, both on the SAME shared Change Alley. How does the shared resource behave when consumers
disagree about mode?

### Problem 1: can't dim melody pins on a shared Change Alley
Can't disable Change Alley's melody pins just because ONE Monsoon is in quantiser mode -- the OTHER
(sequencer) still needs it. The dice are on the SHARED resource; relevance is the UNION of what all
consumers need. So the "dim melody pins in quantiser mode" UI rule breaks on a shared Change Alley.
Rule must be: never dim melody pins on a shared Change Alley (or only dim if ALL connected Monsoons are
quantiser -- but "never dim a shared resource on one consumer's mode" is safest).

### Problem 2: melody pins rerouted to the poly CV in (Rodney's elegant instinct)
In quantiser mode the melody pins are irrelevant to PITCH GENERATION (pitch from CV). Instead of
disabling them, REROUTE the melody pins to control/manipulate the poly CV in (pin-style
manipulation of the incoming CV being quantised). Why it's more than "don't waste the control":
- Melody pins are a PITCH-DOMAIN control (sequencer: perturb generated pitch). In quantiser mode the
  pitch domain is the external CV. Rerouting melody pins -> poly CV manipulation is SEMANTICALLY
  CONSISTENT: the pins always affect "the pitch material"; only the material's SOURCE changed
  (generated -> external CV). The control keeps its meaning, its target follows the mode's pitch source.
  Exactly the quantiser unification ("same machinery, swapped pitch source") applied to the melody pins.
- Turns dead panel space (disabled pins) into expression (CV scrub/reorder/perturb), free (pins mechanism
  exists).

### The collision + resolution: reroute at the MONSOON, not at Change Alley
Collision: a SHARED melody pins can't simultaneously be "pitch generation for A (sequencer)" AND "CV
routing for B (quantiser)" -- same physical pins, two jobs. Resolution (LEAN: Option B):
- **A shared resource stays MODE-AGNOSTIC; mode-dependent interpretation belongs at the CONSUMER.**
- Change Alley always emits the melody-pins signal as-is (pitch-domain), knowing nothing about consumer
  modes. Each MONSOON, per ITS mode, routes that signal internally: sequencer-Monsoon -> pitch
  generation; quantiser-Monsoon -> its poly-CV-quantise/manipulation stage. Same shared signal,
  per-Monsoon routing. No conflict.
- Solves Problem 1: melody pins NEVER dimmed on shared Change Alley (it's mode-agnostic, always shows).
- Solves Problem 2: the reroute is a per-Monsoon internal decision, so A and B can route the same shared
  dice differently without contradiction.
Rejected (Option A -- reroute at Change Alley): would bake per-consumer-mode behaviour into a shared
resource = the contradiction. Keep Change Alley dumb + shared; make the Monsoons smart + per-mode.

### Principle (general, for the shared-resource topology)
Shared resources (Change Alley) stay mode-agnostic and emit raw signals. Mode-dependent
interpretation/routing lives at the per-consumer (Monsoon) that has the mode. This is the only topology
that avoids "shared resource with conflicting per-consumer modes". Applies beyond melody pins to any
shared-Change-Alley signal that a mode would want to reinterpret.

Cross-ref: the quantiser unification (swapped pitch source), the UI mode-dimming note (this AMENDS it for
shared Change Alley -- don't dim shared resources per one consumer's mode), Change Alley (shared
correlation engine), the per-Monsoon mode.

## Terminology correction (Rodney): it's melody PINS, not melody dice, in the shared-Change-Alley case
The shared-Change-Alley mixed-mode discussion above concerns melody PINS, not melody dice (corrected).
The control rerouted-to-poly-CV in quantiser mode, and the one that can't be dimmed on a shared Change
Alley, is the melody PINS.
NOTE (verify the pins mechanism before over-specifying the reroute): pins and dice are DIFFERENT
controls. Likely distinction (CONFIRM with Rodney/code post-holiday): dice = the randomisation/re-roll
(scrub/re-generate the stochastic draw); pins = per-step FIXING/locking of melody values (pinning a
step's pitch so it doesn't re-roll). If so, the reroute is even more apt: pins FIX pitch material, so
rerouting melody pins into the poly-CV domain = "pin/fix aspects of the incoming CV" rather than scrub
it. But this rationale is UNCONFIRMED -- the term is corrected to pins; the exact pins-in-quantiser-mode
behaviour should be pinned down (pun noted) when the pins mechanism is re-read. Don't encode the "pin the
incoming CV" mechanism as settled until confirmed.
Cross-ref: the shared-Change-Alley section above (now using melody pins), the melody-pins/dice
distinction (verify).

## CONFIRMED: melody pins = per-voice SOURCE-ROUTING; reroute the random draw (seq) or the poly CV (quant)

Rodney clarified what melody pins actually do (my earlier "pin/fix the CV" guess was WRONG). Pins are a
per-voice SOURCE-SELECTION / ROUTING matrix:
- **Sequencer mode**: pins create CORRELATIONS by rerouting the RANDOM DRAW -- a pin says "this voice
  takes its draw from THAT source instead of its own", so pinning voice 2 to voice 1's draw correlates
  them (voice 2 follows voice 1's random pitch). Pins = how you build the East-West correlations at the
  voice level (wire voices to share draws).
- **Quantiser mode**: pins reroute the same way but on the POLY CV instead of the random draw. Example
  (Rodney): CV channels 1-4 going through the system, a pin might route "CV 1 in" to multiple voices --
  voices 2,3,4 all read CV channel 1 instead of their own. Pins do to the external poly CV exactly what
  they do to the internal draw: reroute WHICH SOURCE each voice reads.

### The unifying insight
Pins are a per-voice SOURCE ROUTER; what they route AMONG is the pitch source -- internal draws
(sequencer) or external CV channels (quantiser). The pin doesn't care if the source is a random draw or
a CV channel; it just says "voice X reads source Y." So the reroute is clean: the pins ALREADY are a
source-router; quantiser mode just changes the POOL of sources (draws -> CV channels). Same mechanism,
swapped source pool -- the quantiser unification at the PIN level.

### The musical payoff (why this is more than "don't waste the control")
In quantiser mode, pins let you CORRELATE the external CV voices: feed 4 CV channels, pin voices 2-4 to
CV 1 -> 4 voices all tracking CV 1's (quantised) pitch = a unison/correlated texture from one CV line.
Or pin to different channels for independence, or mix. The SAME correlation-building expressiveness the
pins give in sequencer mode, now on external CV. Change Alley's correlation apparatus (the East-West
heart) works IDENTICALLY on external CV as on internal draws -- because the pins that build correlation
are SOURCE-AGNOSTIC. This IS the interoperability story at its deepest: any Rack CV, run through Change
Alley's full correlation engine via the pins.

### Correction to the shared-Change-Alley section
The reroute there is: pins route WHICH source each voice reads. Sequencer-Monsoon: pins route among
internal draws. Quantiser-Monsoon: pins route among external CV channels. Same shared pins signal,
per-Monsoon source pool -- reinforces the "reroute at the Monsoon, not Change Alley" resolution (each
Monsoon supplies its own source pool per its mode; the shared pins matrix is interpreted against that
pool at the consumer). NOT "pin/fix the CV" (my wrong guess) -- it's source-routing/correlation.

Cross-ref: the shared-Change-Alley section (reroute = source-routing per-Monsoon), Change Alley
correlation engine (pins build correlations at voice level), the quantiser unification (swapped source
pool), the interoperability positioning (any Rack CV through the correlation engine via pins).

## Design Q: allow Colonnades/Duo to attach to MULTIPLE Monsoons? (Rodney) -- lean yes-as-option, verify topology

The dual of the shared-Change-Alley question, but the shared thing is a TUNING AUTHORITY (Colonnades/Duo
define scale/tuning: per-degree cents/weights/enabled-mask/tonic) that a Monsoon quantises/generates
against.

### Case FOR multi-attach
- Consistency: two Monsoons in the SAME tuning (an ensemble in one maqam) -> author ONCE, both follow.
  Without sharing, author the same 24-degree tuning twice + keep manually in sync (tedious, error-prone).
- Live tuning modulation affects ALL: modulate the shared Colonnades (bend a degree, shift tonic) ->
  all attached Monsoons re-tune together (coordinated ensemble microtonal shift).
- Matches the Change Alley precedent (one shared source, many consumers).

### Case AGAINST / complications
- Removes per-Monsoon tuning INDEPENDENCE: if shared, two Monsoons CAN'T be in different tunings -- but
  that's a real wanted case (the CROSS-TUNING CANON, literally one of our demo-patch ideas). Mandatory
  sharing would BREAK cross-tuning.
- Base/override arbitration multiplied: Colonnades authors base, Shophouse Micro overrides. Multiple
  Monsoons each with their own Shophouse Micro on one shared Colonnades = one base, N independent
  overrides. Probably fine (base shared, override per-Monsoon) but be deliberate.
- Shared tonic: all Monsoons share the tonic (usually wanted = same key, but removes independence).

### Lean: allow it as an OPTION, don't mandate (same principle as shared Change Alley)
- YES allow Colonnades/Duo to attach to multiple Monsoons -- for the coordinated-ensemble,
  author-once, modulate-together case.
- BUT keep per-Monsoon tuning possible -- a Monsoon can have its OWN Colonnades, or its own Shophouse
  Micro override on a shared base. Sharing is OPT-IN, not the only topology (preserves cross-tuning).
- Same shared-resource principle: Colonnades stays a mode-agnostic, READ-ONLY tuning SOURCE; each
  Monsoon reads it and can layer its OWN Shophouse Micro override. Shared base + per-Monsoon override =
  EXACTLY the base/override arbitration already built, base now shared across consumers. Multiple
  Monsoons on one Colonnades = one shared base, N private overrides -- preserves independence (via each
  override) while allowing sharing (the common base).

### VERIFY FIRST (codebase risk-shape + VCV topology)
Partial code read: Monsoon READS Colonnades as a "tuning source" (Monsoon.cpp:104
cachedColonnadesExpander, :114 tuningSourceExpander_) -- conceptual direction is right (Monsoon reads,
Colonnades is source). BUT unconfirmed:
1. Does Colonnades hold a single-Monsoon BACK-REFERENCE / write back? If it writes back to one Monsoon,
   multi-attach = N Monsoons fighting one write-back path. Clean design = Colonnades is a PURE read
   source (never writes to a specific Monsoon). Verify.
2. *** The TOPOLOGY question (the real blocker): *** VCV expanders are typically PHYSICALLY ADJACENT
   (left/right neighbour). If Colonnades attaches by adjacency, it sits next to ONE Monsoon -- "attach to
   multiple" isn't a permission question, it's a topology impossibility (an expander is next to one
   module). Multi-attach would need a NON-adjacency reference model (a tuning-source that isn't the
   physical neighbour) -- a bigger change. CONFIRM whether tuning-source is adjacency-bound or can be a
   non-adjacent reference BEFORE deciding this is even feasible.
So: lean yes-as-option IF the topology allows non-adjacent/shared tuning-source; if it's strictly
physical-adjacency, multi-attach needs a reference-model change first. Verify the expander adjacency
model post-holiday.

Cross-ref: the shared-Change-Alley resolution (shared source + per-consumer override), Colonnades/
Shophouse-Micro base/override arbitration (extends to shared base + N overrides), the cross-tuning canon
demo (why NOT to mandate sharing), Monsoon.cpp:104/114 (tuning-source read path -- verify adjacency).
